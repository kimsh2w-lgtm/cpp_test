cmake_minimum_required(VERSION 3.16)
project(logging_demo LANGUAGES CXX)

# ✅ Ninja 빌드 지정
set(CMAKE_GENERATOR "Ninja" CACHE INTERNAL "" FORCE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")


# yaml-cpp, spdlog은 시스템 패키지 또는 FetchContent로 설치 가능
find_package(spdlog REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(CURL REQUIRED )

# ---------- logging 라이브러리 ----------
add_library(logging
    shared/utility/logging/logger_spdlog.cpp
    shared/utility/logging/logger.cpp
)
target_include_directories(logging PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/utility/logging
)
target_link_libraries(logging PUBLIC 
    spdlog::spdlog 
    yaml-cpp 
    CURL::libcurl
)


# ---------- 메인 실행 파일 ----------
add_executable(test_app
    host/main.cpp
)
target_link_libraries(test_app PRIVATE
    logging
)
