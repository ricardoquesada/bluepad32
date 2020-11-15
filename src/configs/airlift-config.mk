CFLAGS += -DUNI_PLATFORM_AIRLIFT

# Disable UART output to avoid conflict with TX/RX pins
# in all Airlift boards.
CFLAGS += -DUNI_UART_OUTPUT_DISABLE

