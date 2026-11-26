#include "lwimg.h"
#include <stddef.h>
#include <string.h>

typedef union {
    uint32_t u32[32];
    uint16_t u16[64];
    uint8_t u8[128];
} pixel_buffer_t;

static pixel_buffer_t src_buffer;
static pixel_buffer_t dst_buffer;

static void setup_rgb565(lwimg_img_t *img, void *data, uint32_t data_size, uint16_t width, uint16_t height, uint32_t stride_pixels)
{
    img->data = data;
    img->data_size = data_size;
    img->pixel_format = LWIMG_PIXEL_FORMAT_RGB565;
    img->width = width;
    img->height = height;
    img->stride_pixels = stride_pixels;
}

static void setup_yuv422(lwimg_img_t *img, void *data, uint32_t data_size, lwimg_pixel_format_t pixel_format, uint16_t width, uint16_t height, uint32_t stride_pixels)
{
    img->data = data;
    img->data_size = data_size;
    img->pixel_format = pixel_format;
    img->width = width;
    img->height = height;
    img->stride_pixels = stride_pixels;
}

static int pixels_match(const uint16_t *actual, const uint16_t *expected, size_t count)
{
    return memcmp(actual, expected, count * sizeof(uint16_t)) == 0;
}

static int check_yuv_format(lwimg_pixel_format_t pixel_format, const uint8_t *bytes)
{
    static const uint16_t expected[2] = {0xF800U, 0xFD13U};
    lwimg_img_t src;
    lwimg_img_t dst;

    memset(&src_buffer, 0, sizeof(src_buffer));
    memset(&dst_buffer, 0, sizeof(dst_buffer));
    memcpy(src_buffer.u8, bytes, 4U);

    setup_yuv422(&src, src_buffer.u8, (uint32_t)sizeof(src_buffer.u8), pixel_format, 2U, 1U, 2U);
    setup_rgb565(&dst, dst_buffer.u8, (uint32_t)sizeof(dst_buffer.u8), 2U, 1U, 2U);

    if (lwimg_yuv422_to_rgb565(&src, &dst) != LWIMG_OK) {
        return 0;
    }

    return pixels_match(dst_buffer.u16, expected, 2U) ? 1 : 0;
}

static int check_rotate(lwimg_rotation_t rotation, uint16_t width, uint16_t height, const uint16_t *expected, size_t count)
{
    static const uint16_t source[6] = {0x0001U, 0x0002U, 0x0003U, 0x0004U, 0x0005U, 0x0006U};
    lwimg_img_t src;
    lwimg_img_t dst;

    memset(&src_buffer, 0, sizeof(src_buffer));
    memset(&dst_buffer, 0, sizeof(dst_buffer));
    memcpy(src_buffer.u16, source, sizeof(source));

    setup_rgb565(&src, src_buffer.u8, (uint32_t)sizeof(src_buffer.u8), 3U, 2U, 3U);
    setup_rgb565(&dst, dst_buffer.u8, (uint32_t)sizeof(dst_buffer.u8), width, height, (uint32_t)width);

    if (lwimg_rotate(&src, rotation, &dst) != LWIMG_OK) {
        return 0;
    }

    return pixels_match(dst_buffer.u16, expected, count) ? 1 : 0;
}

static int check_scale(lwimg_scale_factor_t factor, lwimg_scale_method_t method, uint16_t src_width, uint16_t src_height, uint16_t dst_width, uint16_t dst_height, const uint16_t *expected, size_t count)
{
    lwimg_img_t src;
    lwimg_img_t dst;
    uint16_t width;
    uint16_t height;

    if ((lwimg_get_scale_output_size(src_width, src_height, factor, &width, &height) != LWIMG_OK) ||
        (width != dst_width) ||
        (height != dst_height)) {
        return 0;
    }

    setup_rgb565(&src, src_buffer.u8, (uint32_t)sizeof(src_buffer.u8), src_width, src_height, (uint32_t)src_width);
    setup_rgb565(&dst, dst_buffer.u8, (uint32_t)sizeof(dst_buffer.u8), dst_width, dst_height, (uint32_t)dst_width);

    if (lwimg_scale(&src, factor, method, &dst) != LWIMG_OK) {
        return 0;
    }

    return pixels_match(dst_buffer.u16, expected, count) ? 1 : 0;
}

