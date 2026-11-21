# lwimg

Lightweight image conversion helpers for embedded C99 targets: YUV422 to RGB565 conversion, 90-degree-step rotation and integer-factor upscaling. No OS dependency, no HAL, no dynamic allocation and no writable static data.

The application owns every buffer and describes each image with a `lwimg_img_t`: data pointer, byte capacity, pixel format, visible width and height, and row stride in pixels. The library validates the descriptor, converts in place or into a separate buffer, and never allocates or caches anything.

## Scope

Supported:

- YUV422 to RGB565 for UYVY, YUYV, VYUY and YVYU byte orders, two pixels per 32-bit word.
- In-place conversion when source and destination share the same buffer, dimensions and stride.
- Rotation by 0, 90, 180 and 270 degrees for RGB565 images.
- Upscaling by 2x or 2.5x for RGB565 images, with nearest-neighbour and bilinear methods.
- Arbitrary row strides in pixels for padded rows and destinations inside a larger framebuffer.
- Strict descriptor validation: null pointers, unknown formats, zero dimensions, short strides, undersized buffers, misalignment and odd strides or widths where the algorithm needs word access.

Not supplied, by design:

- RGB565 to YUV, RGB888, alpha, palette handling or colour management.
- Arbitrary rotation angles, arbitrary scale factors, downscaling or cropping.
- In-place rotation or scaling, and overlap detection for any operation. The caller guarantees disjoint buffers, except for the YUV422 to RGB565 in-place conversion.
- DMA, SIMD, cache maintenance, locking or interrupt-context safety.

## Quick start

Compile `src/lwimg.c` and add `src/` to the include path. No configuration header or port functions are required.

```c
#include "lwimg.h"

lwimg_status_t convert_uyvy(void *yuv, uint32_t yuv_size, void *rgb, uint32_t rgb_size)
{
    lwimg_img_t src = {
        .data = yuv,
        .data_size = yuv_size,
        .pixel_format = LWIMG_PIXEL_FORMAT_YUV422_UYVY,
        .width = 240U,
        .height = 240U,
        .stride_pixels = 240U,
    };
    lwimg_img_t dst = {
        .data = rgb,
        .data_size = rgb_size,
        .pixel_format = LWIMG_PIXEL_FORMAT_RGB565,
        .width = 240U,
        .height = 240U,
        .stride_pixels = 240U,
    };

    return lwimg_yuv422_to_rgb565(&src, &dst);
}
```

For rotation and scaling, query the destination dimensions first:

```c
uint16_t width;
uint16_t height;

if (lwimg_get_rotate_output_size(240U, 120U, LWIMG_ROTATION_90, &width, &height) != LWIMG_OK) {
    /* rejected rotation */
}
```

`width` is 120 and `height` is 240 for 90 and 270 degrees; the dimensions do not change for 0 and 180. See [doc/api.md](doc/api.md) for the complete contract.

## Image descriptor

| Field | Meaning |
| --- | --- |
| `data` | First byte of the image. Sources may be read-only storage cast to `void *`; destinations must be writable. |
| `data_size` | Bytes accessible at `data`. At least `(stride_pixels * (height - 1) + width) * 2`. |
| `pixel_format` | A `lwimg_pixel_format_t` enumerator. `LWIMG_PIXEL_FORMAT_COUNT` and larger values are rejected. |
| `width`, `height` | Visible size in pixels; both must be nonzero. |
| `stride_pixels` | Pixel distance between consecutive row starts; at least `width`. |

Rows may be padded. Padding beyond the visible width of a row is never read or written unless it belongs to a later row's visible pixels. Bytes after the last visible pixel of the last row are not touched, so `data_size` is the minimal footprint, not `stride_pixels * height * 2`.

## Operation requirements

| Operation | Data alignment | Stride | Dimensions | Other |
| --- | --- | --- | --- | --- |
| YUV422 to RGB565 | 4 bytes, source and destination | even | destination equals source | source width even; source format is one of the four YUV422 orders; destination format RGB565 |
| Rotate | 2 bytes, source and destination | at least `width` | from the size query | RGB565 only; destination format equals source |
| Scale | 4 bytes, source and destination | even | from the size query | RGB565 only; 2.5x requires source width divisible by 4 and height divisible by 2 |

The base check also requires the format to be known, both dimensions nonzero, `stride_pixels >= width`, a footprint that fits in 32 bits and `data_size` large enough for that footprint.

## API

