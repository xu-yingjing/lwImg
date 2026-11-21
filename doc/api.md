# API contract

The declarations live in `src/lwimg.h`. No function allocates, no function keeps writable static data, and no function depends on an OS, a HAL or a device header. Every function returns `LWIMG_OK` or `LWIMG_ERROR`; there is no partial success and no other status value.

A call that returns `LWIMG_ERROR` has written no output parameter and no destination pixel. Validation runs before the first read or write of the image buffers. Treat the contents of a rejected call as unchanged.

## Types

| Type | Values |
| --- | --- |
| `lwimg_status_t` | `LWIMG_OK`, `LWIMG_ERROR`. |
| `lwimg_pixel_format_t` | `LWIMG_PIXEL_FORMAT_RGB565`, `LWIMG_PIXEL_FORMAT_YUV422_UYVY`, `LWIMG_PIXEL_FORMAT_YUV422_YUYV`, `LWIMG_PIXEL_FORMAT_YUV422_VYUY`, `LWIMG_PIXEL_FORMAT_YUV422_YVYU`; `LWIMG_PIXEL_FORMAT_COUNT` and larger values are rejected. |
| `lwimg_rotation_t` | `LWIMG_ROTATION_0`, `LWIMG_ROTATION_90`, `LWIMG_ROTATION_180`, `LWIMG_ROTATION_270`; `LWIMG_ROTATION_COUNT` and larger values are rejected. |
| `lwimg_scale_factor_t` | `LWIMG_SCALE_FACTOR_2X`, `LWIMG_SCALE_FACTOR_2_5X`; `LWIMG_SCALE_FACTOR_COUNT` and larger values are rejected. |
| `lwimg_scale_method_t` | `LWIMG_SCALE_METHOD_NEAREST`, `LWIMG_SCALE_METHOD_BILINEAR`; `LWIMG_SCALE_METHOD_COUNT` and larger values are rejected. |

Enum arguments are range-checked through `(uint32_t)` casts, so a negative enumerator cannot pass a comparison against a count.

## Image descriptor

```c
typedef struct {
    void *data;
    uint32_t data_size;
    lwimg_pixel_format_t pixel_format;
    uint16_t width;
    uint16_t height;
    uint32_t stride_pixels;
} lwimg_img_t;
```

| Field | Contract |
| --- | --- |
| `data` | First byte of the image. Must be writable for every destination and readable for every source. Sources may be read-only storage cast to `void *`. |
| `data_size` | Bytes accessible at `data`. Must be at least `(stride_pixels * (height - 1) + width) * 2`. |
| `pixel_format` | One of the enumerators above. Every supported format uses two bytes per pixel. |
| `width` | Visible pixels per row; nonzero. |
| `height` | Visible rows; nonzero. |
| `stride_pixels` | Pixels between consecutive row starts, not bytes. Must be at least `width`; larger values leave padding at the end of each row. |

Every function validates the descriptor before touching the buffers:

1. All pointers are non-null and the format enumerator is below its count.
2. `width` and `height` are nonzero and `stride_pixels >= width`.
3. `data` is aligned to two bytes.
4. The minimal footprint `stride_pixels * (height - 1) + width` fits in `uint32_t`, including the multiplication and the conversion to bytes.
5. `data_size` covers that footprint in bytes.

Padding bytes inside a row and bytes after the last visible pixel of the last row are never read or written. A destination buffer may therefore be a sub-rectangle of a larger framebuffer when `data_size` covers the minimal footprint at the chosen stride.

## YUV422 byte orders

| Format | Byte 0 | Byte 1 | Byte 2 | Byte 3 |
| --- | --- | --- | --- | --- |
| `LWIMG_PIXEL_FORMAT_YUV422_UYVY` | U | Y0 | V | Y1 |
| `LWIMG_PIXEL_FORMAT_YUV422_YUYV` | Y0 | U | Y1 | V |
| `LWIMG_PIXEL_FORMAT_YUV422_VYUY` | V | Y0 | U | Y1 |
| `LWIMG_PIXEL_FORMAT_YUV422_YVYU` | Y0 | V | Y1 | U |

The second pixel of a pair reuses the first pixel's U and V samples.

## lwimg_get_yuv422_to_rgb565_output_size

```c
lwimg_status_t lwimg_get_yuv422_to_rgb565_output_size(uint16_t src_width, uint16_t src_height,
                                                      uint16_t *out_width, uint16_t *out_height);
```

Copies the source dimensions to the output parameters, because the conversion preserves the visible size. Either output pointer may be NULL. The function always returns `LWIMG_OK` and performs no other validation; the conversion validates the descriptors and the parity rules.

## lwimg_yuv422_to_rgb565

```c
lwimg_status_t lwimg_yuv422_to_rgb565(const lwimg_img_t *src, lwimg_img_t *dst);
```

Converts one YUV422 image to RGB565. In addition to the common descriptor checks:

- Both formats must be known; `src` must be one of the four YUV422 orders and `dst` must be RGB565.
- `src->width` must be even, because pixels are processed in pairs.
- `src->stride_pixels` and `dst->stride_pixels` must be even.
- Both data pointers must be aligned to four bytes.
- `dst->width` and `dst->height` must equal the source dimensions.

The conversion reads one 32-bit word per pixel pair and writes one 32-bit word containing two RGB565 pixels. Coefficients are integer BT.601 approximations:

