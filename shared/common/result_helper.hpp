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
// 간결한 에러 반환용 매크로.
// 사용 예시:
//   auto r = initSubsystem();
//   RETURN_IF_ERR(r);
// ----------------------------------------------------------------------------
#define RETURN_IF_ERR(res)                        \
    do {                                          \
        if (!(res)) {                             \
            return Result<void>::Error((res).error()); \
        }                                         \
    } while (0)

#define RETURN_IF_ERR_MSG(res, msg)               \
    do {                                          \
        if (!(res)) {                             \
            return Result<void>::Error(fmt::format("{}: {}", msg, (res).error())); \
        }                                         \
    } while (0)

// ----------------------------------------------------------------------------
// 2. LOG_IF_ERR 매크로 (에러 로그만 남기고 계속 진행)
// ----------------------------------------------------------------------------
#define LOG_IF_ERR(res)                           \
    do {                                          \
        if (!(res)) {                             \
            LOGE((res).error());                  \
        }                                         \
    } while (0)

// ----------------------------------------------------------------------------
// 3. Result<T> 체이닝 도우미 (.and_then, .map, .or_else)
// ----------------------------------------------------------------------------
// 함수형 스타일로 Result를 연결할 수 있게 해줍니다.
// ex:
//   computeA()
//     .and_then([](auto a){ return computeB(a); })
//     .and_then([](auto b){ return computeC(b); });
// ----------------------------------------------------------------------------

template <typename T, typename E>
class ResultChain {
public:
    explicit ResultChain(Result<T, E> result) : res_(std::move(result)) {}

    template <typename F>
    auto and_then(F&& func) -> ResultChain<decltype(func(res_.value()).value()), E> {
        if (res_.hasError()) return ResultChain<decltype(func(res_.value()).value()), E>(
            Result<decltype(func(res_.value()).value()), E>::Err(res_.error())
        );
        return ResultChain<decltype(func(res_.value()).value()), E>(func(res_.value()));
    }

    template <typename F>
    auto map(F&& func) -> ResultChain<decltype(func(res_.value())), E> {
        if (res_.hasError()) return ResultChain<decltype(func(res_.value())), E>(
            Result<decltype(func(res_.value())), E>::Err(res_.error())
        );
        return ResultChain<decltype(func(res_.value())), E>(
            Result<decltype(func(res_.value())), E>::Ok(func(res_.value()))
        );
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

// 체이닝 진입점 헬퍼 함수
template <typename T, typename E>
inline auto make_chain(Result<T, E> r) -> ResultChain<T, E> {
    return ResultChain<T, E>(std::move(r));
}

inline Result<void> Ok() { return Result<void>::Ok(); }
inline Result<void> Fail() { return Result<void>::Error("fail"); }
inline Result<void> Error(std::string msg) { return Result<void>::Error(std::move(msg)); }



