# Makefile for libusb based examples
BTSTACK_ROOT = 	${HOME}/src/btstack

CORE += main.c btstack_stdin_posix.c btstack_tlv_posix.c

COMMON += hci_transport_h2_libusb.c btstack_run_loop_posix.c le_device_db_tlv.c btstack_link_key_db_tlv.c wav_util.c btstack_network_posix.c
COMMON += btstack_audio_portaudio.c btstack_chipset_zephyr.c rijndael.c

include ${BTSTACK_ROOT}/example/Makefile.inc

CC=clang
CFLAGS  += -g -std=c99 -Wall -Wmissing-prototypes -Wstrict-prototypes -Wshadow -Wunused-parameter -Wredundant-decls -Wsign-compare
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
CFLAGS  += $(shell pkg-config libusb-1.0 --cflags) -I../main
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
	-rm -rf ../main/*.o
	-rm -rf *.o
	-rm -f sixaxispairer unijoysticle ble_host

unijoysticle: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} ${SDP_CLIENT} btstack_ring_buffer.o btstack_hid_parser.o \
	../main/uni_main.o ../main/uni_bluetooth.o ../main/uni_hid_device.o  ../main/uni_hid_parser.o ../main/uni_gamepad.o \
	../main/uni_hid_parser_android.o ../main/uni_hid_parser_icade.o ../main/uni_hid_parser_ouya.o \
	../main/uni_hid_parser_xboxone.o ../main/uni_hid_parser_nimbus.o ../main/uni_hid_parser_smarttvremote.o \
	../main/uni_hid_parser_ds3.o ../main/uni_hid_parser_ds4.o ../main/uni_hid_parser_8bitdo.o ../main/uni_hid_parser_generic.o \
	../main/uni_hid_parser_wii.o ../main/uni_hid_parser_switch.o \
	../main/uni_circular_buffer.o \
	uni_platform_pc.o
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

ble_host:  ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} ${SDP_CLIENT} btstack_ring_buffer.o btstack_hid_parser.o \
	gatt_client.o att_dispatch.o att_server.o att_db.o \
	ble_host.o

sixaxispairer: sixaxispairer.o
	echo "Compiling sixaxis pairer"
	echo "Make sure that 'libhidapi-dev' is installed"
	${CC} $^ -lhidapi-libusb -o $@

all: unijoysticle ble_host ${EXAMPLES}
