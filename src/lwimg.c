#include "lwimg.h"
#include <stddef.h>
#include <stdbool.h>

#define UTIL_IS_ALIGNED(value, alignment) (((value) & ((alignment) - 1U)) == 0U)
#define UTIL_CLAMP_U8(value)              ((uint8_t)(((value) < 0) ? 0 : (((value) > 255) ? 255 : (value))))
#define UTIL_AVG2_U16(a, b)               ((uint16_t)(((a) & (b)) + ((((a) ^ (b)) & 0xF7DEU) >> 1)))
#define UTIL_AVG4_U16(a, b, c, d)         UTIL_AVG2_U16(UTIL_AVG2_U16((a), (b)), UTIL_AVG2_U16((c), (d)))

static lwimg_status_t lwimg_check_img(const lwimg_img_t *img)
{
    uint8_t bytes_per_pixel;
    uint32_t pixel_count;

    if ((img == NULL) ||
        (img->data == NULL) ||
        ((uint32_t)img->pixel_format >= (uint32_t)LWIMG_PIXEL_FORMAT_COUNT) ||
        (img->width == 0U) ||
        (img->height == 0U) ||
        (img->stride_pixels < (uint32_t)img->width)) {
        return LWIMG_ERROR;
    }

    bytes_per_pixel = lwimg_get_bytes_per_pixel(img->pixel_format);
    if (bytes_per_pixel == 0U) {
        return LWIMG_ERROR;
    }

    if (UTIL_IS_ALIGNED((uintptr_t)img->data, bytes_per_pixel) == false) {
        return LWIMG_ERROR;
    }

    if ((img->height - 1U) > ((UINT32_MAX - img->width) / img->stride_pixels)) {
        return LWIMG_ERROR;
    }

    pixel_count = (img->stride_pixels * (img->height - 1U)) + img->width;
    if (pixel_count > (UINT32_MAX / bytes_per_pixel)) {
        return LWIMG_ERROR;
    }

    if (img->data_size < (pixel_count * bytes_per_pixel)) {
        return LWIMG_ERROR;
    }

    return LWIMG_OK;
}

static void lwimg_yuv422_generic_to_rgb565(const lwimg_img_t *src, lwimg_img_t *dst, uint8_t y0_index, uint8_t y1_index, uint8_t u_index, uint8_t v_index)
{
    typedef union {
        uint32_t u32;
        uint8_t b[4];
    } pixel_2x1_t;

    const uint16_t *src_pixels;
    uint16_t *dst_pixels;
    uint16_t row;
    uint16_t col;
    const uint32_t *src_row_u32;
    uint32_t *dst_row_u32;
    pixel_2x1_t src_pixel_2x1;

    src_pixels = (const uint16_t *)src->data;
    dst_pixels = (uint16_t *)dst->data;

    for (row = 0U; row < src->height; row++) {
        src_row_u32 = (const uint32_t *)(src_pixels + (row * src->stride_pixels));
        dst_row_u32 = (uint32_t *)(dst_pixels + (row * dst->stride_pixels));

        for (col = 0U; col < src->width; col += 2U) {
            src_pixel_2x1.u32 = src_row_u32[col >> 1];

            int16_t u128 = (int16_t)src_pixel_2x1.b[u_index] - 128;
            int16_t v128 = (int16_t)src_pixel_2x1.b[v_index] - 128;

            int16_t dr = (179 * v128) >> 7;
            int16_t dg = (44 * u128 + 91 * v128) >> 7;
            int16_t db = (226 * u128) >> 7;

            uint32_t rgb0 = ((uint32_t)(UTIL_CLAMP_U8((int32_t)src_pixel_2x1.b[y0_index] + dr) & 0xF8U) << 8) |
                            ((uint32_t)(UTIL_CLAMP_U8((int32_t)src_pixel_2x1.b[y0_index] - dg) & 0xFCU) << 3) |
                            ((uint32_t)UTIL_CLAMP_U8((int32_t)src_pixel_2x1.b[y0_index] + db) >> 3);
            uint32_t rgb1 = ((uint32_t)(UTIL_CLAMP_U8((int32_t)src_pixel_2x1.b[y1_index] + dr) & 0xF8U) << 8) |
                            ((uint32_t)(UTIL_CLAMP_U8((int32_t)src_pixel_2x1.b[y1_index] - dg) & 0xFCU) << 3) |
                            ((uint32_t)UTIL_CLAMP_U8((int32_t)src_pixel_2x1.b[y1_index] + db) >> 3);

            dst_row_u32[col >> 1U] = (rgb1 << 16) | rgb0;
        }
    }
}

