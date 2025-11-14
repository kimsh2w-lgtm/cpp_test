// ============================================================================
// File: shared/common/result_helper.hpp
// Description: Result<T> helper macros and functional chaining utilities
// Depends on: shared/common/result.hpp
// ============================================================================

#pragma once
#include "result.h"
#include <functional>

// ----------------------------------------------------------------------------
// 1. RETURN_IF_ERR 매크로
// ----------------------------------------------------------------------------
// 사용 예시:
//   auto r = initSubsystem();
//   RETURN_IF_ERR(r);
// ----------------------------------------------------------------------------
#define RETURN_IF_ERR(res)                                     \
    do {                                                       \
        if (!(res)) {                                          \
            return Result<void>::Error((res).code(), (res).error()); \
        }                                                      \
    } while (0)

#define RETURN_IF_ERR_MSG(res, msg)                            \
    do {                                                       \
        if (!(res)) {                                          \
            auto err_str = (res).error().has_value()           \
                ? fmt::format("{}: {}", msg, *(res).error())   \
                : std::string(msg);                            \
            return Result<void>::Error((res).code(), err_str); \
        }                                                      \
    } while (0)

// ----------------------------------------------------------------------------
// 2. LOG_IF_ERR 매크로
// ----------------------------------------------------------------------------
#define LOG_IF_ERR(res)                                        \
    do {                                                       \
        if (!(res)) {                                          \
            if ((res).error().has_value())                     \
                LOGE(*(res).error());                           \
            else                                                \
                LOGE("Error: {}", to_string((res).code()));     \
        }                                                      \
    } while (0)

// ----------------------------------------------------------------------------
// 3. Result<T> 체이닝 유틸리티 (.and_then, .map, .or_else)
// ----------------------------------------------------------------------------

template <typename T, typename E>
class ResultChain {
public:
    explicit ResultChain(Result<T, E> result)
        : res_(std::move(result)) {}

    template <typename F>
    auto and_then(F&& func)
        -> ResultChain<decltype(func(res_.value()).value()), E> {
        if (res_.hasError()) {
            using NextType = decltype(func(res_.value()).value());
            return ResultChain<NextType, E>(
                Result<NextType, E>::Error(res_.code(), res_.error()));
        }
        return ResultChain<decltype(func(res_.value()).value()), E>(func(res_.value()));
    }

    template <typename F>
    auto map(F&& func)
        -> ResultChain<decltype(func(res_.value())), E> {
        if (res_.hasError()) {
            using NextType = decltype(func(res_.value()));
            return ResultChain<NextType, E>(
                Result<NextType, E>::Error(res_.code(), res_.error()));
        }
        return ResultChain<decltype(func(res_.value())), E>(
            Result<decltype(func(res_.value())), E>::OK(func(res_.value())));
    }

    template <typename F>
    auto or_else(F&& func) -> ResultChain<T, E> {
        if (res_.hasError()) {
            func(res_.error());
        }
        return *this;
    }

    Result<T, E> unwrap() { return std::move(res_); }

private:
    Result<T, E> res_;
};

// ----------------------------------------------------------------------------
// 4. 체이닝 시작 헬퍼
// ----------------------------------------------------------------------------
template <typename T, typename E>
inline auto make_chain(Result<T, E> r) -> ResultChain<T, E> {
    return ResultChain<T, E>(std::move(r));
}

