cmake_minimum_required(VERSION 3.16)
project(logging_demo LANGUAGES CXX)

# ✅ Ninja 빌드 지정
set(CMAKE_GENERATOR "Ninja" CACHE INTERNAL "" FORCE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")


# yaml-cpp, spdlog은 시스템 패키지 또는 FetchContent로 설치 가능
find_package(yaml-cpp REQUIRED)
find_package(CURL REQUIRED )


# ---------- logging 라이브러리 ----------
set(LOGGER_SOURCES
    shared/utility/logging/logger.cpp
    shared/utility/logging/logger_console.cpp
)


# ---- Add spdlog sources only when enabled ----
option(USE_SPDLOG "Enable spdlog logging backend" ON)

if (USE_SPDLOG)
    add_definitions(-DUSE_SPDLOG)
    # Try to find spdlog but allow fallback to console-only
    find_package(spdlog QUIET)
    if (NOT spdlog_FOUND)
        message(WARNING "Warning: spdlog not found; building console-only logger.")
        set(USE_SPDLOG OFF)
    else()
        list(APPEND LOGGER_SOURCES
            shared/utility/logging/logger_spdlog.cpp
        )
    endif()
endif()


# ---------- Build logging library ----------
add_library(logging ${LOGGER_SOURCES})

target_include_directories(logging PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/utility/logging
)

# ---- Link libraries ----

target_link_libraries(logging PUBLIC
    yaml-cpp
    CURL::libcurl
)

if (USE_SPDLOG)
    target_link_libraries(logging PUBLIC
        spdlog::spdlog
    )
endif()

# ---------- 메인 실행 파일 ----------
add_executable(test_app
    host/main.cpp
)
target_link_libraries(test_app PRIVATE
    logging
)