static void lwimg_rotate_rgb565(const lwimg_img_t *src, lwimg_rotation_t rotation, lwimg_img_t *dst)
{
    const uint16_t *src_pixels;
    uint16_t *dst_pixels;
    uint16_t row;
    uint16_t col;
    const uint16_t *src_row_u16;
    uint16_t *dst_row_u16;

    src_pixels = (const uint16_t *)src->data;
    dst_pixels = (uint16_t *)dst->data;

    switch (rotation) {
        case LWIMG_ROTATION_0:
            for (row = 0U; row < src->height; row++) {
                src_row_u16 = src_pixels + (row * src->stride_pixels);
                dst_row_u16 = dst_pixels + (row * dst->stride_pixels);

                for (col = 0U; col < src->width; col++) {
                    dst_row_u16[col] = src_row_u16[col];
                }
            }
            break;

        case LWIMG_ROTATION_90:
            for (row = 0U; row < src->height; row++) {
                src_row_u16 = src_pixels + (row * src->stride_pixels);

                for (col = 0U; col < src->width; col++) {
                    dst_pixels[(col * dst->stride_pixels) + (src->height - 1U - row)] = src_row_u16[col];
                }
            }
            break;

        case LWIMG_ROTATION_180:
            for (row = 0U; row < src->height; row++) {
                src_row_u16 = src_pixels + (row * src->stride_pixels);
                dst_row_u16 = dst_pixels + ((src->height - 1U - row) * dst->stride_pixels);

                for (col = 0U; col < src->width; col++) {
                    dst_row_u16[src->width - 1U - col] = src_row_u16[col];
                }
            }
            break;

        case LWIMG_ROTATION_270:
            for (row = 0U; row < src->height; row++) {
                src_row_u16 = src_pixels + (row * src->stride_pixels);

                for (col = 0U; col < src->width; col++) {
                    dst_pixels[((src->width - 1U - col) * dst->stride_pixels) + row] = src_row_u16[col];
                }
            }
            break;

        default:
            return;
    }
}

static void lwimg_scale_rgb565_2x_nearest(const lwimg_img_t *src, lwimg_img_t *dst)
{
    const uint16_t *src_pixels;
    uint16_t *dst_pixels;
    uint16_t row;
    uint16_t col;
    const uint16_t *src_row;
    uint32_t *dst_row0_u32;
    uint32_t *dst_row1_u32;

    src_pixels = (const uint16_t *)src->data;
    dst_pixels = (uint16_t *)dst->data;

    for (row = 0U; row < src->height; row++) {
        src_row = src_pixels + (row * src->stride_pixels);
        dst_row0_u32 = (uint32_t *)(dst_pixels + ((row * 2U) * dst->stride_pixels));
        dst_row1_u32 = (uint32_t *)(dst_pixels + ((row * 2U + 1U) * dst->stride_pixels));

        for (col = 0U; col < src->width; col++) {
            dst_row0_u32[col] = ((uint32_t)src_row[col] << 16) | (uint32_t)src_row[col];
            dst_row1_u32[col] = ((uint32_t)src_row[col] << 16) | (uint32_t)src_row[col];
        }
    }
}

static void lwimg_scale_rgb565_2_bilinear(const lwimg_img_t *src, lwimg_img_t *dst)
{
    typedef struct {
        union {
            uint32_t u32;
            uint16_t u16[2];
        } row[2];
    } pixel_2x2_t;

    const uint16_t *src_pixels;
    uint16_t *dst_pixels;
    uint16_t row;
    uint16_t col;
    const uint16_t *src_row0;
    const uint16_t *src_row1;
    uint32_t *dst_row0_u32;
    uint32_t *dst_row1_u32;
    pixel_2x2_t src_pixel_2x2;
    pixel_2x2_t dst_pixel_2x2;

    src_pixels = (const uint16_t *)src->data;
    dst_pixels = (uint16_t *)dst->data;

    for (row = 0U; row < src->height; row++) {
        src_row0 = src_pixels + (row * src->stride_pixels);
        if ((row + 1U) == src->height) {
            src_row1 = src_row0;
        } else {
            src_row1 = src_pixels + ((row + 1U) * src->stride_pixels);
        }
        dst_row0_u32 = (uint32_t *)(dst_pixels + ((row * 2U) * dst->stride_pixels));
        dst_row1_u32 = (uint32_t *)(dst_pixels + ((row * 2U + 1U) * dst->stride_pixels));

        for (col = 0U; col < src->width; col++) {
            src_pixel_2x2.row[0].u16[0] = src_row0[col];
            src_pixel_2x2.row[1].u16[0] = src_row1[col];
            if ((col + 1U) == src->width) {
                src_pixel_2x2.row[0].u16[1] = src_row0[col];
                src_pixel_2x2.row[1].u16[1] = src_row1[col];
            } else {
                src_pixel_2x2.row[0].u16[1] = src_row0[col + 1U];
                src_pixel_2x2.row[1].u16[1] = src_row1[col + 1U];
            }

            dst_pixel_2x2.row[0].u16[0] = src_pixel_2x2.row[0].u16[0];
            dst_pixel_2x2.row[0].u16[1] = UTIL_AVG2_U16(src_pixel_2x2.row[0].u16[0], src_pixel_2x2.row[0].u16[1]);
            dst_pixel_2x2.row[1].u16[0] = UTIL_AVG2_U16(src_pixel_2x2.row[0].u16[0], src_pixel_2x2.row[1].u16[0]);
            dst_pixel_2x2.row[1].u16[1] = UTIL_AVG4_U16(src_pixel_2x2.row[0].u16[0], src_pixel_2x2.row[0].u16[1], src_pixel_2x2.row[1].u16[0], src_pixel_2x2.row[1].u16[1]);

            dst_row0_u32[col] = dst_pixel_2x2.row[0].u32;
            dst_row1_u32[col] = dst_pixel_2x2.row[1].u32;
        }
    }
}

