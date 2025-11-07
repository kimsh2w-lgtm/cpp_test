#include "system_control.hpp"
#include <systemd/sd-daemon.h>
#include <cstdlib>

namespace system_control {

void notify_ready() {
    sd_notify(0, "READY=1");
}
void notify_status(const std::string& msg) {
    sd_notifyf(0, "STATUS=%s", msg.c_str());
}
void notify_stopping() {
    sd_notify(0, "STOPPING=1");
}
[[noreturn]] void notify_fatal(const std::string& msg, int exit_code) {
    sd_notifyf(0, "STATUS=FATAL: %s", msg.c_str());
    sd_notify(0, "STOPPING=1");
    std::exit(exit_code);
}
void notify_watchdog() {
    sd_notify(0, "WATCHDOG=1");
}

} // namespace system_control
