#include "qrfast.h"

#include <limits.h>
#include <string.h>

#include <quirc.h>


#if defined(__ARM_NEON) || defined(__aarch64__)
#include <arm_neon.h>
#define QRFAST_HAS_NEON 1
#else
#define QRFAST_HAS_NEON 0
#endif

static int qrfast_min_i(int a, int b) { return a < b ? a : b; }
static int qrfast_max_i(int a, int b) { return a > b ? a : b; }

static void add_row_to_columns(uint32_t *column_sums,
                               const uint8_t *row,
                               int32_t width)
{
    for (int32_t x = 0; x < width; ++x)
        column_sums[x] += row[x];
}

static void sub_row_from_columns(uint32_t *column_sums,
                                 const uint8_t *row,
                                 int32_t width)
{
    for (int32_t x = 0; x < width; ++x)
        column_sums[x] -= row[x];
}

qrfast_status_t qrfast_binarize_y8(const uint8_t *src,
                                   size_t src_stride,
                                   uint8_t *dst,
                                   size_t dst_stride,
                                   int32_t width,
                                   int32_t height,
                                   uint16_t window_radius,
                                   uint8_t threshold_percent,
                                   uint32_t *column_sums,
                                   size_t column_sums_len)
{
    if (!src || !dst || !column_sums || width <= 0 || height <= 0)
        return QRFAST_ERR_ARGUMENT;
    if (src_stride < (size_t)width || dst_stride < (size_t)width)
        return QRFAST_ERR_DIMENSION;
    if (column_sums_len < (size_t)width)
        return QRFAST_ERR_SCRATCH;
    if (threshold_percent > 50)
        return QRFAST_ERR_ARGUMENT;

    int radius = (int)window_radius;
    if (radius < 1)
        radius = 1;
    radius = qrfast_min_i(radius, qrfast_max_i(width, height) - 1);

    memset(column_sums, 0, (size_t)width * sizeof(column_sums[0]));

    int top = 0;
    int bottom = qrfast_min_i(height - 1, radius);
    for (int yy = top; yy <= bottom; ++yy)
        add_row_to_columns(column_sums, src + (size_t)yy * src_stride, width);

    const uint32_t keep_percent = 100u - threshold_percent;

    for (int y = 0; y < height; ++y) {
        const int wanted_top = qrfast_max_i(0, y - radius);
        const int wanted_bottom = qrfast_min_i(height - 1, y + radius);

        while (top < wanted_top) {
            sub_row_from_columns(column_sums,
                                 src + (size_t)top * src_stride,
                                 width);
            ++top;
        }
        while (bottom < wanted_bottom) {
            ++bottom;
            add_row_to_columns(column_sums,
                               src + (size_t)bottom * src_stride,
                               width);
        }

        int left = 0;
        int right = qrfast_min_i(width - 1, radius);
        uint64_t window_sum = 0;
        for (int xx = left; xx <= right; ++xx)
            window_sum += column_sums[xx];

        const uint32_t window_h = (uint32_t)(bottom - top + 1);
        const uint8_t *src_row = src + (size_t)y * src_stride;
        uint8_t *dst_row = dst + (size_t)y * dst_stride;

        for (int x = 0; x < width; ++x) {
            const uint32_t window_w = (uint32_t)(right - left + 1);
            const uint64_t area = (uint64_t)window_w * window_h;
            const uint64_t pixel_term = (uint64_t)src_row[x] * area * 100u;
            const uint64_t mean_term = window_sum * keep_percent;

            dst_row[x] = (pixel_term < mean_term) ? 0u : 255u;

            if (x + 1 < width) {
                const int next_left = qrfast_max_i(0, (x + 1) - radius);
                const int next_right = qrfast_min_i(width - 1, (x + 1) + radius);

                while (left < next_left) {
                    window_sum -= column_sums[left];
                    ++left;
                }
                while (right < next_right) {
                    ++right;
                    window_sum += column_sums[right];
                }
            }
        }
    }

    return QRFAST_OK;
}

static void invert_binary(uint8_t *buf, size_t len)
{
#if QRFAST_HAS_NEON
    size_t i = 0;
    const uint8x16_t all_ones = vdupq_n_u8(0xFFu);
    for (; i + 16 <= len; i += 16) {
        uint8x16_t v = vld1q_u8(buf + i);
        v = veorq_u8(v, all_ones);
        vst1q_u8(buf + i, v);
    }
    for (; i < len; ++i)
        buf[i] ^= 0xFFu;
#else
    for (size_t i = 0; i < len; ++i)
        buf[i] ^= 0xFFu;
#endif
}