static void lwimg_scale_rgb565_2_5x_nearest(const lwimg_img_t *src, lwimg_img_t *dst)
{
    const uint16_t *src_pixels;
    uint16_t *dst_pixels;
    uint16_t row;
    uint16_t col;
    const uint16_t *src_row0;
    const uint16_t *src_row1;
    uint32_t *dst_row0_u32;
    uint32_t *dst_row1_u32;
    uint32_t *dst_row2_u32;
    uint32_t *dst_row3_u32;
    uint32_t *dst_row4_u32;

    src_pixels = (const uint16_t *)src->data;
    dst_pixels = (uint16_t *)dst->data;

    for (row = 0U; row < src->height; row += 2U) {
        src_row0 = src_pixels + (row * src->stride_pixels);
        src_row1 = src_pixels + ((row + 1U) * src->stride_pixels);
        dst_row0_u32 = (uint32_t *)(dst_pixels + (((row >> 1) * 5U) * dst->stride_pixels));
        dst_row1_u32 = (uint32_t *)(dst_pixels + (((row >> 1) * 5U + 1U) * dst->stride_pixels));
        dst_row2_u32 = (uint32_t *)(dst_pixels + (((row >> 1) * 5U + 2U) * dst->stride_pixels));
        dst_row3_u32 = (uint32_t *)(dst_pixels + (((row >> 1) * 5U + 3U) * dst->stride_pixels));
        dst_row4_u32 = (uint32_t *)(dst_pixels + (((row >> 1) * 5U + 4U) * dst->stride_pixels));

        for (col = 0U; col < src->width; col += 4U) {
            dst_row0_u32[(col >> 2) * 5U] = ((uint32_t)src_row0[col] << 16) | (uint32_t)src_row0[col];
            dst_row0_u32[(col >> 2) * 5U + 1U] = ((uint32_t)src_row0[col + 1U] << 16) | (uint32_t)src_row0[col + 1U];
            dst_row0_u32[(col >> 2) * 5U + 2U] = ((uint32_t)src_row0[col + 2U] << 16) | (uint32_t)src_row0[col + 1U];
            dst_row0_u32[(col >> 2) * 5U + 3U] = ((uint32_t)src_row0[col + 3U] << 16) | (uint32_t)src_row0[col + 2U];
            dst_row0_u32[(col >> 2) * 5U + 4U] = ((uint32_t)src_row0[col + 3U] << 16) | (uint32_t)src_row0[col + 3U];
            dst_row1_u32[(col >> 2) * 5U] = ((uint32_t)src_row0[col] << 16) | (uint32_t)src_row0[col];
            dst_row1_u32[(col >> 2) * 5U + 1U] = ((uint32_t)src_row0[col + 1U] << 16) | (uint32_t)src_row0[col + 1U];
            dst_row1_u32[(col >> 2) * 5U + 2U] = ((uint32_t)src_row0[col + 2U] << 16) | (uint32_t)src_row0[col + 1U];
            dst_row1_u32[(col >> 2) * 5U + 3U] = ((uint32_t)src_row0[col + 3U] << 16) | (uint32_t)src_row0[col + 2U];
            dst_row1_u32[(col >> 2) * 5U + 4U] = ((uint32_t)src_row0[col + 3U] << 16) | (uint32_t)src_row0[col + 3U];
            dst_row2_u32[(col >> 2) * 5U] = ((uint32_t)src_row1[col] << 16) | (uint32_t)src_row1[col];
            dst_row2_u32[(col >> 2) * 5U + 1U] = ((uint32_t)src_row1[col + 1U] << 16) | (uint32_t)src_row1[col + 1U];
            dst_row2_u32[(col >> 2) * 5U + 2U] = ((uint32_t)src_row1[col + 2U] << 16) | (uint32_t)src_row1[col + 1U];
            dst_row2_u32[(col >> 2) * 5U + 3U] = ((uint32_t)src_row1[col + 3U] << 16) | (uint32_t)src_row1[col + 2U];
            dst_row2_u32[(col >> 2) * 5U + 4U] = ((uint32_t)src_row1[col + 3U] << 16) | (uint32_t)src_row1[col + 3U];
            dst_row3_u32[(col >> 2) * 5U] = ((uint32_t)src_row1[col] << 16) | (uint32_t)src_row1[col];
            dst_row3_u32[(col >> 2) * 5U + 1U] = ((uint32_t)src_row1[col + 1U] << 16) | (uint32_t)src_row1[col + 1U];
            dst_row3_u32[(col >> 2) * 5U + 2U] = ((uint32_t)src_row1[col + 2U] << 16) | (uint32_t)src_row1[col + 1U];
            dst_row3_u32[(col >> 2) * 5U + 3U] = ((uint32_t)src_row1[col + 3U] << 16) | (uint32_t)src_row1[col + 2U];
            dst_row3_u32[(col >> 2) * 5U + 4U] = ((uint32_t)src_row1[col + 3U] << 16) | (uint32_t)src_row1[col + 3U];
            dst_row4_u32[(col >> 2) * 5U] = ((uint32_t)src_row1[col] << 16) | (uint32_t)src_row1[col];
            dst_row4_u32[(col >> 2) * 5U + 1U] = ((uint32_t)src_row1[col + 1U] << 16) | (uint32_t)src_row1[col + 1U];
            dst_row4_u32[(col >> 2) * 5U + 2U] = ((uint32_t)src_row1[col + 2U] << 16) | (uint32_t)src_row1[col + 1U];
            dst_row4_u32[(col >> 2) * 5U + 3U] = ((uint32_t)src_row1[col + 3U] << 16) | (uint32_t)src_row1[col + 2U];
            dst_row4_u32[(col >> 2) * 5U + 4U] = ((uint32_t)src_row1[col + 3U] << 16) | (uint32_t)src_row1[col + 3U];
        }
    }
}

