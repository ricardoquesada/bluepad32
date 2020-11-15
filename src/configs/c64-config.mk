CFLAGS += -DUNI_PLATFORM_C64

# Enable C64 POT support. Currently not implemented
# Enabled if 1
CFLAGS += -DPLAT_C64_ENABLE_POT=0

# Enable Amiga/Atari ST mouse support.
# Enabled if 1
CFLAGS += -DPLAT_C64_ENABLE_MOUSE=1

# To be used with Unijoysticle devices that only connect to one port.
# For exmaple, the Amiga device made by https://arananet.net/
# These devices have only one port, so they only cannot use JOYSTICK_PORT_A,
# and have 3 buttons mapped.
# Enabled if 1
CFLAGS += -DPLAT_C64_SINGLE_PORT=0

