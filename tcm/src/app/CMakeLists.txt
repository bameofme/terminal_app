add_library(tcm_app STATIC
    Application.cpp
)
target_include_directories(tcm_app PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(tcm_app PUBLIC
    tcm_core
    tcm_protocol
    tcm_tui
    tcm_logger
    tcm_credential
    nlohmann_json::nlohmann_json
)