```text
dr = (179 * (V - 128)) >> 7
dg = (44 * (U - 128) + 91 * (V - 128)) >> 7
db = (226 * (U - 128)) >> 7
R = clamp(Y + dr, 0, 255)
G = clamp(Y - dg, 0, 255)
B = clamp(Y + db, 0, 255)
RGB565 = ((R & 0xF8) << 8) | ((G & 0xFC) << 3) | (B >> 3)
```

Source and destination may be the same buffer when `src->data == dst->data` and the dimensions, stride and buffer size match. Each pair is rewritten in place before the next pair is read, so no other overlap is supported.

## lwimg_get_rotate_output_size

```c
lwimg_status_t lwimg_get_rotate_output_size(uint16_t src_width, uint16_t src_height,
                                            lwimg_rotation_t rotation,
                                            uint16_t *out_width, uint16_t *out_height);
```

| Rotation | Output width | Output height |
| --- | --- | --- |
| `LWIMG_ROTATION_0`, `LWIMG_ROTATION_180` | `src_width` | `src_height` |
| `LWIMG_ROTATION_90`, `LWIMG_ROTATION_270` | `src_height` | `src_width` |

Either output pointer may be NULL. An unknown rotation returns `LWIMG_ERROR` without writing the outputs. The dimensions themselves are copied, not computed, so they cannot overflow `uint16_t`.

## lwimg_rotate

```c
lwimg_status_t lwimg_rotate(const lwimg_img_t *src, lwimg_rotation_t rotation, lwimg_img_t *dst);
```

Rotates an RGB565 image. In addition to the common descriptor checks:

- Both pixel formats must be known and equal; only RGB565 is implemented.
- `dst->width` and `dst->height` must match the size query for the requested rotation.

The destination pixel at (x, y) is taken from the source as follows. Width W and height H refer to the source.

| Rotation | Destination size | Source pixel |
| --- | --- | --- |
| 0 | W x H | (x, y) |
| 90 | H x W | (y, H - 1 - x) |
| 180 | W x H | (W - 1 - x, H - 1 - y) |
| 270 | H x W | (W - 1 - y, x) |

90 degrees is clockwise. Rotation 0 is a plain copy. Source and destination must be disjoint; the library does not detect overlap and an in-place rotation corrupts the image. The rotation path needs only the two-byte base alignment, not four-byte alignment.

## lwimg_get_scale_output_size

```c
lwimg_status_t lwimg_get_scale_output_size(uint16_t src_width, uint16_t src_height,
                                           lwimg_scale_factor_t scale_factor,
                                           uint16_t *out_width, uint16_t *out_height);
```

| Factor | Output width | Output height |
| --- | --- | --- |
| `LWIMG_SCALE_FACTOR_2X` | `src_width * 2` | `src_height * 2` |
| `LWIMG_SCALE_FACTOR_2_5X` | `src_width * 5 / 2` | `src_height * 5 / 2` |

The products are computed in `uint32_t`. If either result exceeds 65535, the function returns `LWIMG_ERROR` without writing the outputs; it never truncates. Either output pointer may be NULL. An unknown factor also fails.

The operation itself requires a source width divisible by 4 and height divisible by 2 for 2.5x, so the integer divisions in this query do not lose a pixel in an accepted call.

## lwimg_scale

```c
lwimg_status_t lwimg_scale(const lwimg_img_t *src, lwimg_scale_factor_t scale_factor,
                           lwimg_scale_method_t scale_method, lwimg_img_t *dst);
```

Upscales an RGB565 image. In addition to the common descriptor checks:

- Both pixel formats must be known and equal; only RGB565 is implemented.
- `scale_method` must be below `LWIMG_SCALE_METHOD_COUNT`.
- Both data pointers must be aligned to four bytes.
- `src->stride_pixels` and `dst->stride_pixels` must be even.
- `dst->width` and `dst->height` must match the size query, which also applies the 65535 limit.
- For 2.5x, `src->width` must be divisible by 4 and `src->height` by 2.

2x nearest repeats every source pixel into a 2x2 destination block. 2x bilinear writes the source pixel at even coordinates and averages horizontally, vertically and diagonally; the last source row and column are replicated, so the right and bottom edges of the destination repeat the edge pixels.

2.5x processes each 4x2 source block into one 10x5 destination block. Nearest-neighbour uses the pixel-centre mapping `0, 0, 1, 1, 1, 2, 2, 3, 3, 3` across the block and `0, 0, 1, 1, 1` down the block. Bilinear interpolates the block corners and edges inside the block; it is a block-wise approximation, not a global-grid resampler, and adjacent blocks can show a small step.

Averages use the SWAR RGB565 average, so a result may differ from a per-channel rounded average by one least-significant bit.

Source and destination must be disjoint; overlap is not detected. Padding inside a row and bytes after the last visible pixel are not touched.

## Failure summary

The following conditions return `LWIMG_ERROR` with no output or pixel written:

- Null image pointer or null source/destination data pointer.
- Unknown or out-of-range enumerator.
- Zero width or height.
- `stride_pixels < width` or an odd stride where the operation needs word access.
- Data pointer not aligned to two bytes, or to four bytes for conversion and scaling.
- A footprint that overflows 32 bits or a `data_size` smaller than the footprint.
- A destination size that does not match the size query.
- An odd source width for YUV422 conversion, or a source width not divisible by 4 or height not divisible by 2 for 2.5x scaling.
- A scale result above 65535.
