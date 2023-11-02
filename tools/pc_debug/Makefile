# Makefile for libusb based examples
BTSTACK_ROOT = 	../../external/btstack
BLUEPAD32_ROOT = ../../src/components/bluepad32

CORE += main.c btstack_stdin_posix.c btstack_tlv_posix.c hci_dump_posix_fs.c

COMMON += hci_transport_h2_libusb.c btstack_run_loop_posix.c le_device_db_tlv.c btstack_link_key_db_tlv.c wav_util.c btstack_network_posix.c
COMMON += gatt_client.c att_dispatch.c hids_client.c device_information_service_client.c scan_parameters_service_client.c hid_host.c
COMMON += btstack_audio_portaudio.c btstack_chipset_zephyr.c rijndael.c hid_device.c

include ${BTSTACK_ROOT}/example/Makefile.inc

CFLAGS += -DCONFIG_BLUEPAD32_PLATFORM_PC_DEBUG

CC ?= clang
CFLAGS  += -g -std=c99 -Wall -Wmissing-prototypes -Wstrict-prototypes -Wshadow -Wunused-parameter -Wredundant-decls -Wsign-compare -Wswitch-default
CFLAGS  += -Wextra
CFLAGS  += -fstack-protector-all -Wstack-protector --param ssp-buffer-size=4
CFLAGS  += -O2 -D_FORTIFY_SOURCE=2
# CFLAGS += -Werror
# CFLAGS += -pedantic

# only LLVM
# CFLAGS += -Wnewline-eof
# CFLAGS += -Wc11-extensions
# CFLAGS += -Wgnu-empty-initializer

CFLAGS += -I${BTSTACK_ROOT}/platform/posix    \
		  -I${BTSTACK_ROOT}/platform/embedded \
		  -I${BTSTACK_ROOT}/3rd-party/tinydir \
		  -I${BTSTACK_ROOT}/3rd-party/rijndael \
		  -I${BTSTACK_ROOT}/chipset/zephyr

VPATH += ${BTSTACK_ROOT}/3rd-party/rijndael
VPATH += ${BTSTACK_ROOT}/platform/embedded
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/platform/libusb
VPATH += ${BTSTACK_ROOT}/chipset/csr
VPATH += ${BTSTACK_ROOT}/chipset/zephyr

# use pkg-config
CFLAGS  += $(shell pkg-config libusb-1.0 --cflags) -I${BLUEPAD32_ROOT}/include
LDFLAGS += $(shell pkg-config libusb-1.0 --libs)

#EXAMPLES = ${EXAMPLES_DUAL_MODE} ${EXAMPLES_LE_ONLY}
#EXAMPLES = ${EXAMPLES_GENERAL} ${EXAMPLES_CLASSIC_ONLY} ${EXAMPLES_LE_ONLY} ${EXAMPLES_DUAL_MODE}
#EXAMPLES += pan_lwip_http_server
#EXAMPLES += csr_set_bd_addr

csr_set_bd_addr: ${CORE_OBJ} ${COMMON_OBJ} btstack_chipset_csr.o csr_set_bd_addr.o
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

# use pkg-config for portaudio
# CFLAGS  += $(shell pkg-config portaudio-2.0 --cflags) -DHAVE_PORTAUDIO
# LDFLAGS += $(shell pkg-config portaudio-2.0 --libs)

# hard coded flags for portaudio in /usr/local/lib
# CFLAGS  += -I/usr/local/include -DHAVE_PORTAUDIO
# LDFLAGS += -L/sw/lib -lportaudio -Wl,-framework,CoreAudio -Wl,-framework,AudioToolbox -Wl,-framework,AudioUnit -Wl,-framework,Carbon

clean_src:
	rm -rf ${BTSTACK_ROOT}/src/*.o
	rm -rf ${BTSTACK_ROOT}/src/classic/*.o
	rm -rf ${BTSTACK_ROOT}/src/ble/*.o
	rm -rf ${BTSTACK_ROOT}/platform/embedded/*.o

clean:
	-rm -rf ${BLUEPAD32_ROOT}/*.o
	-rm -rf *.o
	-rm -f bluepad32

bluepad32: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} ${SDP_CLIENT} btstack_ring_buffer.o btstack_hid_parser.o \
	${BLUEPAD32_ROOT}/uni_balance_board.o \
	${BLUEPAD32_ROOT}/uni_bt.o \
	${BLUEPAD32_ROOT}/uni_bt_allowlist.o \
	${BLUEPAD32_ROOT}/uni_bt_bredr.o \
	${BLUEPAD32_ROOT}/uni_bt_conn.o \
	${BLUEPAD32_ROOT}/uni_bt_le.o \
	${BLUEPAD32_ROOT}/uni_bt_sdp.o \
	${BLUEPAD32_ROOT}/uni_bt_setup.o \
	${BLUEPAD32_ROOT}/uni_circular_buffer.o \
	${BLUEPAD32_ROOT}/uni_controller.o \
	${BLUEPAD32_ROOT}/uni_gamepad.o \
	${BLUEPAD32_ROOT}/uni_hci_cmd.o \
	${BLUEPAD32_ROOT}/uni_hid_device.o \
	${BLUEPAD32_ROOT}/uni_hid_parser.o \
	${BLUEPAD32_ROOT}/uni_hid_parser_8bitdo.o \
	${BLUEPAD32_ROOT}/uni_hid_parser_android.o \
	${BLUEPAD32_ROOT}/uni_hid_parser_ds3.o \
	${BLUEPAD32_ROOT}/uni_hid_parser_ds4.o \
	${BLUEPAD32_ROOT}/uni_hid_parser_ds5.o \
	${BLUEPAD32_ROOT}/uni_hid_parser_generic.o \
	${BLUEPAD32_ROOT}/uni_hid_parser_icade.o \
	${BLUEPAD32_ROOT}/uni_hid_parser_keyboard.o \
	${BLUEPAD32_ROOT}/uni_hid_parser_mouse.o \
	${BLUEPAD32_ROOT}/uni_hid_parser_nimbus.o \
	${BLUEPAD32_ROOT}/uni_hid_parser_ouya.o \
	${BLUEPAD32_ROOT}/uni_hid_parser_psmove.o \
	${BLUEPAD32_ROOT}/uni_hid_parser_smarttvremote.o \
	${BLUEPAD32_ROOT}/uni_hid_parser_steam.o \
	${BLUEPAD32_ROOT}/uni_hid_parser_switch.o \
	${BLUEPAD32_ROOT}/uni_hid_parser_wii.o \
	${BLUEPAD32_ROOT}/uni_hid_parser_xboxone.o \
	${BLUEPAD32_ROOT}/uni_joystick.o \
	${BLUEPAD32_ROOT}/uni_keyboard.o \
	${BLUEPAD32_ROOT}/uni_log.o \
	${BLUEPAD32_ROOT}/uni_main.o \
	${BLUEPAD32_ROOT}/uni_mouse.o \
	${BLUEPAD32_ROOT}/uni_platform.o \
	${BLUEPAD32_ROOT}/uni_platform_pc_debug.o \
	${BLUEPAD32_ROOT}/uni_property.o \
	${BLUEPAD32_ROOT}/uni_property_mem.o \
	${BLUEPAD32_ROOT}/uni_utils.o \
	${BLUEPAD32_ROOT}/uni_virtual_device.o
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

all: bluepad32 ${EXAMPLES}
