#pragma once
#include <string>
#include <vector>
#include <map>
#include <optional>

#include "utility/logging/logging.hpp"

namespace manifest {
// ---------------------------
// 호스트 정보 구조체
// ---------------------------
struct HostInfo {
    std::string entry;  // 실행 엔트리 경로 (예: hosts/gui_qt/dashboard)
};

// ---------------------------
// 서브시스템 정보 구조체
// ---------------------------
struct SubsystemInfo {
    std::string name;                // 서브시스템 이름
    std::string group;               // 그룹 (예: media, services)
    std::string description;         // 설명
    int priority = 0;                // 로딩 우선순위
    std::string config;              // 설정 파일 경로
    bool auto_start = false;         // 부팅 시 자동 시작 여부
    std::string allow_version;       // 허용 버전
    std::vector<int> affinity;       // CPU 코어 바인딩
    std::string restart_policy;      // 재시작 정책 (on_failure 등)
    int restart_delay_ms = 0;        // 재시작 지연 (ms)
    int max_retries = 0;             // 최대 재시도 횟수
    bool optional = false;           // 실패 시 무시 가능 여부
    std::vector<std::string> denied_modes; // 금지 모드 (예: low_power)
    std::vector<std::string> depends_on;   // 의존 서브시스템
};

// ---------------------------
// 시스템 정보 구조체
// ---------------------------
struct SystemInfo {
    std::string name;         // 시스템 이름
    std::string description;  // 시스템 설명
    std::string mode;         // 현재 모드 (예: normal)
};

// ---------------------------
// 전체 매니페스트 구조체
// ---------------------------
struct SystemManifest {
    std::vector<std::string> platforms;       // 플랫폼 목록
    std::vector<std::string> modes;           // 지원 모드 목록
    std::vector<std::string> restart_policys; // 재시작 정책 목록
    SystemInfo system;                        // 시스템 정보
    std::map<std::string, HostInfo> hosts;    // 호스트 목록
    std::vector<SubsystemInfo> subsystems;    // 서브시스템 목록
};

}; // namespace system_manifest.hpp