int main(void)
{
    static const uint8_t uyvy_bytes[4] = {84U, 76U, 255U, 235U};
    static const uint8_t yuyv_bytes[4] = {76U, 84U, 235U, 255U};
    static const uint8_t vyuy_bytes[4] = {255U, 76U, 84U, 235U};
    static const uint8_t yvyu_bytes[4] = {76U, 255U, 235U, 84U};
    static const uint16_t expected_color[2] = {0xF800U, 0xFD13U};
    static const uint16_t rotate_0[6] = {0x0001U, 0x0002U, 0x0003U, 0x0004U, 0x0005U, 0x0006U};
    static const uint16_t rotate_90[6] = {0x0004U, 0x0001U, 0x0005U, 0x0002U, 0x0006U, 0x0003U};
    static const uint16_t rotate_180[6] = {0x0006U, 0x0005U, 0x0004U, 0x0003U, 0x0002U, 0x0001U};
    static const uint16_t rotate_270[6] = {0x0003U, 0x0006U, 0x0002U, 0x0005U, 0x0001U, 0x0004U};
    static const uint16_t scale_2x_source[4] = {0x0000U, 0x07E0U, 0x001FU, 0xFFFFU};
    static const uint16_t scale_2x_nearest[16] = {
        0x0000U, 0x0000U, 0x07E0U, 0x07E0U,
        0x0000U, 0x0000U, 0x07E0U, 0x07E0U,
        0x001FU, 0x001FU, 0xFFFFU, 0xFFFFU,
        0x001FU, 0x001FU, 0xFFFFU, 0xFFFFU,
    };
    static const uint16_t scale_2x_bilinear[16] = {
        0x0000U, 0x03E0U, 0x07E0U, 0x07E0U,
        0x000FU, 0x3BEFU, 0x7FEFU, 0x7FEFU,
        0x001FU, 0x7BFFU, 0xFFFFU, 0xFFFFU,
        0x001FU, 0x7BFFU, 0xFFFFU, 0xFFFFU,
    };
    static const uint16_t scale_25_source[8] = {
        0x0100U, 0x0101U, 0x0102U, 0x0103U,
        0x0200U, 0x0201U, 0x0202U, 0x0203U,
    };
    static const uint16_t scale_25_nearest[50] = {
        0x0100U, 0x0100U, 0x0101U, 0x0101U, 0x0101U, 0x0102U, 0x0102U, 0x0103U, 0x0103U, 0x0103U,
        0x0100U, 0x0100U, 0x0101U, 0x0101U, 0x0101U, 0x0102U, 0x0102U, 0x0103U, 0x0103U, 0x0103U,
        0x0200U, 0x0200U, 0x0201U, 0x0201U, 0x0201U, 0x0202U, 0x0202U, 0x0203U, 0x0203U, 0x0203U,
        0x0200U, 0x0200U, 0x0201U, 0x0201U, 0x0201U, 0x0202U, 0x0202U, 0x0203U, 0x0203U, 0x0203U,
        0x0200U, 0x0200U, 0x0201U, 0x0201U, 0x0201U, 0x0202U, 0x0202U, 0x0203U, 0x0203U, 0x0203U,
    };
    static const uint16_t scale_25_bilinear[50] = {
        0x0100U, 0x0100U, 0x0100U, 0x0100U, 0x0101U, 0x0102U, 0x0102U, 0x0102U, 0x0102U, 0x0103U,
        0x0140U, 0x0140U, 0x0140U, 0x0140U, 0x0141U, 0x0142U, 0x0142U, 0x0142U, 0x0142U, 0x0143U,
        0x0180U, 0x0180U, 0x0180U, 0x0180U, 0x0181U, 0x0182U, 0x0182U, 0x0182U, 0x0182U, 0x0183U,
        0x01C0U, 0x01C0U, 0x01C0U, 0x01C0U, 0x01C1U, 0x01C2U, 0x01C2U, 0x01C2U, 0x01C2U, 0x01C3U,
        0x0200U, 0x0200U, 0x0200U, 0x0200U, 0x0201U, 0x0202U, 0x0202U, 0x0202U, 0x0202U, 0x0203U,
    };
    static const uint16_t stride_scaled[24] = {
        0x0000U, 0x0000U, 0x07E0U, 0x07E0U, 0xBEEFU, 0xBEEFU,
        0x0000U, 0x0000U, 0x07E0U, 0x07E0U, 0xBEEFU, 0xBEEFU,
        0x001FU, 0x001FU, 0xFFFFU, 0xFFFFU, 0xBEEFU, 0xBEEFU,
        0x001FU, 0x001FU, 0xFFFFU, 0xFFFFU, 0xBEEFU, 0xBEEFU,
    };
    lwimg_img_t src;
    lwimg_img_t dst;
    uint16_t width;
    uint16_t height;
    size_t index;

    if (!check_yuv_format(LWIMG_PIXEL_FORMAT_YUV422_UYVY, uyvy_bytes) ||
        !check_yuv_format(LWIMG_PIXEL_FORMAT_YUV422_YUYV, yuyv_bytes) ||
        !check_yuv_format(LWIMG_PIXEL_FORMAT_YUV422_VYUY, vyuy_bytes) ||
        !check_yuv_format(LWIMG_PIXEL_FORMAT_YUV422_YVYU, yvyu_bytes)) {
        return 1;
    }

    if ((lwimg_get_bytes_per_pixel(LWIMG_PIXEL_FORMAT_RGB565) != 2U) ||
        (lwimg_get_bytes_per_pixel(LWIMG_PIXEL_FORMAT_YUV422_UYVY) != 2U) ||
        (lwimg_get_bytes_per_pixel(LWIMG_PIXEL_FORMAT_YUV422_YUYV) != 2U) ||
        (lwimg_get_bytes_per_pixel(LWIMG_PIXEL_FORMAT_YUV422_VYUY) != 2U) ||
        (lwimg_get_bytes_per_pixel(LWIMG_PIXEL_FORMAT_YUV422_YVYU) != 2U) ||
        (lwimg_get_bytes_per_pixel((lwimg_pixel_format_t)LWIMG_PIXEL_FORMAT_COUNT) != 0U) ||
        (lwimg_get_bytes_per_pixel((lwimg_pixel_format_t)0xFFU) != 0U)) {
        return 1;
    }

    memset(&src_buffer, 0, sizeof(src_buffer));
    memcpy(src_buffer.u8, yuyv_bytes, 4U);
    setup_yuv422(&src, src_buffer.u8, (uint32_t)sizeof(src_buffer.u8), LWIMG_PIXEL_FORMAT_YUV422_YUYV, 2U, 1U, 2U);
    setup_rgb565(&dst, src_buffer.u8, (uint32_t)sizeof(src_buffer.u8), 2U, 1U, 2U);
    if ((lwimg_yuv422_to_rgb565(&src, &dst) != LWIMG_OK) ||
        (!pixels_match(src_buffer.u16, expected_color, 2U))) {
        return 1;
    }

    if (!check_rotate(LWIMG_ROTATION_0, 3U, 2U, rotate_0, 6U) ||
        !check_rotate(LWIMG_ROTATION_90, 2U, 3U, rotate_90, 6U) ||
        !check_rotate(LWIMG_ROTATION_180, 3U, 2U, rotate_180, 6U) ||
        !check_rotate(LWIMG_ROTATION_270, 2U, 3U, rotate_270, 6U)) {
        return 1;
    }

    memset(&src_buffer, 0, sizeof(src_buffer));
    memcpy(src_buffer.u16, scale_2x_source, sizeof(scale_2x_source));
    if (!check_scale(LWIMG_SCALE_FACTOR_2X, LWIMG_SCALE_METHOD_NEAREST, 2U, 2U, 4U, 4U, scale_2x_nearest, 16U) ||
        !check_scale(LWIMG_SCALE_FACTOR_2X, LWIMG_SCALE_METHOD_BILINEAR, 2U, 2U, 4U, 4U, scale_2x_bilinear, 16U)) {
        return 1;
    }

    memset(&src_buffer, 0, sizeof(src_buffer));
    memcpy(src_buffer.u16, scale_25_source, sizeof(scale_25_source));
    if (!check_scale(LWIMG_SCALE_FACTOR_2_5X, LWIMG_SCALE_METHOD_NEAREST, 4U, 2U, 10U, 5U, scale_25_nearest, 50U) ||
        !check_scale(LWIMG_SCALE_FACTOR_2_5X, LWIMG_SCALE_METHOD_BILINEAR, 4U, 2U, 10U, 5U, scale_25_bilinear, 50U)) {
        return 1;
    }

    memset(&src_buffer, 0, sizeof(src_buffer));
    memset(&dst_buffer, 0, sizeof(dst_buffer));
    for (index = 0U; index < 8U; index++) {
        src_buffer.u16[index] = 0xDEADU;
    }
    for (index = 0U; index < 24U; index++) {
        dst_buffer.u16[index] = 0xBEEFU;
    }
    src_buffer.u16[0] = 0x0000U;
    src_buffer.u16[1] = 0x07E0U;
    src_buffer.u16[4] = 0x001FU;
    src_buffer.u16[5] = 0xFFFFU;
    setup_rgb565(&src, src_buffer.u8, (uint32_t)(8U * sizeof(uint16_t)), 2U, 2U, 4U);
    setup_rgb565(&dst, dst_buffer.u8, (uint32_t)(24U * sizeof(uint16_t)), 4U, 4U, 6U);
    if ((lwimg_scale(&src, LWIMG_SCALE_FACTOR_2X, LWIMG_SCALE_METHOD_NEAREST, &dst) != LWIMG_OK) ||
        (!pixels_match(dst_buffer.u16, stride_scaled, 24U)) ||
        (src_buffer.u16[2] != 0xDEADU) ||
        (src_buffer.u16[3] != 0xDEADU) ||
        (src_buffer.u16[6] != 0xDEADU) ||
        (src_buffer.u16[7] != 0xDEADU)) {
        return 1;
    }

    if ((lwimg_get_scale_output_size(40000U, 4U, LWIMG_SCALE_FACTOR_2X, &width, &height) != LWIMG_ERROR) ||
        (lwimg_get_scale_output_size(4U, 40000U, LWIMG_SCALE_FACTOR_2X, &width, &height) != LWIMG_ERROR) ||
        (lwimg_get_scale_output_size(26215U, 2U, LWIMG_SCALE_FACTOR_2_5X, &width, &height) != LWIMG_ERROR) ||
        (lwimg_get_scale_output_size(2U, 2U, (lwimg_scale_factor_t)LWIMG_SCALE_FACTOR_COUNT, &width, &height) != LWIMG_ERROR) ||
        (lwimg_get_scale_output_size(2U, 2U, LWIMG_SCALE_FACTOR_2X, NULL, NULL) != LWIMG_OK) ||
        (lwimg_get_rotate_output_size(3U, 2U, (lwimg_rotation_t)LWIMG_ROTATION_COUNT, &width, &height) != LWIMG_ERROR) ||
        (lwimg_get_yuv422_to_rgb565_output_size(2U, 1U, NULL, NULL) != LWIMG_OK)) {
        return 1;
    }

    if ((lwimg_get_scale_output_size(26214U, 2U, LWIMG_SCALE_FACTOR_2_5X, &width, &height) != LWIMG_OK) ||
        (width != 65535U) ||
        (height != 5U)) {
        return 1;
    }

    memset(&src_buffer, 0, sizeof(src_buffer));
    memset(&dst_buffer, 0, sizeof(dst_buffer));
    memcpy(src_buffer.u8, uyvy_bytes, 4U);
    setup_yuv422(&src, src_buffer.u8, (uint32_t)sizeof(src_buffer.u8), LWIMG_PIXEL_FORMAT_YUV422_UYVY, 2U, 1U, 2U);
    setup_rgb565(&dst, dst_buffer.u8, (uint32_t)sizeof(dst_buffer.u8), 2U, 1U, 2U);

    src.width = 3U;
    src.stride_pixels = 4U;
    dst.width = 3U;
    dst.stride_pixels = 4U;
    if (lwimg_yuv422_to_rgb565(&src, &dst) != LWIMG_ERROR) {
        return 1;
    }

    setup_yuv422(&src, src_buffer.u8, (uint32_t)sizeof(src_buffer.u8), LWIMG_PIXEL_FORMAT_YUV422_UYVY, 2U, 1U, 2U);
    setup_rgb565(&dst, dst_buffer.u8, (uint32_t)sizeof(dst_buffer.u8), 2U, 1U, 2U);
    src.data_size = 3U;
    if (lwimg_yuv422_to_rgb565(&src, &dst) != LWIMG_ERROR) {
        return 1;
    }

    setup_yuv422(&src, src_buffer.u8, (uint32_t)sizeof(src_buffer.u8), LWIMG_PIXEL_FORMAT_YUV422_UYVY, 2U, 1U, 2U);
    setup_rgb565(&dst, dst_buffer.u8, (uint32_t)sizeof(dst_buffer.u8), 2U, 1U, 2U);
    src.stride_pixels = 1U;
    if (lwimg_yuv422_to_rgb565(&src, &dst) != LWIMG_ERROR) {
        return 1;
    }

    setup_yuv422(&src, src_buffer.u8, (uint32_t)sizeof(src_buffer.u8), LWIMG_PIXEL_FORMAT_YUV422_UYVY, 2U, 1U, 2U);
    setup_rgb565(&dst, dst_buffer.u8, (uint32_t)sizeof(dst_buffer.u8), 2U, 1U, 2U);
    src.pixel_format = (lwimg_pixel_format_t)LWIMG_PIXEL_FORMAT_COUNT;
    if (lwimg_yuv422_to_rgb565(&src, &dst) != LWIMG_ERROR) {
        return 1;
    }

    setup_yuv422(&src, src_buffer.u8, (uint32_t)sizeof(src_buffer.u8), LWIMG_PIXEL_FORMAT_YUV422_UYVY, 2U, 1U, 2U);
    setup_rgb565(&dst, dst_buffer.u8, (uint32_t)sizeof(dst_buffer.u8), 2U, 1U, 2U);
    dst.pixel_format = LWIMG_PIXEL_FORMAT_YUV422_UYVY;
    if (lwimg_yuv422_to_rgb565(&src, &dst) != LWIMG_ERROR) {
        return 1;
    }

    setup_yuv422(&src, src_buffer.u8, (uint32_t)sizeof(src_buffer.u8), LWIMG_PIXEL_FORMAT_YUV422_UYVY, 2U, 1U, 2U);
    setup_rgb565(&dst, dst_buffer.u8, (uint32_t)sizeof(dst_buffer.u8), 2U, 1U, 2U);
    src.data = NULL;
    if (lwimg_yuv422_to_rgb565(&src, &dst) != LWIMG_ERROR) {
        return 1;
    }

    setup_yuv422(&src, src_buffer.u8, (uint32_t)sizeof(src_buffer.u8), LWIMG_PIXEL_FORMAT_YUV422_UYVY, 2U, 1U, 2U);
    setup_rgb565(&dst, dst_buffer.u8, (uint32_t)sizeof(dst_buffer.u8), 2U, 1U, 2U);
    dst.width = 4U;
    dst.stride_pixels = 4U;
    if (lwimg_yuv422_to_rgb565(&src, &dst) != LWIMG_ERROR) {
        return 1;
    }

    setup_rgb565(&src, src_buffer.u8, (uint32_t)sizeof(src_buffer.u8), 3U, 2U, 3U);
    setup_rgb565(&dst, dst_buffer.u8, (uint32_t)sizeof(dst_buffer.u8), 2U, 3U, 2U);
    if (lwimg_rotate(&src, (lwimg_rotation_t)LWIMG_ROTATION_COUNT, &dst) != LWIMG_ERROR) {
        return 1;
    }

    setup_rgb565(&dst, dst_buffer.u8, (uint32_t)sizeof(dst_buffer.u8), 3U, 3U, 3U);
    if (lwimg_rotate(&src, LWIMG_ROTATION_90, &dst) != LWIMG_ERROR) {
        return 1;
    }

    setup_rgb565(&src, src_buffer.u8, (uint32_t)sizeof(src_buffer.u8), 2U, 2U, 2U);
    setup_rgb565(&dst, dst_buffer.u8, (uint32_t)sizeof(dst_buffer.u8), 4U, 4U, 4U);
    if ((lwimg_scale(&src, (lwimg_scale_factor_t)LWIMG_SCALE_FACTOR_COUNT, LWIMG_SCALE_METHOD_NEAREST, &dst) != LWIMG_ERROR) ||
        (lwimg_scale(&src, LWIMG_SCALE_FACTOR_2X, (lwimg_scale_method_t)LWIMG_SCALE_METHOD_COUNT, &dst) != LWIMG_ERROR) ||
        (lwimg_scale(NULL, LWIMG_SCALE_FACTOR_2X, LWIMG_SCALE_METHOD_NEAREST, &dst) != LWIMG_ERROR)) {
        return 1;
    }

    setup_rgb565(&dst, dst_buffer.u8, (uint32_t)sizeof(dst_buffer.u8), 5U, 5U, 6U);
    if (lwimg_scale(&src, LWIMG_SCALE_FACTOR_2_5X, LWIMG_SCALE_METHOD_NEAREST, &dst) != LWIMG_ERROR) {
        return 1;
    }

    setup_rgb565(&dst, dst_buffer.u8, (uint32_t)sizeof(dst_buffer.u8), 4U, 4U, 5U);
    if (lwimg_scale(&src, LWIMG_SCALE_FACTOR_2X, LWIMG_SCALE_METHOD_NEAREST, &dst) != LWIMG_ERROR) {
        return 1;
    }

    return 0;
}
