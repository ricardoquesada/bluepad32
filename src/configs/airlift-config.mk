CFLAGS += -DCONFIG_BLUEPAD32_PLATFORM_AIRLIFT

# Disable UART output to avoid conflict with TX/RX pins
# in all Airlift boards.
CFLAGS += -DCONFIG_BLUEPAD32_UART_OUTPUT_DISABLE

# Other platform specific defines could be added here (or in the platform file).
# E.g:
# CFLAGS += -DPLAT_AIRLIFT_XXXX
