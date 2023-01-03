if("${IDF_VERSION_MAJOR}" LESS 5)
idf_component_register(SRCS "cmd_nvs.c"
                    INCLUDE_DIRS .
                    REQUIRES console nvs_flash)
endif()
