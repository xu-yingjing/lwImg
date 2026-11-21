# Changelog

## 0.1.0

Initial standalone packaging of the in-tree image conversion helpers.

- `lwimg_yuv422_to_rgb565()` converts UYVY, YUYV, VYUY and YVYU images to RGB565 two pixels at a time, using clamped integer BT.601 coefficients and in-place operation when source and destination share the same buffer, dimensions and stride.
- `lwimg_rotate()` copies or rotates RGB565 images by 0, 90, 180 or 270 degrees; `lwimg_get_rotate_output_size()` reports the swapped dimensions for 90 and 270.
- `lwimg_scale()` upscales RGB565 images by 2x or 2.5x with nearest-neighbour or bilinear sampling; `lwimg_get_scale_output_size()` rejects scaled dimensions above 65535 instead of truncating them to `uint16_t`.
- Image descriptors carry a caller-owned buffer, byte capacity, pixel format, visible size and row stride in pixels. Every call validates the descriptor, the alignment and stride requirements and the operation-specific constraints before touching the buffers.
- New for the standalone release: `README.md`, `LICENSE`, `doc/api.md`, `examples/lwimg_demo.c` and `build.sh`. The two files under `src/` are copied from the in-tree module.
- Known gaps, by design: RGB565-only rotation and scaling; fixed rotation angles and scale factors; no downscaling, cropping, overlap detection or in-place rotation/scaling; no DMA, SIMD or locking. The 2.5x bilinear path interpolates 4x2 blocks into 10x5 blocks instead of sampling a global grid, and requires a source width divisible by 4 and a source height divisible by 2.
