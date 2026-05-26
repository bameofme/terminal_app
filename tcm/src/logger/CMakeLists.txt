add_library(tcm_logger STATIC
    AsciicastLogger.cpp
)
target_include_directories(tcm_logger PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/logger
)
target_link_libraries(tcm_logger PUBLIC
    nlohmann_json::nlohmann_json
)
