#pragma once

// MAKU_APP_VERSION and MAKUTWEAKER_BUILD are set in CMakeLists.txt from BuildNumber.txt.

#ifndef MAKU_APP_VERSION
#define MAKU_APP_VERSION "5.8.0"
#endif

#ifndef MAKUTWEAKER_BUILD
#define MAKUTWEAKER_BUILD 0
#endif

namespace maku::version {

inline constexpr const char* kText = MAKU_APP_VERSION;
inline constexpr int kBuild = MAKUTWEAKER_BUILD;

} // namespace maku::version
