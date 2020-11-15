# Unijoysticle 2 firmware
PROJECT_NAME := firmware

include $(IDF_PATH)/make/project.mk

# Select the platform to build for.
# e.g: "c64", "airlift", etc.
ifeq ($(PLATFORM),)
  $(error You must provide a PLATFORM parameter. E.g: "PLATFORM=c64 make -j")
else
  ifeq ($(wildcard platforms/$(PLATFORM)-config.mk),)
    $(error Invalid PLATFORM specified)
  endif
endif

include platforms/$(PLATFORM)-config.mk

