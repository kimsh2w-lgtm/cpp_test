#pragma once
#include <string>

namespace system_control {

void notify_ready();
void notify_status(const std::string& msg);
void notify_stopping();
[[noreturn]] void notify_fatal(const std::string& msg, int exit_code = 1);
void notify_watchdog();

} // namespace system_control
