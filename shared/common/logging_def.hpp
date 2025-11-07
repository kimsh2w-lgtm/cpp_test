#pragma once
#include <string>

namespace logging {

enum class Type { SpdLog };
enum class Level { Trace, Debug, Info, Warn, Error, Fatal, Off };

inline constexpr std::string_view GLOBAL_TAG = "*";


inline constexpr const char* DEFAULT_TAG = "Default";

} // namespace