static qrfast_status_t decode_from_prepared_image(qrfast_scanner_t *scanner,
                                                   uint8_t *payload_out,
                                                   size_t payload_capacity,
                                                   qrfast_result_t *result,
                                                   uint8_t inverted,
                                                   uint8_t adaptive)
{
    quirc_end(scanner->decoder);
    const int count = quirc_count(scanner->decoder);

    for (int i = 0; i < count; ++i) {
        struct quirc_code code;
        struct quirc_data data;
        quirc_extract(scanner->decoder, i, &code);

        quirc_decode_error_t err = quirc_decode(&code, &data);
        uint8_t mirrored = 0;

        if (err != QUIRC_SUCCESS) {
            quirc_flip(&code);
            err = quirc_decode(&code, &data);
            mirrored = (err == QUIRC_SUCCESS) ? 1u : 0u;
        }

        if (err != QUIRC_SUCCESS)
            continue;

        if (data.payload_len < 0)
            continue;
        const size_t payload_len = (size_t)data.payload_len;
        if (payload_len > payload_capacity)
            return QRFAST_ERR_OUTPUT;

        if (payload_len != 0)
            memcpy(payload_out, data.payload, payload_len);

        if (result) {
            memset(result, 0, sizeof(*result));
            result->payload_len = payload_len;
            result->version = data.version;
            result->ecc_level = data.ecc_level;
            result->mask = data.mask;
            result->eci = data.eci;
            result->was_mirrored = mirrored;
            result->was_inverted = inverted;
            result->used_adaptive = adaptive;
            for (int c = 0; c < 4; ++c) {
                result->corners[c].x = code.corners[c].x;
                result->corners[c].y = code.corners[c].y;
            }
        }
        return QRFAST_OK;
    }

    return QRFAST_NOT_FOUND;
}

qrfast_status_t qrfast_init(qrfast_scanner_t *scanner,
                            int32_t width,
                            int32_t height,
                            uint32_t *column_sums,
                            size_t column_sums_len)
{
    if (!scanner || !column_sums || width <= 0 || height <= 0)
        return QRFAST_ERR_ARGUMENT;
    if (column_sums_len < (size_t)width)
        return QRFAST_ERR_SCRATCH;

    memset(scanner, 0, sizeof(*scanner));
    scanner->decoder = quirc_new();
    if (!scanner->decoder)
        return QRFAST_ERR_ALLOC;

    if (quirc_resize(scanner->decoder, width, height) < 0) {
        quirc_destroy(scanner->decoder);
        memset(scanner, 0, sizeof(*scanner));
        return QRFAST_ERR_ALLOC;
    }

    scanner->column_sums = column_sums;
    scanner->column_sums_len = column_sums_len;
    scanner->width = width;
    scanner->height = height;

    /* Bradley-style default: total window ~= width / 8. */
    int default_radius = width / 16;
    if (default_radius < 8)
        default_radius = 8;
    if (default_radius > 96)
        default_radius = 96;
    scanner->window_radius = (uint16_t)default_radius;
    scanner->threshold_percent = 15u;
    scanner->retry_inverted = 1u;
    scanner->fast_global_first = 1u;

    return QRFAST_OK;
}

void qrfast_deinit(qrfast_scanner_t *scanner)
{
    if (!scanner)
        return;
    if (scanner->decoder)
        quirc_destroy(scanner->decoder);
    memset(scanner, 0, sizeof(*scanner));
}

void qrfast_set_adaptive_threshold(qrfast_scanner_t *scanner,
                                   uint16_t window_radius,
                                   uint8_t threshold_percent)
{
    if (!scanner)
        return;
    if (window_radius != 0)
        scanner->window_radius = window_radius;
    if (threshold_percent <= 50)
        scanner->threshold_percent = threshold_percent;
}

void qrfast_set_retry_inverted(qrfast_scanner_t *scanner, int enabled)
{
    if (scanner)
        scanner->retry_inverted = enabled ? 1u : 0u;
}

void qrfast_set_fast_global_first(qrfast_scanner_t *scanner, int enabled)
{
    if (scanner)
        scanner->fast_global_first = enabled ? 1u : 0u;
}

