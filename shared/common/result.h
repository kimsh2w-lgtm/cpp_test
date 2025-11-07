// ============================================================================
// File: shared/common/result.hpp
// Description: Unified Result / Error handling structure for entire system
// Author: <your name>
// ============================================================================

#pragma once
#include <string>
#include <utility>
#include <type_traits>
#include <optional>
#include <cstdint>

// ----------------------------------------------------------------------------
// 1. Subsystem-level result code (for ABI-safe C interface)
// ----------------------------------------------------------------------------

extern "C" {

// NOTE: C ABI safe (int-based). Extendable by appending new codes.
enum class SystemResultCode : int32_t {
    Ok = 0,
    Fail,
    InvalidArgument,
    Timeout,
    NotFound,
    InternalError,
    NotSupported,
    Unknown
};

// C-compatible result struct (for extern "C" interface)
struct SystemResult {
    SystemResultCode code;
    const char* message;
};

} // extern "C"

// ----------------------------------------------------------------------------
// 2. Modern C++ Result<T, E> template (for internal use)
// ----------------------------------------------------------------------------

template <typename T, typename E = std::string>
class Result {
public:
    // Factory methods
    static Result Ok(T value) { return Result(std::move(value)); }
    static Result Error(E error) { return Result(std::move(error), true); }

    // Query
    bool hasError() const noexcept { return is_error_; }
    explicit operator bool() const noexcept { return !is_error_; }

    const T& value() const { return value_; }
    T& value() { return value_; }
    const E& error() const { return error_; }

private:
    bool is_error_ = false;
    T value_{};
    E error_{};

    Result(T val) : is_error_(false), value_(std::move(val)) {}
    Result(E err, bool) : is_error_(true), error_(std::move(err)) {}
};

// Partial specialization for void
template <typename E>
class Result<void, E> {
public:
    static Result Ok() { return Result(false); }
    static Result Error(E error) { return Result(true, std::move(error)); }

    bool hasError() const noexcept { return is_error_; }
    explicit operator bool() const noexcept { return !is_error_; }

    const E& error() const { return error_; }

private:
    bool is_error_;
    E error_;

    Result(bool err) : is_error_(err) {}
    Result(bool err, E e) : is_error_(err), error_(std::move(e)) {}
};

// ----------------------------------------------------------------------------
// 3. Utility converters between Result<T> and SubsystemResult
// ----------------------------------------------------------------------------

// Convert Result<void> → SubsystemResult (C-ABI boundary)
inline SystemResult toSystemResult(const Result<void>& r) {
    if (r.hasError()) {
        return { SystemResultCode::InternalError, r.error().c_str() };
    }
    return { SystemResultCode::Ok, "OK" };
}

// Convert SubsystemResult → Result<void>
inline Result<void> fromSubsystemResult(const SystemResult& s) {
    if (s.code == SystemResultCode::Ok)
        return Result<void>::Ok();
    return Result<void>::Error(s.message ? s.message : "unknown error");
}

// ----------------------------------------------------------------------------
// 4. Optional utility for string conversion (for logging / debugging)
// ----------------------------------------------------------------------------

inline const char* to_string(SystemResultCode code) {
    switch (code) {
        case SystemResultCode::Ok: return "Ok";
        case SystemResultCode::Fail: return "Fail";
        case SystemResultCode::InvalidArgument: return "InvalidArgument";
        case SystemResultCode::Timeout: return "Timeout";
        case SystemResultCode::NotFound: return "NotFound";
        case SystemResultCode::InternalError: return "InternalError";
        case SystemResultCode::NotSupported: return "NotSupported";
        default: return "Unknown";
    }
}
