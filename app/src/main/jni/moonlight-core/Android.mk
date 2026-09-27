# Android.mk for moonlight-core and binding
MY_LOCAL_PATH := $(call my-dir)

include $(call all-subdir-makefiles)

LOCAL_PATH := $(MY_LOCAL_PATH)

include $(CLEAR_VARS)
LOCAL_MODULE    := moonlight-core

PYROWAVE_COMMON_DIR := $(abspath $(LOCAL_PATH)/../../../../build/generated/pyrowave/common)

LOCAL_SRC_FILES := $(PYROWAVE_COMMON_DIR)/AudioStream.c \
                   $(PYROWAVE_COMMON_DIR)/ByteBuffer.c \
                   $(PYROWAVE_COMMON_DIR)/Connection.c \
                   $(PYROWAVE_COMMON_DIR)/ConnectionTester.c \
                   $(PYROWAVE_COMMON_DIR)/ControlStream.c \
                   $(PYROWAVE_COMMON_DIR)/FakeCallbacks.c \
                   $(PYROWAVE_COMMON_DIR)/InputStream.c \
                   $(PYROWAVE_COMMON_DIR)/LinkedBlockingQueue.c \
                   $(PYROWAVE_COMMON_DIR)/Misc.c \
                   $(PYROWAVE_COMMON_DIR)/Platform.c \
                   $(PYROWAVE_COMMON_DIR)/PlatformCrypto.c \
                   $(PYROWAVE_COMMON_DIR)/PlatformSockets.c \
                   $(PYROWAVE_COMMON_DIR)/RtpAudioQueue.c \
                   $(PYROWAVE_COMMON_DIR)/RtpVideoQueue.c \
                   $(PYROWAVE_COMMON_DIR)/RtspConnection.c \
                   $(PYROWAVE_COMMON_DIR)/RtspParser.c \
                   $(PYROWAVE_COMMON_DIR)/SdpGenerator.c \
                   $(PYROWAVE_COMMON_DIR)/SimpleStun.c \
                   $(PYROWAVE_COMMON_DIR)/VideoDepacketizer.c \
                   $(PYROWAVE_COMMON_DIR)/VideoStream.c \
                   moonlight-common-c/nanors/deps/obl/oblas_common.c \
                   moonlight-common-c/nanors/deps/obl/oblas_lite.c \
                   moonlight-common-c/nanors/rs.c \
                   moonlight-common-c/enet/callbacks.c \
                   moonlight-common-c/enet/compress.c \
                   moonlight-common-c/enet/host.c \
                   moonlight-common-c/enet/list.c \
                   moonlight-common-c/enet/packet.c \
                   moonlight-common-c/enet/peer.c \
                   moonlight-common-c/enet/protocol.c \
                   moonlight-common-c/enet/unix.c \
                   moonlight-common-c/enet/win32.c \
                   simplejni.c \
                   callbacks.c \
                   minisdl.c \


LOCAL_C_INCLUDES := $(LOCAL_PATH)/moonlight-common-c/enet/include \
                    $(LOCAL_PATH)/moonlight-common-c/nanors/deps/obl \
                    $(LOCAL_PATH)/moonlight-common-c/nanors \
                    $(PYROWAVE_COMMON_DIR) \

LOCAL_CFLAGS := -DHAS_SOCKLEN_T=1 -DLC_ANDROID -DHAVE_CLOCK_GETTIME=1

ifeq ($(NDK_DEBUG),1)
LOCAL_CFLAGS += -DLC_DEBUG
endif

LOCAL_LDLIBS := -llog

LOCAL_STATIC_LIBRARIES := libopus libcrypto cpufeatures
LOCAL_LDFLAGS += -Wl,--exclude-libs,ALL

LOCAL_BRANCH_PROTECTION := standard

include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/cpufeatures)