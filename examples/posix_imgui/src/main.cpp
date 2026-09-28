// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2014 BlueKitchen GmbH
// Copyright 2026 Ricardo Quesada
// http://retro.moe/unijoysticle2

/**
 * @file main.cpp
 * @brief Application entry point for the Bluepad32 POSIX Dear ImGui Controller Tester.
 *
 * Architectural Role & Lifecycle Orchestration:
 *   Combines BTstack's POSIX `libusb` USB HCI transport setup (adapted from
 *   `examples/posix/src/main.c`) with a GLFW + OpenGL3 + Dear ImGui desktop window
 *   lifecycle across two cooperating threads:
 *
 *   1. CLI Flag Parsing (Headless-Safe):
 *      Parses command-line flags (`-h/--help`, `-u/--usbpath`, `-l/--logfile`,
 *      `-r/--reset-tlv`, `-b/--ble`, `-d/--delete`, `-e/--enhanced`) BEFORE initializing
 *      GLFW or BTstack so `--help` / `-h` exits with `EXIT_SUCCESS` (`0`) even in
 *      headless environments without an X11/Wayland display or USB dongle.
 *   2. BTstack & Bluepad32 Initialization (Main Thread, Pre-Spawn):
 *      Initializes BTstack memory pools, POSIX run loop, USB path filter, PacketLogger
 *      HCI trace (`/tmp/hci_dump*.pklg`), `SIGINT` signal mask (`btstack_signal_register_callback`),
 *      Realtek/Zephyr chipset drivers, and registers `get_posix_imgui_platform()` via
 *      `uni_platform_set_custom()` + `uni_init()`.
 *   3. GLFW + OpenGL3 + Dear ImGui Initialization (Thread 1 / Main Thread):
 *      Creates the desktop window, OpenGL 3.0+ context, and Dear ImGui context (with
 *      `ImGuiConfigFlags_NavEnableGamepad` deliberately omitted so gamepad buttons never
 *      steal ImGui widget focus), and uploads all 44 controller PNG sprites via
 *      `DemoScene::OnCreate()`.
 *   4. Background BTstack Worker Thread (Thread 2):
 *      Spawns `bt_thread` running `btstack_run_loop_execute()`.
 *   5. Clean Asynchronous Teardown:
 *      On window close or `Ctrl-C` (`SIGINT`), signals the BTstack thread via
 *      `posix_imgui_request_shutdown()`, powers down HCI (`HCI_POWER_OFF`) or triggers
 *      immediate run-loop exit if no dongle was initialized, joins `bt_thread`, unloads
 *      OpenGL textures, and destroys the Dear ImGui and GLFW contexts cleanly.
 */

#define BTSTACK_FILE__ "main.cpp"

// Standard C/C++ headers MUST be included outside and before `extern "C"` so C++
// standard library declarations are never trapped inside C linkage.
#include <getopt.h>
#include <signal.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

extern "C" {
#include "btstack_config.h"

#include "ble/le_device_db_tlv.h"
#include "bluetooth_company_id.h"
#include "btstack_audio.h"
#include "btstack_chipset_realtek.h"
#include "btstack_chipset_zephyr.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "btstack_run_loop_posix.h"
#include "btstack_signal.h"
#include "btstack_stdin.h"
#include "btstack_tlv_posix.h"
#include "classic/btstack_link_key_db_tlv.h"
#include "hal_led.h"
#include "hci.h"
#include "hci_dump.h"
#include "hci_dump_posix_fs.h"
#include "hci_transport.h"
#include "hci_transport_usb.h"

#include <uni.h>
#include "sdkconfig.h"
}

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

#include "demo_scene.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "posix_imgui_platform.h"

