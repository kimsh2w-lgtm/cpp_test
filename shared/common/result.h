// ============================================================================
// File: shared/common/result.hpp
// Description: Unified Result / Error handling structure for entire system
// Author: <your name>
// ============================================================================

#pragma once
#include <string>
#include <utility>
#include <optional>
#include <cstdint>


extern "C" {

// NOTE: C ABI safe (int-based). Extendable by appending new codes.
enum class ResultCode : int32_t {
    OK                  = 0,
    Fail                = 1,
    Cancelled           = 2,

    // input & state error
    InvalidArgument     = 100,
    AlreadyExists       = 101,
    DuplicateIgnored    = 102,
    NotFound            = 103,
    OutOfRange          = 104,
    
    // system & resource error
    PermissionDenied    = 200,
    Timeout             = 201,
    OutOfMemory         = 202,
    ResourceBusy        = 203,
    InvalidState        = 204,
    RateLimit           = 205,

    // intternal error
    InternalError       = 300,
    NotSupported        = 301,
    SocketError         = 302,

    // network error
    NetworkError        = 400,
    ConnectionFail      = 402,
    ConnectionLost      = 403,
    ProtocolError       = 404,

    Unknown
};

inline constexpr bool isSuccess(ResultCode code) noexcept {
    switch (code) {
        case ResultCode::OK:
        case ResultCode::DuplicateIgnored:
            return true;
        default:
            return false;
    }
}

inline constexpr bool isFailure(ResultCode code) noexcept {
    return !isSuccess(code);
}

// ----------------------------------------------------------------------------
// 1. System-level result code (for ABI-safe C interface)
// ----------------------------------------------------------------------------

// C-compatible result struct (for extern "C" interface)
struct SystemResult {
    ResultCode code;
    const char* message;
};

} // extern "C"

// ----------------------------------------------------------------------------
// 2. Modern C++ Result<T, E> template (for internal use)
// ----------------------------------------------------------------------------

template <typename T, typename E = std::optional<std::string>>
class Result {
public:
    // Default constructor: success by default
    Result() : code_(ResultCode::OK), error_(std::nullopt) {}

    // Factory methods
    static Result OK(T value) { return Result(std::move(value)); }
    static Result Fail() { return Error(ResultCode::Fail); }
    static Result Error(ResultCode code, E error = std::nullopt) { return Result(code, std::move(error)); }

    // Query
    [[nodiscard]] bool hasError() const noexcept { return isFailure(code_); }
    [[nodiscard]] explicit operator bool() const noexcept { return isSuccess(code_); }

    [[nodiscard]] ResultCode code() const noexcept { return code_; }
    [[nodiscard]] const T& value() const noexcept { return value_; }
    [[nodiscard]] T& value() noexcept  { return value_; }
    [[nodiscard]] const E& error() const noexcept  { return error_; }

private:
    ResultCode code_;
    T value_{};
    E error_;

    // Success constructor
    explicit Result(T val)
        : code_(ResultCode::OK), value_(std::move(val)), error_(std::nullopt) {}

    // Error constructor
    Result(ResultCode code, E err)
        : code_(code), error_(std::move(err)) {}
};

// ----------------------------------------------------------------------------
// Partial specialization for void
// ----------------------------------------------------------------------------
template <typename E>
class Result<void, E> {
public:
    Result() : code_(ResultCode::OK), error_(std::nullopt) {}
    static Result OK() { return Result(ResultCode::OK, std::nullopt); }
    static Result Fail() { return Error(ResultCode::Fail); }
    static Result Error(ResultCode code, E error = std::nullopt) { return Result(code, std::move(error)); }

    [[nodiscard]] bool hasError() const noexcept { return isFailure(code_); }
    [[nodiscard]] explicit operator bool() const noexcept { return isSuccess(code_); }

    [[nodiscard]] const E& error() const noexcept  { return error_; }
    [[nodiscard]] ResultCode code() const noexcept  { return code_; }

    [[nodiscard]] const char* c_str() const noexcept  {
        return error_.has_value() ? error_->c_str() : "";
    }

private:
    ResultCode code_;
    E error_;

    Result(ResultCode code, E e)
        : code_(code), error_(std::move(e)) {}
};

// ----------------------------------------------------------------------------
// 3. Utility converters between Result<T> and SubsystemResult
// ----------------------------------------------------------------------------

// Convert Result<void> → SubsystemResult (C-ABI boundary)
inline SystemResult toSystemResult(const Result<void>& r) {
    return { r.code(), r.error().has_value() ? r.error()->c_str() : "" };
}

// Convert SubsystemResult → Result<void>
inline Result<void> fromSystemResult(const SystemResult& s) {
    if (isSuccess(s.code))
        return Result<void>::OK();
    if (s.message)
        return Result<void>::Error(s.code, std::string(s.message));
    return Result<void>::Error(s.code);
}

// ----------------------------------------------------------------------------
// 4. Optional utility for string conversion (for logging / debugging)
// ----------------------------------------------------------------------------

constexpr const char* to_string(ResultCode code) {
    switch (code) {
        case ResultCode::OK:               return "OK";
        case ResultCode::Fail:             return "Fail";
        case ResultCode::Cancelled:        return "Cancelled";
        case ResultCode::InvalidArgument:  return "InvalidArgument";
        case ResultCode::AlreadyExists:    return "AlreadyExists";
        case ResultCode::DuplicateIgnored: return "DuplicateIgnored";
        case ResultCode::NotFound:         return "NotFound";
        case ResultCode::OutOfRange:       return "OutOfRange";
        case ResultCode::PermissionDenied: return "PermissionDenied";
        case ResultCode::Timeout:          return "Timeout";
        case ResultCode::OutOfMemory:      return "OutOfMemory";
        case ResultCode::ResourceBusy:     return "ResourceBusy";
        case ResultCode::InternalError:    return "InternalError";
        case ResultCode::NotSupported:     return "NotSupported";
        case ResultCode::NetworkError:     return "NetworkError";
        case ResultCode::ConnectionLost:   return "ConnectionLost";
        case ResultCode::ProtocolError:    return "ProtocolError";
        default:                           return "Unknown";
    }
}



// LOGI("Subsystem result: {}", to_string(result));
inline std::string to_string(const Result<void>& r) {
    return std::string(to_string(r.code())) +
           (r.error().has_value() ? (": " + *r.error()) : "");
}


inline Result<void> OK() noexcept { return Result<void>::OK(); }
inline Result<void> Fail() noexcept { return Result<void>::Fail(); }
inline Result<void> Error(ResultCode code, std::optional<std::string> msg = std::nullopt) noexcept  { 
    return Result<void>::Error(code, std::move(msg)); 
}


// 중복이지만 허용되는 경우 명시적으로 사용 가능
inline Result<void> DuplicateIgnored(std::optional<std::string> msg = std::nullopt) noexcept  {
    return Result<void>::Error(ResultCode::DuplicateIgnored, std::move(msg));
}