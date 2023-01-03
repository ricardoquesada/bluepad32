if("${IDF_VERSION_MAJOR}" GREATER_EQUAL 5)
idf_component_register(SRCS "cmd_system.c"
                    INCLUDE_DIRS .
                    REQUIRES console spi_flash driver)
endif()