| Function | Behaviour |
| --- | --- |
| `lwimg_get_yuv422_to_rgb565_output_size(src_width, src_height, out_width, out_height)` | Return the source dimensions unchanged; either output pointer may be NULL. Always succeeds. |
| `lwimg_yuv422_to_rgb565(src, dst)` | Convert one YUV422 image to RGB565. Source and destination may share storage when their dimensions, stride and layout match. |
| `lwimg_get_rotate_output_size(src_width, src_height, rotation, out_width, out_height)` | Return the rotated dimensions; 90 and 270 swap width and height. An unknown rotation fails. |
| `lwimg_rotate(src, rotation, dst)` | Copy for 0 degrees, rotate clockwise for 90, rotate 180 or rotate 270. |
| `lwimg_get_scale_output_size(src_width, src_height, scale_factor, out_width, out_height)` | Return the scaled dimensions. A result above 65535 fails instead of wrapping. |
| `lwimg_scale(src, scale_factor, scale_method, dst)` | Upscale RGB565 with nearest-neighbour or bilinear sampling. |

All functions return `LWIMG_OK` or `LWIMG_ERROR`. A failed call writes no output parameter and no destination pixel.

## Conversion details

### YUV422 byte orders

| Format | Byte 0 | Byte 1 | Byte 2 | Byte 3 |
| --- | --- | --- | --- | --- |
| `LWIMG_PIXEL_FORMAT_YUV422_UYVY` | U | Y0 | V | Y1 |
| `LWIMG_PIXEL_FORMAT_YUV422_YUYV` | Y0 | U | Y1 | V |
| `LWIMG_PIXEL_FORMAT_YUV422_VYUY` | V | Y0 | U | Y1 |
| `LWIMG_PIXEL_FORMAT_YUV422_YVYU` | Y0 | V | Y1 | U |

Two adjacent pixels share one U and one V sample. The RGB565 result uses clamped integer BT.601 approximations:

```text
R = Y + (179 * (V - 128)) >> 7
G = Y - ((44 * (U - 128) + 91 * (V - 128)) >> 7)
B = Y + (226 * (U - 128)) >> 7
```

Each channel is clamped to 0-255 and then quantised to 5, 6 and 5 bits.

### Rotation

90 degrees is clockwise. For a source of width W and height H, the destination pixel at (x, y) comes from:

| Rotation | Destination size | Source pixel |
| --- | --- | --- |
| 0 | W x H | (x, y) |
| 90 | H x W | (y, H - 1 - x) |
| 180 | W x H | (W - 1 - x, H - 1 - y) |
| 270 | H x W | (W - 1 - y, x) |

### Scaling

2x nearest repeats every source pixel in a 2x2 block. 2x bilinear keeps the source pixel and averages horizontally, vertically and diagonally, replicating the last row and column at the right and bottom edges.

2.5x works on 4x2 source blocks and produces 10x5 destination blocks. Nearest-neighbour uses the pixel-centre mapping `0, 0, 1, 1, 1, 2, 2, 3, 3, 3` horizontally and `0, 0, 1, 1, 1` vertically. Bilinear interpolates the block corners and edges inside the block; it is not a global-grid resampler, so a small discontinuity remains between adjacent blocks. Both methods need a source width divisible by 4 and a source height divisible by 2.

RGB565 averaging uses the standard SWAR average, so results may differ by one least-significant bit from a per-channel rounded average.

## Concurrency

The library has no global state, no lock and no interrupt-safety contract. Independent calls on independent buffers can run concurrently; calls that touch the same buffer must be serialised by the caller.

## Verification

From the repository root, with a C99 compiler available:

```sh
sh build.sh
```

The script compiles the library and the example separately with `-std=c99 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wundef -Wwrite-strings -Werror` in a temporary directory, then removes the temporary files. It does not link or execute them. `CC` selects the compiler and `CFLAGS` adds flags, for example:

```sh
CC=armclang CFLAGS="--target=arm-arm-none-eabi -mcpu=cortex-m4 -mthumb" sh build.sh
```

[examples/lwimg_demo.c](examples/lwimg_demo.c) is a self-checking host demo. On a host with a native C compiler:

```sh
mkdir -p build
cc -std=c99 -O2 -Wall -Wextra -Isrc \
    src/lwimg.c examples/lwimg_demo.c -o build/lwimg_demo
./build/lwimg_demo
```

The demo returns zero on success and covers all four YUV422 orders, in-place conversion, all rotations, both scale factors and methods, padded source and destination rows, the output-size overflow guard and rejected descriptors. No hardware is involved.

## Layout

```text
src/        lwimg.c, lwimg.h
examples/   lwimg_demo.c
doc/        api.md
build.sh    strict-warning compile check, no hardware involved
```

The standalone packaging starts at 0.1.0; release history is in [CHANGELOG.md](CHANGELOG.md).

## License

BSD 3-Clause, see [LICENSE](LICENSE).
