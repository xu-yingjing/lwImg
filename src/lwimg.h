#ifndef LWIMG_H
#define LWIMG_H

#include <stdint.h>

typedef enum {
    LWIMG_OK = 0U,
    LWIMG_ERROR,
} lwimg_status_t;

typedef enum {
    LWIMG_PIXEL_FORMAT_RGB565 = 0U,
    LWIMG_PIXEL_FORMAT_YUV422_UYVY,
    LWIMG_PIXEL_FORMAT_YUV422_YUYV,
    LWIMG_PIXEL_FORMAT_YUV422_VYUY,
    LWIMG_PIXEL_FORMAT_YUV422_YVYU,
    LWIMG_PIXEL_FORMAT_COUNT,
} lwimg_pixel_format_t;

typedef enum {
    LWIMG_ROTATION_0 = 0U,
    LWIMG_ROTATION_90,
    LWIMG_ROTATION_180,
    LWIMG_ROTATION_270,
    LWIMG_ROTATION_COUNT,
} lwimg_rotation_t;

typedef enum {
    LWIMG_SCALE_FACTOR_2X = 0U,
    LWIMG_SCALE_FACTOR_2_5X,
    LWIMG_SCALE_FACTOR_COUNT,
} lwimg_scale_factor_t;

typedef enum {
    LWIMG_SCALE_METHOD_NEAREST = 0U,
    LWIMG_SCALE_METHOD_BILINEAR,
    LWIMG_SCALE_METHOD_COUNT,
} lwimg_scale_method_t;

typedef struct {
    void *data;
    uint32_t data_size;
    lwimg_pixel_format_t pixel_format;
    uint16_t width;
    uint16_t height;
    uint32_t stride_pixels;
} lwimg_img_t;

lwimg_status_t lwimg_get_yuv422_to_rgb565_output_size(uint16_t src_width, uint16_t src_height, uint16_t *out_width, uint16_t *out_height);

lwimg_status_t lwimg_yuv422_to_rgb565(const lwimg_img_t *src, lwimg_img_t *dst);

lwimg_status_t lwimg_get_rotate_output_size(uint16_t src_width, uint16_t src_height, lwimg_rotation_t rotation, uint16_t *out_width, uint16_t *out_height);

lwimg_status_t lwimg_rotate(const lwimg_img_t *src, lwimg_rotation_t rotation, lwimg_img_t *dst);

lwimg_status_t lwimg_get_scale_output_size(uint16_t src_width, uint16_t src_height, lwimg_scale_factor_t scale_factor, uint16_t *out_width, uint16_t *out_height);

lwimg_status_t lwimg_scale(const lwimg_img_t *src, lwimg_scale_factor_t scale_factor, lwimg_scale_method_t scale_method, lwimg_img_t *dst);

#endif
