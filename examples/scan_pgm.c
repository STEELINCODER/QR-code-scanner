/* Desktop validation utility only. The mobile hot path does not malloc. */
#include "qrfast.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_token(FILE *f, char *buf, size_t cap)
{
    int c;
    do {
        c = fgetc(f);
        if (c == '#') {
            while (c != '\n' && c != EOF) c = fgetc(f);
        }
    } while (c != EOF && c <= ' ');
    if (c == EOF) return 0;

    size_t n = 0;
    do {
        if (n + 1 < cap) buf[n++] = (char)c;
        c = fgetc(f);
    } while (c != EOF && c > ' ');
    buf[n] = '\0';
    return 1;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s image.pgm\n", argv[0]);
        return 2;
    }

    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror("fopen"); return 2; }

    char tok[64];
    if (!read_token(f, tok, sizeof(tok)) || strcmp(tok, "P5") != 0) {
        fprintf(stderr, "expected binary PGM (P5)\n");
        return 2;
    }
    if (!read_token(f, tok, sizeof(tok))) return 2;
    int w = atoi(tok);
    if (!read_token(f, tok, sizeof(tok))) return 2;
    int h = atoi(tok);
    if (!read_token(f, tok, sizeof(tok))) return 2;
    int maxv = atoi(tok);
    if (w <= 0 || h <= 0 || maxv != 255) {
        fprintf(stderr, "unsupported PGM\n");
        return 2;
    }

    uint8_t *image = malloc((size_t)w * (size_t)h);
    uint32_t *scratch = calloc((size_t)w, sizeof(uint32_t));
    if (!image || !scratch) return 2;

    if (fread(image, 1, (size_t)w * (size_t)h, f) != (size_t)w * (size_t)h) {
        fprintf(stderr, "short read\n");
        return 2;
    }
    fclose(f);

    qrfast_scanner_t scanner;
    qrfast_status_t st = qrfast_init(&scanner, w, h, scratch, (size_t)w);
    if (st != QRFAST_OK) {
        fprintf(stderr, "init: %s\n", qrfast_status_string(st));
        return 1;
    }

    uint8_t payload[8896];
    qrfast_result_t result;
    st = qrfast_scan_y8(&scanner, image, (size_t)w,
                        payload, sizeof(payload), &result);

    if (st == QRFAST_OK) {
        fwrite(payload, 1, result.payload_len, stdout);
        fputc('\n', stdout);
        fprintf(stderr, "version=%d inverted=%u mirrored=%u\n",
                result.version, result.was_inverted, result.was_mirrored);
    } else {
        fprintf(stderr, "scan: %s\n", qrfast_status_string(st));
    }

    qrfast_deinit(&scanner);
    free(scratch);
    free(image);
    return st == QRFAST_OK ? 0 : 1;
}