static void lwimg_scale_rgb565_2_5x_bilinear(const lwimg_img_t *src, lwimg_img_t *dst)
{
    typedef struct {
        union {
            uint16_t u16[4];
            uint32_t u32[2];
        } row[2];
    } pixel_4x2_t;

    typedef struct {
        union {
            uint16_t u16[10];
            uint32_t u32[5];
        } row[5];
    } pixel_10x5_t;

    const uint16_t *src_pixels;
    uint16_t *dst_pixels;
    uint16_t row;
    uint16_t col;
    const uint32_t *src_row0_u32;
    const uint32_t *src_row1_u32;
    uint32_t *dst_row0_u32;
    uint32_t *dst_row1_u32;
    uint32_t *dst_row2_u32;
    uint32_t *dst_row3_u32;
    uint32_t *dst_row4_u32;
    pixel_4x2_t src_pixel_4x2;
    pixel_10x5_t dst_pixel_10x5;

    src_pixels = (const uint16_t *)src->data;
    dst_pixels = (uint16_t *)dst->data;

    for (row = 0U; row < src->height; row += 2U) {
        src_row0_u32 = (uint32_t *)(src_pixels + (row * src->stride_pixels));
        src_row1_u32 = (uint32_t *)(src_pixels + ((row + 1U) * src->stride_pixels));
        dst_row0_u32 = (uint32_t *)(dst_pixels + (((row >> 1) * 5U) * dst->stride_pixels));
        dst_row1_u32 = (uint32_t *)(dst_pixels + (((row >> 1) * 5U + 1U) * dst->stride_pixels));
        dst_row2_u32 = (uint32_t *)(dst_pixels + (((row >> 1) * 5U + 2U) * dst->stride_pixels));
        dst_row3_u32 = (uint32_t *)(dst_pixels + (((row >> 1) * 5U + 3U) * dst->stride_pixels));
        dst_row4_u32 = (uint32_t *)(dst_pixels + (((row >> 1) * 5U + 4U) * dst->stride_pixels));

        for (col = 0U; col < src->width; col += 4U) {
            src_pixel_4x2.row[0].u32[0] = src_row0_u32[0];
            src_pixel_4x2.row[0].u32[1] = src_row0_u32[1];
            src_pixel_4x2.row[1].u32[0] = src_row1_u32[0];
            src_pixel_4x2.row[1].u32[1] = src_row1_u32[1];

            dst_pixel_10x5.row[0].u16[0] = src_pixel_4x2.row[0].u16[0];
            dst_pixel_10x5.row[0].u16[4] = src_pixel_4x2.row[0].u16[1];
            dst_pixel_10x5.row[4].u16[0] = src_pixel_4x2.row[1].u16[0];
            dst_pixel_10x5.row[4].u16[4] = src_pixel_4x2.row[1].u16[1];
            dst_pixel_10x5.row[0].u16[5] = src_pixel_4x2.row[0].u16[2];
            dst_pixel_10x5.row[0].u16[9] = src_pixel_4x2.row[0].u16[3];
            dst_pixel_10x5.row[4].u16[5] = src_pixel_4x2.row[1].u16[2];
            dst_pixel_10x5.row[4].u16[9] = src_pixel_4x2.row[1].u16[3];

            dst_pixel_10x5.row[0].u16[2] = UTIL_AVG2_U16(dst_pixel_10x5.row[0].u16[0], dst_pixel_10x5.row[0].u16[4]);
            dst_pixel_10x5.row[2].u16[0] = UTIL_AVG2_U16(dst_pixel_10x5.row[0].u16[0], dst_pixel_10x5.row[4].u16[0]);
            dst_pixel_10x5.row[2].u16[4] = UTIL_AVG2_U16(dst_pixel_10x5.row[0].u16[4], dst_pixel_10x5.row[4].u16[4]);
            dst_pixel_10x5.row[4].u16[2] = UTIL_AVG2_U16(dst_pixel_10x5.row[4].u16[0], dst_pixel_10x5.row[4].u16[4]);
            dst_pixel_10x5.row[0].u16[7] = UTIL_AVG2_U16(dst_pixel_10x5.row[0].u16[5], dst_pixel_10x5.row[0].u16[9]);
            dst_pixel_10x5.row[2].u16[5] = UTIL_AVG2_U16(dst_pixel_10x5.row[0].u16[5], dst_pixel_10x5.row[4].u16[5]);
            dst_pixel_10x5.row[2].u16[9] = UTIL_AVG2_U16(dst_pixel_10x5.row[0].u16[9], dst_pixel_10x5.row[4].u16[9]);
            dst_pixel_10x5.row[4].u16[7] = UTIL_AVG2_U16(dst_pixel_10x5.row[4].u16[5], dst_pixel_10x5.row[4].u16[9]);

            dst_pixel_10x5.row[0].u16[1] = UTIL_AVG2_U16(dst_pixel_10x5.row[0].u16[0], dst_pixel_10x5.row[0].u16[2]);
            dst_pixel_10x5.row[0].u16[3] = UTIL_AVG2_U16(dst_pixel_10x5.row[0].u16[2], dst_pixel_10x5.row[0].u16[4]);
            dst_pixel_10x5.row[1].u16[0] = UTIL_AVG2_U16(dst_pixel_10x5.row[0].u16[0], dst_pixel_10x5.row[2].u16[0]);
            dst_pixel_10x5.row[1].u16[4] = UTIL_AVG2_U16(dst_pixel_10x5.row[0].u16[4], dst_pixel_10x5.row[2].u16[4]);
            dst_pixel_10x5.row[2].u16[2] = UTIL_AVG2_U16(dst_pixel_10x5.row[0].u16[2], dst_pixel_10x5.row[4].u16[2]);
            dst_pixel_10x5.row[3].u16[0] = UTIL_AVG2_U16(dst_pixel_10x5.row[2].u16[0], dst_pixel_10x5.row[4].u16[0]);
            dst_pixel_10x5.row[3].u16[4] = UTIL_AVG2_U16(dst_pixel_10x5.row[2].u16[4], dst_pixel_10x5.row[4].u16[4]);
            dst_pixel_10x5.row[4].u16[1] = UTIL_AVG2_U16(dst_pixel_10x5.row[4].u16[0], dst_pixel_10x5.row[4].u16[2]);
            dst_pixel_10x5.row[4].u16[3] = UTIL_AVG2_U16(dst_pixel_10x5.row[4].u16[2], dst_pixel_10x5.row[4].u16[4]);
            dst_pixel_10x5.row[0].u16[6] = UTIL_AVG2_U16(dst_pixel_10x5.row[0].u16[5], dst_pixel_10x5.row[0].u16[7]);
            dst_pixel_10x5.row[0].u16[8] = UTIL_AVG2_U16(dst_pixel_10x5.row[0].u16[7], dst_pixel_10x5.row[0].u16[9]);
            dst_pixel_10x5.row[1].u16[5] = UTIL_AVG2_U16(dst_pixel_10x5.row[0].u16[5], dst_pixel_10x5.row[2].u16[5]);
            dst_pixel_10x5.row[1].u16[9] = UTIL_AVG2_U16(dst_pixel_10x5.row[0].u16[9], dst_pixel_10x5.row[2].u16[9]);
            dst_pixel_10x5.row[2].u16[7] = UTIL_AVG2_U16(dst_pixel_10x5.row[0].u16[7], dst_pixel_10x5.row[4].u16[7]);
            dst_pixel_10x5.row[3].u16[5] = UTIL_AVG2_U16(dst_pixel_10x5.row[2].u16[5], dst_pixel_10x5.row[4].u16[5]);
            dst_pixel_10x5.row[3].u16[9] = UTIL_AVG2_U16(dst_pixel_10x5.row[2].u16[9], dst_pixel_10x5.row[4].u16[9]);
            dst_pixel_10x5.row[4].u16[6] = UTIL_AVG2_U16(dst_pixel_10x5.row[4].u16[5], dst_pixel_10x5.row[4].u16[7]);
            dst_pixel_10x5.row[4].u16[8] = UTIL_AVG2_U16(dst_pixel_10x5.row[4].u16[7], dst_pixel_10x5.row[4].u16[9]);

            dst_pixel_10x5.row[1].u16[2] = UTIL_AVG2_U16(dst_pixel_10x5.row[0].u16[2], dst_pixel_10x5.row[2].u16[2]);
            dst_pixel_10x5.row[2].u16[1] = UTIL_AVG2_U16(dst_pixel_10x5.row[2].u16[0], dst_pixel_10x5.row[2].u16[2]);
            dst_pixel_10x5.row[2].u16[3] = UTIL_AVG2_U16(dst_pixel_10x5.row[2].u16[2], dst_pixel_10x5.row[2].u16[4]);
            dst_pixel_10x5.row[3].u16[2] = UTIL_AVG2_U16(dst_pixel_10x5.row[2].u16[2], dst_pixel_10x5.row[4].u16[2]);
            dst_pixel_10x5.row[1].u16[7] = UTIL_AVG2_U16(dst_pixel_10x5.row[0].u16[7], dst_pixel_10x5.row[2].u16[7]);
            dst_pixel_10x5.row[2].u16[6] = UTIL_AVG2_U16(dst_pixel_10x5.row[2].u16[5], dst_pixel_10x5.row[2].u16[7]);
            dst_pixel_10x5.row[2].u16[8] = UTIL_AVG2_U16(dst_pixel_10x5.row[2].u16[7], dst_pixel_10x5.row[2].u16[9]);
            dst_pixel_10x5.row[3].u16[7] = UTIL_AVG2_U16(dst_pixel_10x5.row[2].u16[7], dst_pixel_10x5.row[4].u16[7]);

            dst_pixel_10x5.row[1].u16[1] = UTIL_AVG2_U16(dst_pixel_10x5.row[0].u16[1], dst_pixel_10x5.row[2].u16[1]);
            dst_pixel_10x5.row[1].u16[3] = UTIL_AVG2_U16(dst_pixel_10x5.row[0].u16[3], dst_pixel_10x5.row[2].u16[3]);
            dst_pixel_10x5.row[3].u16[1] = UTIL_AVG2_U16(dst_pixel_10x5.row[2].u16[1], dst_pixel_10x5.row[4].u16[1]);
            dst_pixel_10x5.row[3].u16[3] = UTIL_AVG2_U16(dst_pixel_10x5.row[2].u16[3], dst_pixel_10x5.row[4].u16[3]);
            dst_pixel_10x5.row[1].u16[6] = UTIL_AVG2_U16(dst_pixel_10x5.row[0].u16[6], dst_pixel_10x5.row[2].u16[6]);
            dst_pixel_10x5.row[1].u16[8] = UTIL_AVG2_U16(dst_pixel_10x5.row[0].u16[8], dst_pixel_10x5.row[2].u16[8]);
            dst_pixel_10x5.row[3].u16[6] = UTIL_AVG2_U16(dst_pixel_10x5.row[2].u16[6], dst_pixel_10x5.row[4].u16[6]);
            dst_pixel_10x5.row[3].u16[8] = UTIL_AVG2_U16(dst_pixel_10x5.row[2].u16[8], dst_pixel_10x5.row[4].u16[8]);

            dst_row0_u32[0] = dst_pixel_10x5.row[0].u32[0];
            dst_row0_u32[1] = dst_pixel_10x5.row[0].u32[1];
            dst_row0_u32[2] = dst_pixel_10x5.row[0].u32[2];
            dst_row0_u32[3] = dst_pixel_10x5.row[0].u32[3];
            dst_row0_u32[4] = dst_pixel_10x5.row[0].u32[4];
            dst_row1_u32[0] = dst_pixel_10x5.row[1].u32[0];
            dst_row1_u32[1] = dst_pixel_10x5.row[1].u32[1];
            dst_row1_u32[2] = dst_pixel_10x5.row[1].u32[2];
            dst_row1_u32[3] = dst_pixel_10x5.row[1].u32[3];
            dst_row1_u32[4] = dst_pixel_10x5.row[1].u32[4];
            dst_row2_u32[0] = dst_pixel_10x5.row[2].u32[0];
            dst_row2_u32[1] = dst_pixel_10x5.row[2].u32[1];
            dst_row2_u32[2] = dst_pixel_10x5.row[2].u32[2];
            dst_row2_u32[3] = dst_pixel_10x5.row[2].u32[3];
            dst_row2_u32[4] = dst_pixel_10x5.row[2].u32[4];
            dst_row3_u32[0] = dst_pixel_10x5.row[3].u32[0];
            dst_row3_u32[1] = dst_pixel_10x5.row[3].u32[1];
            dst_row3_u32[2] = dst_pixel_10x5.row[3].u32[2];
            dst_row3_u32[3] = dst_pixel_10x5.row[3].u32[3];
            dst_row3_u32[4] = dst_pixel_10x5.row[3].u32[4];
            dst_row4_u32[0] = dst_pixel_10x5.row[4].u32[0];
            dst_row4_u32[1] = dst_pixel_10x5.row[4].u32[1];
            dst_row4_u32[2] = dst_pixel_10x5.row[4].u32[2];
            dst_row4_u32[3] = dst_pixel_10x5.row[4].u32[3];
            dst_row4_u32[4] = dst_pixel_10x5.row[4].u32[4];

            src_row0_u32 += 2U;
            src_row1_u32 += 2U;
            dst_row0_u32 += 5U;
            dst_row1_u32 += 5U;
            dst_row2_u32 += 5U;
            dst_row3_u32 += 5U;
            dst_row4_u32 += 5U;
        }
    }
}

