# Select the platform to build for.
# e.g: "unijoysticle", "airlift", etc.
ifeq ($(PLATFORM),)
  $(error You must provide a PLATFORM parameter. E.g: "PLATFORM=unijoysticle make -j")
else
  ifeq ($(wildcard configs/$(PLATFORM)-config.mk),)
    $(error Invalid PLATFORM specified)
  endif
endif

include configs/$(PLATFORM)-config.mk

# Bluepad32 firmware
PROJECT_NAME := bluepad32-${PLATFORM}

include $(IDF_PATH)/make/project.mk

