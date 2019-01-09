PROJECT_NAME := client
CFLAGS += -Wall -Wextra
EXTRA_COMPONENT_DIRS := $(abspath .)
include $(IDF_PATH)/make/project.mk