uint8_t lwimg_get_bytes_per_pixel(lwimg_pixel_format_t pixel_format)
{
    switch (pixel_format) {
        case LWIMG_PIXEL_FORMAT_RGB565:
        case LWIMG_PIXEL_FORMAT_YUV422_UYVY:
        case LWIMG_PIXEL_FORMAT_YUV422_YUYV:
        case LWIMG_PIXEL_FORMAT_YUV422_VYUY:
        case LWIMG_PIXEL_FORMAT_YUV422_YVYU:
            return 2U;

        default:
            return 0U;
    }
}

lwimg_status_t lwimg_get_yuv422_to_rgb565_output_size(uint16_t src_width, uint16_t src_height, uint16_t *out_width, uint16_t *out_height)
{
    if (out_width != NULL) {
        *out_width = src_width;
    }

    if (out_height != NULL) {
        *out_height = src_height;
    }

    return LWIMG_OK;
}

lwimg_status_t lwimg_yuv422_to_rgb565(const lwimg_img_t *src, lwimg_img_t *dst)
{
    uint16_t out_width;
    uint16_t out_height;

    if ((lwimg_check_img(src) != LWIMG_OK) ||
        (lwimg_check_img(dst) != LWIMG_OK)) {
        return LWIMG_ERROR;
    }

    if ((UTIL_IS_ALIGNED((uintptr_t)src->data, sizeof(uint32_t)) == false) ||
        ((src->width & 1U) != 0U) ||
        ((src->stride_pixels & 1U) != 0U) ||
        (UTIL_IS_ALIGNED((uintptr_t)dst->data, sizeof(uint32_t)) == false) ||
        (dst->pixel_format != LWIMG_PIXEL_FORMAT_RGB565) ||
        (lwimg_get_yuv422_to_rgb565_output_size(src->width, src->height, &out_width, &out_height) != LWIMG_OK) ||
        (dst->width != out_width) ||
        (dst->height != out_height) ||
        ((dst->stride_pixels & 1U) != 0U)) {
        return LWIMG_ERROR;
    }

    switch (src->pixel_format) {
        case LWIMG_PIXEL_FORMAT_YUV422_UYVY: lwimg_yuv422_generic_to_rgb565(src, dst, 1U, 3U, 0U, 2U); break;
        case LWIMG_PIXEL_FORMAT_YUV422_YUYV: lwimg_yuv422_generic_to_rgb565(src, dst, 0U, 2U, 1U, 3U); break;
        case LWIMG_PIXEL_FORMAT_YUV422_VYUY: lwimg_yuv422_generic_to_rgb565(src, dst, 1U, 3U, 2U, 0U); break;
        case LWIMG_PIXEL_FORMAT_YUV422_YVYU: lwimg_yuv422_generic_to_rgb565(src, dst, 0U, 2U, 3U, 1U); break;
        default: return LWIMG_ERROR;
    }

    return LWIMG_OK;
}

