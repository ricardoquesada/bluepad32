/* Copyright (c) 2014 John Schember <john@nachtimwald.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE
 */
// Taken from here: https://github.com/user-none/sixaxispairer

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hidapi/hidapi.h>

#define VENDOR_SONY 0x054c
#define PRODUCT_PS3NAV 0x042f
#define PRODUCT_PS3 0x0268
#define PRODUCT_PS4 0x05c4
/* 0xf5   == (0x03f5 & ~(3 << 8))
 * 0x03f5 == (0xf5 | (3 << 8))
 * HIDAPI will automatically add (3 << 8 to the report id.
 * Other tools for setting the report id use hid libraries which
 * don't automatically do this. */
#define MAC_REPORT_ID_PS3 0xf5
#define MAC_REPORT_ID_PS4 0x12

static unsigned char char_to_nible(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return 255;
}

/* return 1 on success, 0 on failure. */
static int mac_to_bytes(const char* in, size_t in_len, unsigned char* out, size_t out_len) {
    const char* p;
    size_t i = 0;

    if (in == NULL || out == NULL || in_len == 0 || out_len == 0)
        return 0;

    memset(out, 0, out_len);
    p = in;
    while (p + 1 < in + in_len && i < out_len) {
        if (*p == ':') {
            p++;
            continue;
        }

        if (!isxdigit(*p) || !isxdigit(*(p + 1))) {
            return 0;
        }

        out[i] = (char_to_nible(*p) << 4) | (char_to_nible(*(p + 1) & 0xff));
        i++;
        p += 2;
    }

    if (p < in + in_len)
        return 0;
    return 1;
}

static void pair_device(hid_device* dev, unsigned short product_id, const char* mac, size_t mac_len) {
    int ret;
    if (product_id == PRODUCT_PS4) {
        unsigned char buf[16];

        memset(buf, 0, sizeof(buf));
        buf[0] = MAC_REPORT_ID_PS4;
        buf[1] = 0xEE;
        buf[2] = 0x45;
        buf[3] = 0x84;
        buf[4] = 0x66;
        buf[5] = 0x15;
        buf[6] = 0xA4;
        buf[7] = 0x08;
        buf[8] = 0x25;
        buf[9] = 0x00;
        if ((mac_len != 12 && mac_len != 17) || !mac_to_bytes(mac, mac_len, buf + 10, 6)) {
            printf("Invalid mac\n");
            return;
        }

        ret = hid_send_feature_report(dev, buf, sizeof(buf));
    } else {
        unsigned char buf[8];
        int ret;

        memset(buf, 0, sizeof(buf));
        buf[0] = MAC_REPORT_ID_PS3;
        buf[1] = 0x0;
        if ((mac_len != 12 && mac_len != 17) || !mac_to_bytes(mac, mac_len, buf + 2, sizeof(buf) - 2)) {
            printf("Invalid mac\n");
            return;
        }

        ret = hid_send_feature_report(dev, buf, sizeof(buf));
    }
    if (ret == -1) {
        printf("Failed to set mac\n");
    }
}

static void show_pairing(hid_device* dev, unsigned short product_id) {
    unsigned char buf[8];
    int ret;

    memset(buf, 0, sizeof(buf));
    buf[0] = (product_id == PRODUCT_PS4) ? MAC_REPORT_ID_PS4 : MAC_REPORT_ID_PS3;
    buf[1] = 0x0;

    ret = hid_get_feature_report(dev, buf, sizeof(buf));
    if (ret < 8) {
        printf("Read error\n");
        return;
    }
    printf("%02x:%02x:%02x:%02x:%02x:%02x\n", buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
}

char process_device(unsigned short vendor_id, unsigned short product_id, int argc, char** argv) {
    hid_device* dev;

    dev = hid_open(vendor_id, product_id, NULL);
    if (dev == NULL) {
        return 1;
    }
    switch (product_id) {
        case PRODUCT_PS3:
            printf("Found PS3 Controller\n");
            break;
        case PRODUCT_PS3NAV:
            printf("Found PS3 Navigation Controller\n");
            break;
        case PRODUCT_PS4:
            printf("Found PS4 Controller\n");
            break;
    }
    if (argc == 2) {
        pair_device(dev, product_id, argv[1], strlen(argv[1]));
    } else {
        show_pairing(dev, product_id);
    }

    hid_close(dev);
    return 0;
}

int main(int argc, char** argv) {
    if ((argc != 1 && argc != 2) ||
        (argc == 2 && (strncmp(argv[1], "-h", 2) == 0 || strncmp(argv[1], "--help", 6) == 0))) {
        printf("usage:\n\t%s [mac]\n", argv[0]);
        return 0;
    }

    if (process_device(VENDOR_SONY, PRODUCT_PS3, argc, argv) &&
        process_device(VENDOR_SONY, PRODUCT_PS3NAV, argc, argv) &&
        process_device(VENDOR_SONY, PRODUCT_PS4, argc, argv)) {
        fprintf(stderr, "Could not find SixAxis controller\n");
        return 1;
    }
    return 0;
}
