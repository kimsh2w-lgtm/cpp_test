#include <cxxabi.h>
#include <string>

static std::string demangle(const char* name) {
    int status = 0;
    char* demangled = abi::__cxa_demangle(name, nullptr, nullptr, &status);
    std::string result = (status == 0 && demangled) ? demangled : name;
    std::free(demangled);
    return result;
}


inline std::string toLowerCamel(const std::string& name) {
    if (name.empty()) return name;
    std::string out = name;
    out[0] = static_cast<char>(std::tolower(out[0]));
    return out;
}