lwimg_status_t lwimg_get_rotate_output_size(uint16_t src_width, uint16_t src_height, lwimg_rotation_t rotation, uint16_t *out_width, uint16_t *out_height)
{
    switch (rotation) {
        case LWIMG_ROTATION_0:
        case LWIMG_ROTATION_180:
            if (out_width != NULL) {
                *out_width = src_width;
            }

            if (out_height != NULL) {
                *out_height = src_height;
            }

            break;

        case LWIMG_ROTATION_90:
        case LWIMG_ROTATION_270:
            if (out_width != NULL) {
                *out_width = src_height;
            }

            if (out_height != NULL) {
                *out_height = src_width;
            }

            break;

        default:
            return LWIMG_ERROR;
    }

    return LWIMG_OK;
}

lwimg_status_t lwimg_rotate(const lwimg_img_t *src, lwimg_rotation_t rotation, lwimg_img_t *dst)
{
    uint16_t out_width;
    uint16_t out_height;

    if ((lwimg_check_img(src) != LWIMG_OK) ||
        (lwimg_check_img(dst) != LWIMG_OK)) {
        return LWIMG_ERROR;
    }

    if ((dst->pixel_format != src->pixel_format) ||
        (lwimg_get_rotate_output_size(src->width, src->height, rotation, &out_width, &out_height) != LWIMG_OK) ||
        (dst->width != out_width) ||
        (dst->height != out_height)) {
        return LWIMG_ERROR;
    }

    switch (src->pixel_format) {
        case LWIMG_PIXEL_FORMAT_RGB565: lwimg_rotate_rgb565(src, rotation, dst); break;
        default: return LWIMG_ERROR;
    }

    return LWIMG_OK;
}

