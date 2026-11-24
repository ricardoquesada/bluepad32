# AGENTS.md

Guidelines and codebase conventions for AI coding agents working on Bluepad32.

## Scope & Language

- Core library lives under `src/components/bluepad32/` and targets **C11** (`set(CMAKE_C_STANDARD 11)` in `tests/CMakeLists.txt`). Because ESP-IDF v6.1 builds with `-std=gnu23`, keep code valid C11 while avoiding C23 keyword collisions (for example `nullptr`, `constexpr`, `typeof`).
- Host unit tests live under `tests/` and use CMake + CTest.
- Do not modify `external/` (including `external/btstack`), vendored upstream ESP-IDF components (`src/components/cmd_nvs/`, `src/components/cmd_system/`), `btstack_config.h`, or generated `*.gatt.h` files unless explicitly requested.

## Core Style Baseline: Zephyr RTOS C (with Four Bluepad32 Exceptions)

Follow **Zephyr RTOS C coding guidelines** (Linux kernel C + MISRA-C:2012 subset) for all embedded C code, with **four explicit formatting exceptions**:

1. **Indentation & Line Width:** **4 spaces** and **120 columns** (instead of Zephyr's 8-space tabs / 100 columns), enforced by `.clang-format`.
2. **Comment Syntax:** **`//` comments are allowed** alongside `/* ... */` and Doxygen `/** ... */`. Never convert existing `/* ... */` comments to `//`.
3. **Pointer Alignment:** **Left-aligned** (`int* ptr`, `uni_hid_device_t* d`), instead of Zephyr's right-aligned (`int *ptr`), enforced by `.clang-format` (`BasedOnStyle: Chromium`).
4. **Brace, Switch & Macro Newline Layout:** Keep attached function braces (`BreakBeforeBraces: Attach`, e.g. `void foo(void) {`), indented `case:` labels (`IndentCaseLabels: true`), left-aligned macro continuation backslashes (`AlignEscapedNewlines: Left`), and **unspecified braces on single-line control statements** (both braced and unbraced single-line `if`/`else`/`for`/`while` bodies are accepted; `InsertBraces` and `RemoveBracesLLVM` are both unset in `.clang-format` — do not add or remove braces solely for style), instead of Zephyr's `Linux` braces, unindented `case:`, right-aligned `\`, and mandatory single-line braces.

For everything else, follow Zephyr conventions:

- **`.clang-format` enforcement:** `.clang-format` enforces `AllowShortFunctionsOnASingleLine: Empty`, `RemoveSemicolon: true`, `InsertNewlineAtEOF: true`, and `SeparateDefinitionBlocks: Always` (while leaving single-line control-statement `{ ... }` unspecified and keeping `AllowShortIfStatementsOnASingleLine: Never` / `AllowShortLoopsOnASingleLine: false`).
- **`switch` exhaustiveness:** Always include a `default:` branch ending with `break;` or `return` (`-Wswitch-default -Werror`).
- **Error handling & cleanup:** Check every returned error code (MISRA Dir 4.7). Use explicit `uni_error_t` / error status returns and single-exit `goto` cleanup when a function manages multiple resources or exit paths.
- **Static allocation & types:** Prefer static / fixed-pool allocation over `malloc`/`free` and verify sizes at compile time with `_Static_assert`. Use fixed-width `<stdint.h>` types (`uint8_t`, `int32_t`, `size_t`) wherever width or signedness matters (MISRA Dir 4.6). Avoid non-standard `0b...` binary literals in new code (prefer hex or `BIT()` / `GENMASK()`).
- **No dead code:** Do not introduce commented-out code blocks or `#if 0` blocks (MISRA Dir 4.4); remove dead code in sections you touch, and leave unrelated existing reference `#if 0` blocks untouched unless asked.
- **Concurrency & ISRs:** Never block in ISR context; use `IRAM_ATTR`, `*FromISR`, `volatile`, and marshal work to BTstack's main loop via `btstack_run_loop_execute_on_main_thread()`.

## `.c` File Layout (`SSSSS -> PPPP`)

Follow Linux kernel / Zephyr C bottom-up ordering so every `.c` file reads cleanly from helpers to exported entry points without redundant forward declarations:

1. **SPDX header & file overview comments**
2. **`#include`s** — matching `.h` header first (to verify self-containment and declare all public functions early), then C stdlib `<...>`, then SDK/BTstack headers, then project `"..."` headers.
3. **Local `#define`s, enums, structs, and typedefs**
4. **`static const` ROM lookup tables and file-scope `static` variables**
5. **Internal `static` helpers** — grouped together in bottom-up dependency order (leaf utilities first, higher-level helpers below).
6. **Public API functions** — all non-`static` functions (and driver/platform structs referencing them) at the bottom.

### Rules for Ordering & Forward Declarations

- **Never interleave `static` and public functions** (`S -> P -> S -> P`). Move file-scope typedefs and `static const` tables referenced by helpers above the `static` helpers section.
- **No redundant `static` forward declarations.** Reorder callees above callers instead.
- **Exceptions (keep forward declarations only for true cycles):** Only forward-declare `static` functions when a genuine FSM or timer callback mutual recursion exists (for example `process_fsm` in `uni_hid_parser_switch.c`, `wii_process_fsm` in `uni_hid_parser_wii.c`, `sw2_advance_command_state` in `uni_hid_parser_switch2.c`, `schedule_pump` in `uni_hid_parser_sinput.c`, and `on_rumble_*` timer callbacks in `uni_hid_parser_rumble.c`). Document why with a short `// Forward-declared because ...` or `/* Forward-declared because ... */` comment.

## Naming, Constness & Comments

- **Naming & Signatures:** Preserve `uni_*` module prefixes, `lower_snake_case` functions/variables, `UPPER_SNAKE_CASE` macros/enums, `d` for `uni_hid_device_t*`, public API signatures, struct layouts, HID offsets, and log messages.
- **Control Flow & Variables:** Flatten deep nesting with guard clauses / early returns, omit redundant `else` after `return` and empty `else {}` blocks, use C11 loop-scoped declarations (`for (int i = 0; ...)`), avoid variable shadowing and unused variables (`-Wunused-variable -Werror`), and fully parenthesize macro parameters and expressions.
- **Constness:** Mark read-only lookup tables and constants `static const` (or `static const char* const` for string arrays).
- **Comments & Documentation:** Keep existing Doxygen `/** ... */` blocks and existing `/* ... */` comments intact. Capitalize proper nouns and acronyms properly in comments (`UART`, `SPI`, `USB`, `BLE`, `HID`, `GATT`, `SDP`, `CMake`). Both `/* ... */` and `//` are permitted.

## Codebase-Specific Pitfalls

- **BTstack non-`const` APIs:** Many BTstack functions (e.g., `gatt_client_write_value_of_characteristic*`, `gatt_client_write_characteristic_descriptor_using_descriptor_handle`, `bd_addr_cmp`, `bd_addr_copy`, `des_iterator_*`, `l2cap_send`) accept non-`const` `uint8_t*` or `bd_addr_t`. Do not add `const` to buffers passed directly to BTstack APIs without verifying the prototype (`-Wdiscarded-qualifiers` fails `-Werror`).
- **`uni_controller_list.h` needs `NULL`:** Include `<stddef.h>` before `uni_controller_list.h` when removing `<stdio.h>`.
- **Non-POSIX files are NOT compiled by host tests:** The 17 ESP32/Pico-only `.c` files (`uni_gpio.c`, `uni_mouse_quadrature.c`, `arch/*_esp32.c`, `arch/*_pico.c`, `platform/uni_platform_unijoysticle*.c`) are excluded from `tests/build`. Be extra conservative when editing them:
  - Do not remove `#include`s unless 100% certain across all `#ifdef` branches.
  - Do not change non-`static` linkage on globals or functions without grepping the entire repository first.
  - Manually verify every reordered helper sees all needed headers, macros, globals, and `static` callees above it.

## Verification

`tests/CMakeLists.txt` builds with `-std=gnu11` (C11), `-Wall -Wextra -Wno-sign-compare -Wstrict-prototypes -Wunused-variable -Wswitch-default -Werror`, and UBSan enabled. CI checks formatting via `pre-commit` (`.pre-commit-config.yaml`) across `src/`, `tests/`, `examples/`, and `tools/`.

After edits, run:

```sh
# Format modified files (never format btstack_config.h or *.gatt.h)
clang-format -i <modified-files...>

# Build and run host unit tests
cmake -S tests -B tests/build
cmake --build tests/build -j
ctest --test-dir tests/build --output-on-failure

# Build ESP32 firmware when editing ESP32-only files (uni_gpio.c,
# uni_mouse_quadrature.c, arch/*_esp32.c, platform/uni_platform_unijoysticle*.c)
idf.py -C src build

# Verify formatting across all repo C/C++ sources (matching .pre-commit-config.yaml exclusions)
git ls-files '*.c' '*.h' '*.cpp' '*.cc' '*.hpp' '*.cxx' \
  | grep -Ev '^(external/|src/components/cmd_nvs/|src/components/cmd_system/)|\.gatt\.h$|btstack_config\.h$' \
  | sort \
  | xargs clang-format --dry-run --Werror
```