qrfast_status_t qrfast_scan_y8(qrfast_scanner_t *scanner,
                               const uint8_t *y_plane,
                               size_t y_stride,
                               uint8_t *payload_out,
                               size_t payload_capacity,
                               qrfast_result_t *result)
{
    if (!scanner || !scanner->decoder || !y_plane || !payload_out)
        return QRFAST_ERR_ARGUMENT;
    if (y_stride < (size_t)scanner->width)
        return QRFAST_ERR_DIMENSION;

    int qw = 0;
    int qh = 0;
    uint8_t *image = quirc_begin(scanner->decoder, &qw, &qh);
    if (!image || qw != scanner->width || qh != scanner->height)
        return QRFAST_ERR_DIMENSION;

    qrfast_status_t st = QRFAST_NOT_FOUND;

    /* Tier 0: the common case. Copy native luma directly and let quirc use
     * its global threshold. This avoids the adaptive pass on easy frames. */
    if (scanner->fast_global_first) {
        for (int32_t row = 0; row < scanner->height; ++row) {
            memcpy(image + (size_t)row * (size_t)scanner->width,
                   y_plane + (size_t)row * y_stride,
                   (size_t)scanner->width);
        }

        st = decode_from_prepared_image(scanner,
                                        payload_out,
                                        payload_capacity,
                                        result,
                                        0u,
                                        0u);
        if (st != QRFAST_NOT_FOUND)
            return st;

        image = quirc_begin(scanner->decoder, &qw, &qh);
        if (!image)
            return QRFAST_ERR_DIMENSION;
    }

    /* Tier 1: uneven illumination / shadows / screen glare. */
    st = qrfast_binarize_y8(y_plane,
                            y_stride,
                            image,
                            (size_t)scanner->width,
                            scanner->width,
                            scanner->height,
                            scanner->window_radius,
                            scanner->threshold_percent,
                            scanner->column_sums,
                            scanner->column_sums_len);
    if (st != QRFAST_OK)
        return st;

    st = decode_from_prepared_image(scanner,
                                    payload_out,
                                    payload_capacity,
                                    result,
                                    0u,
                                    1u);
    if (st != QRFAST_NOT_FOUND || !scanner->retry_inverted)
        return st;

    /* Tier 2: light modules on a dark background. */
    image = quirc_begin(scanner->decoder, &qw, &qh);
    if (!image)
        return QRFAST_ERR_DIMENSION;
    invert_binary(image, (size_t)scanner->width * (size_t)scanner->height);

    return decode_from_prepared_image(scanner,
                                      payload_out,
                                      payload_capacity,
                                      result,
                                      1u,
                                      1u);
}

qrfast_status_t qrfast_rgba8888_to_y8(const uint8_t *rgba,
                                      size_t rgba_stride,
                                      uint8_t *y_out,
                                      size_t y_stride,
                                      int32_t width,
                                      int32_t height)
{
    if (!rgba || !y_out || width <= 0 || height <= 0)
        return QRFAST_ERR_ARGUMENT;
    if (rgba_stride < (size_t)width * 4u || y_stride < (size_t)width)
        return QRFAST_ERR_DIMENSION;

    for (int32_t y = 0; y < height; ++y) {
        const uint8_t *src = rgba + (size_t)y * rgba_stride;
        uint8_t *dst = y_out + (size_t)y * y_stride;
        int32_t x = 0;

#if QRFAST_HAS_NEON
        for (; x + 8 <= width; x += 8) {
            const uint8x8x4_t px = vld4_u8(src + (size_t)x * 4u);
            uint8x8_t weight_r = vdup_n_u8(77u);
            uint8x8_t weight_g = vdup_n_u8(150u);
            uint8x8_t weight_b = vdup_n_u8(29u);

            uint16x8_t sum = vmull_u8(px.val[0], weight_r);
            sum = vmlal_u8(sum, px.val[1], weight_g);
            sum = vmlal_u8(sum, px.val[2], weight_b);

            vst1_u8(dst + x, vshrn_n_u16(sum, 8));
        }
#endif
        for (; x < width; ++x) {
            const uint8_t r = src[(size_t)x * 4u + 0u];
            const uint8_t g = src[(size_t)x * 4u + 1u];
            const uint8_t b = src[(size_t)x * 4u + 2u];
            dst[x] = (uint8_t)(((uint32_t)77u * r +
                                (uint32_t)150u * g +
                                (uint32_t)29u * b) >> 8);
        }
    }
    return QRFAST_OK;
}

const char *qrfast_status_string(qrfast_status_t status)
{
    switch (status) {
    case QRFAST_OK: return "ok";
    case QRFAST_NOT_FOUND: return "not found";
    case QRFAST_ERR_ARGUMENT: return "invalid argument";
    case QRFAST_ERR_ALLOC: return "initial allocation failed";
    case QRFAST_ERR_DIMENSION: return "invalid dimensions or stride";
    case QRFAST_ERR_SCRATCH: return "scratch buffer too small";
    case QRFAST_ERR_OUTPUT: return "payload output buffer too small";
    default: return "unknown error";
    }
}