lwimg_status_t lwimg_get_scale_output_size(uint16_t src_width, uint16_t src_height, lwimg_scale_factor_t scale_factor, uint16_t *out_width, uint16_t *out_height)
{
    uint32_t numerator;
    uint32_t denominator;
    uint32_t width;
    uint32_t height;

    switch (scale_factor) {
        case LWIMG_SCALE_FACTOR_2X:
            numerator = 2U;
            denominator = 1U;
            break;

        case LWIMG_SCALE_FACTOR_2_5X:
            numerator = 5U;
            denominator = 2U;
            break;

        default:
            return LWIMG_ERROR;
    }

    width = (uint32_t)src_width * numerator / denominator;
    if (width > (uint32_t)UINT16_MAX) {
        return LWIMG_ERROR;
    }

    height = (uint32_t)src_height * numerator / denominator;
    if (height > (uint32_t)UINT16_MAX) {
        return LWIMG_ERROR;
    }

    if (out_width != NULL) {
        *out_width = (uint16_t)width;
    }

    if (out_height != NULL) {
        *out_height = (uint16_t)height;
    }

    return LWIMG_OK;
}

lwimg_status_t lwimg_scale(const lwimg_img_t *src, lwimg_scale_factor_t scale_factor, lwimg_scale_method_t scale_method, lwimg_img_t *dst)
{
    uint16_t out_width;
    uint16_t out_height;

    if ((lwimg_check_img(src) != LWIMG_OK) ||
        ((uint32_t)scale_method >= (uint32_t)LWIMG_SCALE_METHOD_COUNT) ||
        (lwimg_check_img(dst) != LWIMG_OK)) {
        return LWIMG_ERROR;
    }

    if ((UTIL_IS_ALIGNED((uintptr_t)src->data, sizeof(uint32_t)) == false) ||
        (UTIL_IS_ALIGNED(src->stride_pixels, 2U) == false) ||
        (UTIL_IS_ALIGNED((uintptr_t)dst->data, sizeof(uint32_t)) == false) ||
        (UTIL_IS_ALIGNED(dst->stride_pixels, 2U) == false) ||
        (dst->pixel_format != src->pixel_format) ||
        (lwimg_get_scale_output_size(src->width, src->height, scale_factor, &out_width, &out_height) != LWIMG_OK) ||
        (dst->width != out_width) ||
        (dst->height != out_height) ||
        ((scale_factor == LWIMG_SCALE_FACTOR_2_5X) &&
         ((UTIL_IS_ALIGNED(src->width, 4U) == false) ||
          (UTIL_IS_ALIGNED(src->height, 2U) == false)))) {
        return LWIMG_ERROR;
    }

    switch (src->pixel_format) {
        case LWIMG_PIXEL_FORMAT_RGB565:
            switch (scale_factor) {
                case LWIMG_SCALE_FACTOR_2X:
                    switch (scale_method) {
                        case LWIMG_SCALE_METHOD_NEAREST: lwimg_scale_rgb565_2x_nearest(src, dst); break;
                        case LWIMG_SCALE_METHOD_BILINEAR: lwimg_scale_rgb565_2_bilinear(src, dst); break;
                        default: return LWIMG_ERROR;
                    }
                    break;

                case LWIMG_SCALE_FACTOR_2_5X:
                    switch (scale_method) {
                        case LWIMG_SCALE_METHOD_NEAREST: lwimg_scale_rgb565_2_5x_nearest(src, dst); break;
                        case LWIMG_SCALE_METHOD_BILINEAR: lwimg_scale_rgb565_2_5x_bilinear(src, dst); break;
                        default: return LWIMG_ERROR;
                    }
                    break;

                default:
                    return LWIMG_ERROR;
            }
            break;

        default:
            return LWIMG_ERROR;
    }

    return LWIMG_OK;
}
