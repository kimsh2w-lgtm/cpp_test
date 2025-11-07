#include "logging.hpp"

using namespace logging;

static constexpr const char* TAG = "Subsystem";

int main() {
    
    // YAML 기반 설정 적용
    logging::init(logging::Type::SpdLog, "logging.yaml");
    
    LOG_INFO(TAG, "서비스 시작");

    //LOG_INFO("CORE", "서비스 시작111");
    //LOG_DEBUG("DB", "DB 쿼리 실행됨111");
    //LOG_FATAL("DB", "치명적 오류 발생 (flush만)111");
    //LOG_WARN("CORE", "오류 발생 후 서비스 종료1111");
    //LOG_INFO("TEST", "TEST AA111=======");

}
