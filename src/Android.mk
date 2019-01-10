#
# Copyright (C) 2019 The Android-x86 Open Source Project
#
# Licensed under the GNU Lesser General Public License Version 2.1.
# You may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.gnu.org/licenses/lgpl-2.1.html
#

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := makeguids
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_CFLAGS := -DEFIVAR_BUILD_ENVIRONMENT
LOCAL_SRC_FILES := guid.c makeguids.c
LOCAL_LDLIBS := -ldl
include $(BUILD_HOST_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_MODULE := libefivar
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LIBEFIBOOT_SOURCES := \
	crc32.c \
	creator.c \
	disk.c \
	gpt.c \
	loadopt.c \
	path-helpers.c \
	$(notdir $(wildcard $(LOCAL_PATH)/linux*.c))

LIBEFIVAR_SOURCES := \
	dp.c \
	dp-acpi.c \
	dp-hw.c \
	dp-media.c \
	dp-message.c \
	efivarfs.c \
	error.c \
	export.c \
	guid.c \
	guids.S \
	lib.c \
	vars.c

LOCAL_SRC_FILES := $(LIBEFIBOOT_SOURCES) $(LIBEFIVAR_SOURCES)
LOCAL_CFLAGS := -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -std=gnu11
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_C_INCLUDES) $(LOCAL_C_INCLUDES)/efivar $(local-generated-sources-dir)
LIBEFIVAR_GUIDS_H := $(local-generated-sources-dir)/efivar/efivar-guids.h
LOCAL_GENERATED_SOURCES := $(LIBEFIVAR_GUIDS_H) $(local-generated-sources-dir)/guid-symbols.c
$(LIBEFIVAR_GUIDS_H): PRIVATE_CUSTOM_TOOL = $^ $(addprefix $(dir $(@D)),guids.bin names.bin guid-symbols.c efivar/efivar-guids.h)
$(LIBEFIVAR_GUIDS_H): $(BUILD_OUT_EXECUTABLES)/makeguids $(LOCAL_PATH)/guids.txt
	$(transform-generated-source)
$(lastword $(LOCAL_GENERATED_SOURCES)): $(LIBEFIVAR_GUIDS_H)

include $(BUILD_STATIC_LIBRARY)
