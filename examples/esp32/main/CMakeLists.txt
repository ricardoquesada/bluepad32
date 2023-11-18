set(srcs "main.c" "my_platform.c")

set(requires "bluepad32" "btstack")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "."
                    REQUIRES "${requires}")
