/*
 * Copyright (C) 2020 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "QTI PowerHAL"

#include <aidl/android/hardware/power/BnPower.h>
#include <android-base/file.h>
#include <android-base/logging.h>
#include <sys/ioctl.h>
#include <log/log.h>

// defines from drivers/input/touchscreen/xiaomi/xiaomi_touch.h
#define SET_CUR_VALUE 0
#define Touch_Doubletap_Mode 14

#define TOUCH_DEV_PATH "/dev/xiaomi-touch"

#define TOUCH_MAGIC 0x5400
#define TOUCH_IOC_SETMODE TOUCH_MAGIC + SET_CUR_VALUE

extern "C" {
#include "hint-data.h"
#include "metadata-defs.h"
#include "performance.h"
#include "power-common.h"
#include "utils.h"

const int kMaxLaunchDuration = 4000; /* ms */

static int process_activity_launch_hint(void* data) {
    bool enabled = *((bool*) data);
    static int launch_handle = -1;
    static int launch_mode = 0;

    // release lock early if launch has finished
    if (!enabled) {
        if (CHECK_HANDLE(launch_handle)) {
            release_request(launch_handle);
            launch_handle = -1;
        }
        launch_mode = 0;
        return HINT_HANDLED;
    }

    if (!launch_mode) {
        launch_handle = perf_hint_enable_with_type(VENDOR_HINT_FIRST_LAUNCH_BOOST,
                                                   kMaxLaunchDuration, LAUNCH_BOOST_V1);
        if (!CHECK_HANDLE(launch_handle)) {
            ALOGE("Failed to perform launch boost");
            return HINT_NONE;
        }
        launch_mode = 1;
    }
    return HINT_HANDLED;
}
}

using ::aidl::android::hardware::power::Mode;

namespace aidl {
namespace android {
namespace hardware {
namespace power {
namespace impl {

bool isDeviceSpecificModeSupported(Mode type, bool* _aidl_return) {
    switch (type) {
        case Mode::DOUBLE_TAP_TO_WAKE:
            *_aidl_return = true;
            return true;
        case Mode::LAUNCH:
            *_aidl_return = true;
            return true;
        default:
            return false;
    }
}

bool setDeviceSpecificMode(Mode type, bool enabled) {
    switch (type) {
        case Mode::DOUBLE_TAP_TO_WAKE: {
            int fd = open(TOUCH_DEV_PATH, O_RDWR);
            int arg[2] = {Touch_Doubletap_Mode, enabled ? 1 : 0};
            ioctl(fd, TOUCH_IOC_SETMODE, &arg);
            close(fd);
            return true;
        }
        case Mode::LAUNCH:
            process_activity_launch_hint(&enabled);
            return true;
        default:
            return false;
    }
}

}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace android
}  // namespace aidl
