LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := dns.c
LOCAL_C_INCLUDES := $(KERNEL_HEADERS)
LOCAL_MODULE = dns
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := text2byte.c

LOCAL_C_INCLUDES := $(KERNEL_HEADERS)
LOCAL_MODULE = text2byte
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := rawsend.c lib/netutils.c

LOCAL_C_INCLUDES := $(KERNEL_HEADERS) $(LOCAL_PATH)/include
LOCAL_MODULE = rawsend 
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := rawrecv.c lib/netutils.c

LOCAL_C_INCLUDES := $(KERNEL_HEADERS) $(LOCAL_PATH)/include
LOCAL_MODULE = rawrecv
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := igmptools.c lib/netutils.c

LOCAL_C_INCLUDES := $(KERNEL_HEADERS) $(LOCAL_PATH)/include
LOCAL_MODULE = igmptools
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := vconfig.c
LOCAL_C_INCLUDES := $(KERNEL_HEADERS)
LOCAL_MODULE = vconfig
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := msend.c

LOCAL_C_INCLUDES := $(KERNEL_HEADERS)
LOCAL_MODULE = msend
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := mrecv.c 

LOCAL_C_INCLUDES := $(KERNEL_HEADERS)
LOCAL_SHARED_LIBRARIES := libc libcutils libnetutils
LOCAL_MODULE = mrecv 
LOCAL_MODULE_TAGS := optional 
include $(BUILD_EXECUTABLE)

