#ifndef PULSE_SAFEDOCK_SHARED_BUILD_FEATURES_H
#define PULSE_SAFEDOCK_SHARED_BUILD_FEATURES_H

#ifndef PULSE_FEATURE_IO_MIRROR_REMOTE_DO_PLUS
#define PULSE_FEATURE_IO_MIRROR_REMOTE_DO_PLUS 0
#endif

#ifndef PULSE_FEATURE_OTA_RELEASE_POLICY_STRICT
#define PULSE_FEATURE_OTA_RELEASE_POLICY_STRICT 0
#endif

#ifndef PULSE_FW_VERSION_YEAR
#define PULSE_FW_VERSION_YEAR 26
#endif

#ifndef PULSE_FW_VERSION_MAJOR
#define PULSE_FW_VERSION_MAJOR 1
#endif

#ifndef PULSE_FW_VERSION_MINOR
#define PULSE_FW_VERSION_MINOR 0
#endif

#ifndef PULSE_FW_VERSION_PATCH
#define PULSE_FW_VERSION_PATCH 0
#endif

#define PULSE_STRINGIZE_IMPL(x) #x
#define PULSE_STRINGIZE(x) PULSE_STRINGIZE_IMPL(x)

#define PULSE_FW_VERSION_VALUE PULSE_STRINGIZE(PULSE_FW_VERSION_YEAR) "." \
                               PULSE_STRINGIZE(PULSE_FW_VERSION_MAJOR) "." \
                               PULSE_STRINGIZE(PULSE_FW_VERSION_MINOR) "." \
                               PULSE_STRINGIZE(PULSE_FW_VERSION_PATCH)

#define PULSE_FW_VERSION_LABEL "PR: " PULSE_FW_VERSION_VALUE

namespace pulse {
namespace shared {

inline constexpr bool kFeatureIoMirrorRemoteDoPlus = PULSE_FEATURE_IO_MIRROR_REMOTE_DO_PLUS != 0;
inline constexpr bool kFeatureOtaReleasePolicyStrict = PULSE_FEATURE_OTA_RELEASE_POLICY_STRICT != 0;
inline constexpr const char* kFirmwareVersionValue = PULSE_FW_VERSION_VALUE;
inline constexpr const char* kFirmwareVersionLabel = PULSE_FW_VERSION_LABEL;

}  // namespace shared
}  // namespace pulse

#endif
