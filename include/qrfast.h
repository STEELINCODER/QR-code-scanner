#ifndef QRFAST_H
#define QRFAST_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct quirc;

typedef enum qrfast_status {
    QRFAST_OK = 1,
    QRFAST_NOT_FOUND = 0,
    QRFAST_ERR_ARGUMENT = -1,
    QRFAST_ERR_ALLOC = -2,
    QRFAST_ERR_DIMENSION = -3,
    QRFAST_ERR_SCRATCH = -4,
    QRFAST_ERR_OUTPUT = -5
} qrfast_status_t;

typedef struct qrfast_point {
    int32_t x;
    int32_t y;
} qrfast_point_t;

typedef struct qrfast_result {
    size_t payload_len;
    int32_t version;
    int32_t ecc_level;
    int32_t mask;
    uint32_t eci;
    qrfast_point_t corners[4];
    uint8_t was_mirrored;
    uint8_t was_inverted;
    uint8_t used_adaptive;
} qrfast_result_t;

typedef struct qrfast_scanner {
    struct quirc *decoder;
    uint32_t *column_sums;
    size_t column_sums_len;
    int32_t width;
    int32_t height;
    uint16_t window_radius;
    uint8_t threshold_percent;
    uint8_t retry_inverted;
    uint8_t fast_global_first;
} qrfast_scanner_t;

/*
 * Initializes a persistent scanner for one fixed processing resolution.
 * quirc_new()/quirc_resize() may allocate here. No allocation is performed
 * by qrfast_scan_y8() after successful initialization.
 *
 * column_sums must contain at least width uint32_t elements and remains
 * caller-owned for the lifetime of the scanner.
 */
qrfast_status_t qrfast_init(qrfast_scanner_t *scanner,
                            int32_t width,
                            int32_t height,
                            uint32_t *column_sums,
                            size_t column_sums_len);

void qrfast_deinit(qrfast_scanner_t *scanner);

void qrfast_set_adaptive_threshold(qrfast_scanner_t *scanner,
                                   uint16_t window_radius,
                                   uint8_t threshold_percent);

void qrfast_set_retry_inverted(qrfast_scanner_t *scanner, int enabled);
void qrfast_set_fast_global_first(qrfast_scanner_t *scanner, int enabled);

/*
 * Zero-allocation per-frame scanner for an 8-bit luma plane.
 * y_stride is bytes between input rows. The input may therefore be a crop
 * or a platform camera Y plane without repacking.
 */
qrfast_status_t qrfast_scan_y8(qrfast_scanner_t *scanner,
                               const uint8_t *y_plane,
                               size_t y_stride,
                               uint8_t *payload_out,
                               size_t payload_capacity,
                               qrfast_result_t *result);

/*
 * Standalone Bradley-style local mean thresholding. dst_stride must be at
 * least width. column_sums_len must be at least width.
 * Output is 0 for black and 255 for white.
 */
qrfast_status_t qrfast_binarize_y8(const uint8_t *src,
                                   size_t src_stride,
                                   uint8_t *dst,
                                   size_t dst_stride,
                                   int32_t width,
                                   int32_t height,
                                   uint16_t window_radius,
                                   uint8_t threshold_percent,
                                   uint32_t *column_sums,
                                   size_t column_sums_len);

/* Optional fallback when a platform only exposes RGBA8888. */
qrfast_status_t qrfast_rgba8888_to_y8(const uint8_t *rgba,
                                      size_t rgba_stride,
                                      uint8_t *y_out,
                                      size_t y_stride,
                                      int32_t width,
                                      int32_t height);

const char *qrfast_status_string(qrfast_status_t status);

#ifdef __cplusplus
}
#endif

#endif
