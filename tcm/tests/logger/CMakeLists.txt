tcm_add_test(test_logger test_logger_placeholder.cpp)
target_link_libraries(test_logger PRIVATE tcm_logger)

tcm_add_test(test_asciicast_logger AsciicastLoggerTest.cpp)
target_link_libraries(test_asciicast_logger PRIVATE
    tcm_logger
    nlohmann_json::nlohmann_json
)

tcm_add_test(test_logger_macro_integration LoggerMacroIntegrationTest.cpp)
target_link_libraries(test_logger_macro_integration PRIVATE
    tcm_logger
    tcm_macro
    tcm_core
    nlohmann_json::nlohmann_json
)