namespace {

constexpr uint16_t kUsbVendorIdRealtek = 0x0bda;
constexpr int kUsbMaxPathLen = 7;
constexpr const char* kTlvDbPathPrefix = "/tmp/btstack_";
constexpr const char* kTlvDbPathPostfix = ".tlv";

char tlv_db_path[100];
bool tlv_reset = false;
const btstack_tlv_t* tlv_impl = nullptr;
btstack_tlv_posix_t tlv_context = {};
btstack_tlv_posix_t* tlv_context_ptr = nullptr;
bd_addr_t local_addr = {};
bd_addr_t static_address = {};
int using_static_address = 0;

btstack_packet_callback_registration_t hci_event_callback_registration = {};

std::atomic<bool> shutdown_triggered{false};
std::atomic<bool> g_should_quit{false};
std::atomic<bool> g_glfw_initialized{false};
std::atomic<bool> g_bt_thread_exited{false};

void create_instance_tlv() {
    tlv_impl = btstack_tlv_posix_init_instance(&tlv_context, tlv_db_path);
    btstack_tlv_set_instance(tlv_impl, &tlv_context);
    tlv_context_ptr = &tlv_context;
}

void get_or_create_instance_tlv() {
    void* raw_ctx = nullptr;
    btstack_tlv_get_instance(&tlv_impl, &raw_ctx);
    tlv_context_ptr = static_cast<btstack_tlv_posix_t*>(raw_ctx);
    if (!tlv_impl || !tlv_context_ptr) {
        create_instance_tlv();
    }
}

void local_version_information_handler(uint8_t* packet) {
    std::printf("Local version information:\n");
    uint16_t hci_version = packet[6];
    uint16_t hci_revision = little_endian_read_16(packet, 7);
    uint16_t lmp_version = packet[9];
    uint16_t manufacturer = little_endian_read_16(packet, 10);
    uint16_t lmp_subversion = little_endian_read_16(packet, 12);
    std::printf("- HCI Version    0x%04x\n", hci_version);
    std::printf("- HCI Revision   0x%04x\n", hci_revision);
    std::printf("- LMP Version    0x%04x\n", lmp_version);
    std::printf("- LMP Subversion 0x%04x\n", lmp_subversion);
    std::printf("- Manufacturer   0x%04x\n", manufacturer);
    switch (manufacturer) {
        case BLUETOOTH_COMPANY_ID_THE_LINUX_FOUNDATION:
            std::printf("- Linux Foundation - assume Zephyr hci_usb firmware running on nRF52xx\n");
            hci_set_chipset(btstack_chipset_zephyr_instance());
            sm_init();
            break;
        default:
            break;
    }
}

/**
 * @brief BTstack HCI event packet handler (executed on Thread 2: BTstack Run-Loop Thread).
 */
void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) {
        return;
    }

    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_TRANSPORT_USB_INFO: {
            uint8_t usb_path_len = hci_event_transport_usb_info_get_path_len(packet);
            const uint8_t* usb_path = hci_event_transport_usb_info_get_path(packet);
            uint16_t product_id = hci_event_transport_usb_info_get_product_id(packet);
            uint16_t vendor_id = hci_event_transport_usb_info_get_vendor_id(packet);
            std::printf("USB device 0x%04x/0x%04x, path: ", vendor_id, product_id);
            for (uint8_t i = 0; i < usb_path_len; i++) {
                if (i) {
                    std::printf("-");
                }
                std::printf("%02x", usb_path[i]);
            }
            std::printf("\n");

            if (vendor_id == kUsbVendorIdRealtek) {
                std::printf("Realtek Controller - requires firmware and config download\n");
                btstack_chipset_realtek_set_product_id(product_id);
                hci_set_chipset(btstack_chipset_realtek_instance());
                hci_enable_custom_pre_init();
            }
            break;
        }
        case BTSTACK_EVENT_STATE:
            switch (btstack_event_state_get_state(packet)) {
                case HCI_STATE_WORKING:
                    gap_local_bd_addr(local_addr);
                    if (using_static_address) {
                        std::memcpy(local_addr, static_address, 6);
                    }
                    btstack_strcpy(tlv_db_path, sizeof(tlv_db_path), kTlvDbPathPrefix);
                    btstack_strcat(tlv_db_path, sizeof(tlv_db_path), bd_addr_to_str_with_delimiter(local_addr, '-'));
                    btstack_strcat(tlv_db_path, sizeof(tlv_db_path), kTlvDbPathPostfix);
                    if (tlv_reset) {
                        int rc = ::unlink(tlv_db_path);
                        if (rc == 0) {
                            std::printf(", reset ok");
                        } else {
                            std::printf(", reset failed with result = %d", rc);
                        }
                    }
                    std::printf("\n");
                    get_or_create_instance_tlv();
                    if (tlv_context_ptr != nullptr) {
                        std::printf("TLV path: %s\n", tlv_context_ptr->db_path);
#ifdef ENABLE_CLASSIC
                        hci_set_link_key_db(btstack_link_key_db_tlv_get_instance(tlv_impl, tlv_context_ptr));
#endif
#ifdef ENABLE_BLE
                        le_device_db_tlv_configure(tlv_impl, tlv_context_ptr);
#endif
                    }
                    std::printf("BTstack up and running on %s.\n", bd_addr_to_str(local_addr));
                    break;
                case HCI_STATE_OFF: {
                    // Why: `btstack_tlv_posix_deinit(self)` dereferences `self` without a
                    // null check. If the user closes the GUI window when no USB Bluetooth
                    // dongle is attached, `HCI_STATE_WORKING` was never reached and
                    // `tlv_context_ptr` may be null. Always retrieve the active TLV instance
                    // and null-check before deinitializing.
                    void* raw_ctx = nullptr;
                    btstack_tlv_get_instance(&tlv_impl, &raw_ctx);
                    tlv_context_ptr = static_cast<btstack_tlv_posix_t*>(raw_ctx);
                    if (tlv_context_ptr != nullptr) {
                        btstack_tlv_posix_deinit(tlv_context_ptr);
                        tlv_context_ptr = nullptr;
                    }
                    if (!shutdown_triggered.load() && !posix_imgui_is_shutdown_requested()) {
                        break;
                    }
                    // Call `btstack_run_loop_trigger_exit()` instead of `exit(0)` so
                    // `btstack_run_loop_execute()` returns cleanly and the main thread can
                    // join `bt_thread` and tear down OpenGL/ImGui/GLFW resources.
                    log_info("HCI_STATE_OFF reached, triggering run loop exit.");
                    g_should_quit.store(true);
                    btstack_run_loop_trigger_exit();
                    break;
                }
                default:
                    break;
            }
            break;
        case HCI_EVENT_COMMAND_COMPLETE:
            switch (hci_event_command_complete_get_command_opcode(packet)) {
                case HCI_OPCODE_HCI_READ_LOCAL_VERSION_INFORMATION:
                    local_version_information_handler(packet);
                    break;
                case HCI_OPCODE_HCI_ZEPHYR_READ_STATIC_ADDRESS: {
                    const uint8_t* params = hci_event_command_complete_get_return_parameters(packet);
                    if (params[0] != 0) {
                        break;
                    }
                    if (size < 13) {
                        break;
                    }
                    reverse_48(&params[2], static_address);
                    gap_random_address_set(static_address);
                    using_static_address = 1;
                    break;
                }
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

void sigint_handler() {
    std::printf("CTRL-C - SIGINT received, shutting down...\n");
    log_info("sigint_handler: shutting down");
    shutdown_triggered.store(true);
    g_should_quit.store(true);
    posix_imgui_request_shutdown();
    // Wake the GLFW event loop immediately if it is sleeping or polling.
    if (g_glfw_initialized.load()) {
        glfwPostEmptyEvent();
    }
}

void glfw_error_callback(int error, const char* description) {
    std::fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

constexpr const char* kShortOptions = "hu:l:rb:de";

const struct option kLongOptions[] = {
    {"help", no_argument, nullptr, 'h'},      {"logfile", required_argument, nullptr, 'l'},
    {"reset-tlv", no_argument, nullptr, 'r'}, {"usbpath", required_argument, nullptr, 'u'},
    {"ble", required_argument, nullptr, 'b'}, {"delete", no_argument, nullptr, 'd'},
    {"enhanced", no_argument, nullptr, 'e'},  {nullptr, 0, nullptr, 0},
};

const char* const kHelpOptions[] = {
    "print (this) help.",
    "set file to store debug output and HCI trace.",
    "reset bonding information stored in TLV.",
    "set USB path to Bluetooth Controller.",
    "disable (0) or enable (1) BLE.",
    "delete stored bonding keys.",
    "enable enhanced mode.",
};

const char* const kOptionArgName[] = {
    "", "LOGFILE", "", "USBPATH", "0|1", "", "",
};

void usage(const char* name) {
    std::printf("usage:\n\t%s [options]\n", name);
    std::printf("valid options:\n");
    for (unsigned int i = 0; kLongOptions[i].name != nullptr; i++) {
        std::printf("--%-10s| -%c  %-10s\t\t%s\n", kLongOptions[i].name, kLongOptions[i].val, kOptionArgName[i],
                    kHelpOptions[i]);
    }
}

}  // namespace

extern "C" void hal_led_toggle(void) {
    // No-op on POSIX ImGui desktop target.
}

int main(int argc, const char* argv[]) {
    uint8_t usb_path[kUsbMaxPathLen] = {};
    int usb_path_len = 0;
    const char* usb_path_string = nullptr;
    const char* log_file_path = nullptr;

    // 1. Parse command-line arguments BEFORE initializing GLFW or BTstack so
    // --help / -h succeeds with EXIT_SUCCESS (0) in headless environments.
    while (true) {
        int c = getopt_long(argc, const_cast<char* const*>(argv), kShortOptions, kLongOptions, nullptr);
        if (c < 0) {
            break;
        }
        switch (c) {
            case 'u':
                usb_path_string = optarg;
                break;
            case 'l':
                log_file_path = optarg;
                break;
            case 'r':
                tlv_reset = true;
                break;
            case 'b':
            case 'd':
            case 'e':
                // Handled inside posix_imgui_platform.cpp (posix_imgui_init)
                break;
            case 'h':
                usage(argv[0]);
                return EXIT_SUCCESS;
            case '?':
            default:
                usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    if (usb_path_string != nullptr) {
        std::printf("Specified USB Path: ");
        const char* cursor = usb_path_string;
        while (usb_path_len < kUsbMaxPathLen) {
            char* delimiter = nullptr;
            long port = std::strtol(cursor, &delimiter, 16);
            usb_path[usb_path_len] = static_cast<uint8_t>(port);
            usb_path_len++;
            std::printf("%02x ", static_cast<unsigned int>(port & 0xff));
            if (!delimiter || (*delimiter != ':' && *delimiter != '-')) {
                break;
            }
            cursor = delimiter + 1;
        }
        std::printf("\n");
    }

    // 2. Initialize BTstack memory, POSIX run loop, and SIGINT mask on the main
    // thread BEFORE spawning child threads or initializing GLFW.
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_posix_get_instance());

    if (usb_path_len > 0) {
        hci_transport_usb_set_path(usb_path_len, usb_path);
    }

    char pklg_path[100];
    if (log_file_path == nullptr) {
        btstack_strcpy(pklg_path, sizeof(pklg_path), "/tmp/hci_dump");
        if (usb_path_len > 0 && usb_path_string != nullptr) {
            btstack_strcat(pklg_path, sizeof(pklg_path), "_");
            btstack_strcat(pklg_path, sizeof(pklg_path), usb_path_string);
        }
        btstack_strcat(pklg_path, sizeof(pklg_path), ".pklg");
        log_file_path = pklg_path;
    }

    hci_dump_posix_fs_open(log_file_path, HCI_DUMP_PACKETLOGGER);
    const hci_dump_t* hci_dump_impl = hci_dump_posix_fs_get_instance();
    hci_dump_init(hci_dump_impl);
    std::printf("Packet Log: %s\n", log_file_path);

    hci_init(hci_transport_usb_instance(), nullptr);

#ifdef HAVE_PORTAUDIO
    btstack_audio_sink_set_instance(btstack_audio_portaudio_sink_get_instance());
    btstack_audio_source_set_instance(btstack_audio_portaudio_source_get_instance());
#endif

    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // Register SIGINT handler before spawning bt_thread or GLFW threads so all
    // threads inherit the blocked SIGINT signal mask.
    btstack_signal_register_callback(SIGINT, &sigint_handler);

    uint16_t realtek_num_controllers = btstack_chipset_realtek_get_num_usb_controllers();
    for (uint16_t i = 0; i < realtek_num_controllers; i++) {
        uint16_t vendor_id = 0;
        uint16_t product_id = 0;
        btstack_chipset_realtek_get_vendor_product_id(i, &vendor_id, &product_id);
        hci_transport_usb_add_device(vendor_id, product_id);
    }

    uni_platform_set_custom(get_posix_imgui_platform());
    uni_init(argc, argv);

    // 3. Initialize GLFW + OpenGL3 + Dear ImGui on the main thread
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialize GLFW.\n");
        return EXIT_FAILURE;
    }
    g_glfw_initialized.store(true);

#if defined(__APPLE__)
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    float main_scale = 1.0f;
    if (GLFWmonitor* primary_monitor = glfwGetPrimaryMonitor()) {
        main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(primary_monitor);
        if (main_scale <= 0.0f) {
            main_scale = 1.0f;
        }
    }

    GLFWwindow* window = glfwCreateWindow(static_cast<int>(1280.0f * main_scale), static_cast<int>(800.0f * main_scale),
                                          "Bluepad32 POSIX Controller Tester", nullptr, nullptr);
    if (window == nullptr) {
        std::fprintf(stderr, "Failed to create GLFW window.\n");
        g_glfw_initialized.store(false);
        glfwTerminate();
        return EXIT_FAILURE;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    // Enable keyboard navigation, but explicitly DO NOT enable ImGuiConfigFlags_NavEnableGamepad
    // so gamepad buttons/sticks never hijack Dear ImGui focus or tabs during controller testing.
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    DemoScene demo_scene;

    // 4. Spawn the BTstack worker thread
    std::thread bt_thread([]() {
        btstack_run_loop_execute();
        g_bt_thread_exited.store(true);
    });

    // 5. Main GLFW + Dear ImGui render loop
    const ImVec4 clear_color(0.08f, 0.10f, 0.14f, 1.00f);
    while (!glfwWindowShouldClose(window) && !g_should_quit.load() && !posix_imgui_is_shutdown_requested()) {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        demo_scene.DoFrame();

        ImGui::Render();
        int display_w = 0;
        int display_h = 0;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w,
                     clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // 6. Clean asynchronous shutdown of BTstack worker thread
    shutdown_triggered.store(true);
    posix_imgui_request_shutdown();

    constexpr int kMaxWaitIterations = 150;  // 150 * 10ms = 1500ms bounded wait
    for (int i = 0; i < kMaxWaitIterations && !g_bt_thread_exited.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!g_bt_thread_exited.load()) {
        btstack_run_loop_trigger_exit();
    }
    if (bt_thread.joinable()) {
        bt_thread.join();
    }

    // 7. Tear down Dear ImGui and GLFW
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    g_glfw_initialized.store(false);
    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}
