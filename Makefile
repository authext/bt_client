PROJECT_NAME := client
CFLAGS += -Wall -Wextra
EXTRA_COMPONENT_DIRS := $(abspath ../a2dp_core) $(abspath .)
include $(IDF_PATH)/make/project.mk
