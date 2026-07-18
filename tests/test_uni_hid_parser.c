#include <assert.h>
#include <stdio.h>

#include "controller/uni_gamepad.h"
#include "hid_usage.h"
#include "parser/uni_hid_parser.h"

//
// Tests
//

static void test_process_axis(void) {
    printf("Testing uni_hid_parser_process_axis...\n");
    hid_globals_t globals = {0};
    globals.logical_minimum = 0;
    globals.logical_maximum = 255;
    globals.report_size = 8;  // Used when max == -1

    int32_t val;

    // Test center
    val = uni_hid_parser_process_axis(&globals, 128);
    // Range is 256. Center is 128. Value 128 -> centered 0 -> norm 0.
    // 128 - 256/2 - 0 = 0
    assert(val == 0);

    // Test min
    val = uni_hid_parser_process_axis(&globals, 0);
    // 0 - 128 - 0 = -128. -128 * 512 / 256 = -256. ?? Wait, AXIS_NORMALIZE_RANGE is 1024?
    // Let's check the code: centered * AXIS_NORMALIZE_RANGE / range
    // If range is 256 (0..255), centered is -128.
    // Normalized = -128 * 1024 / 256 = -512. Correct.
    assert(val == -512);

    // Test max
    val = uni_hid_parser_process_axis(&globals, 255);
    // 255 - 128 = 127. 127 * 1024 / 256 = 508.
    // Note: It doesn't reach 511 exactly because of integer division/range alignment.
    // Let's verify what the code actually does.
    // 127 * 4 = 508.
    assert(val == 508);

    // Test logical_maximum == -1 case (unsigned byte treated as signed in descriptor)
    globals.logical_maximum = -1;
    globals.report_size = 8;
    // Should behave like 0..255
    val = uni_hid_parser_process_axis(&globals, 128);
    assert(val == 0);

    // Test signed 8-bit range: -128 to 127
    globals.logical_minimum = -128;
    globals.logical_maximum = 127;
    // Range = 127 - (-128) + 1 = 256.

    // Center (0)
    val = uni_hid_parser_process_axis(&globals, 0);
    // 0 - 128 - (-128) = 0.
    assert(val == 0);

    // Min (-128)
    val = uni_hid_parser_process_axis(&globals, -128);
    // -128 - 128 - (-128) = -128. -128 * 4 = -512.
    // Wait, value passed is usually unsigned from parser, but if it's cast to int32_t...
    // The parser usually returns int32_t value.
    // Let's check with casting.
    assert(val == -512);

    printf("PASS\n");
}

static void test_process_pedal(void) {
    printf("Testing uni_hid_parser_process_pedal...\n");
    hid_globals_t globals = {0};
    globals.logical_minimum = 0;
    globals.logical_maximum = 255;
    globals.report_size = 8;

    int32_t val;

    // Min
    val = uni_hid_parser_process_pedal(&globals, 0);
    assert(val == 0);

    // Max
    val = uni_hid_parser_process_pedal(&globals, 255);
    // 255 * 1024 / 256 = 1020.
    assert(val == 1020);

    // Mid
    val = uni_hid_parser_process_pedal(&globals, 128);
    // 128 * 1024 / 256 = 512.
    assert(val == 512);

    printf("PASS\n");
}

static void test_hat_to_dpad(void) {
    printf("Testing uni_hid_parser_hat_to_dpad...\n");
    assert(uni_hid_parser_hat_to_dpad(0) == DPAD_UP);
    assert(uni_hid_parser_hat_to_dpad(1) == (DPAD_UP | DPAD_RIGHT));
    assert(uni_hid_parser_hat_to_dpad(2) == DPAD_RIGHT);
    assert(uni_hid_parser_hat_to_dpad(3) == (DPAD_RIGHT | DPAD_DOWN));
    assert(uni_hid_parser_hat_to_dpad(4) == DPAD_DOWN);
    assert(uni_hid_parser_hat_to_dpad(5) == (DPAD_DOWN | DPAD_LEFT));
    assert(uni_hid_parser_hat_to_dpad(6) == DPAD_LEFT);
    assert(uni_hid_parser_hat_to_dpad(7) == (DPAD_LEFT | DPAD_UP));
    assert(uni_hid_parser_hat_to_dpad(8) == 0);     // Center
    assert(uni_hid_parser_hat_to_dpad(0xFF) == 0);  // Null
    printf("\nPASS\n");
}

static void test_process_dpad(void) {
    printf("Testing uni_hid_parser_process_dpad...\n");
    uint8_t dpad = 0;

    uni_hid_parser_process_dpad(HID_USAGE_DPAD_UP, 1, &dpad);
    assert(dpad == DPAD_UP);

    uni_hid_parser_process_dpad(HID_USAGE_DPAD_DOWN, 1, &dpad);
    assert(dpad == (DPAD_UP | DPAD_DOWN));  // Both set now

    uni_hid_parser_process_dpad(HID_USAGE_DPAD_UP, 0, &dpad);
    assert(dpad == DPAD_DOWN);

    // Unsupported usage
    uni_hid_parser_process_dpad(0x9999, 1, &dpad);
    assert(dpad == DPAD_DOWN);  // Should be unchanged

    printf("PASS\n");
}

int main(int argc, char** argv) {
    test_process_axis();
    test_process_pedal();
    test_hat_to_dpad();
    test_process_dpad();

    printf("All uni_hid_parser tests passed!\n");
    return 0;
}
