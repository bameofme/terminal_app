add_library(tcm_protocol STATIC
    SshConnection.cpp
    SerialConnection.cpp
    IacParser.cpp
    TelnetConnection.cpp
    ConnectionFactory.cpp
    SessionConfig.cpp
)

target_include_directories(tcm_protocol PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${LIBSSH2_INCLUDE_DIRS}
)

target_link_libraries(tcm_protocol PUBLIC
    ${LIBSSH2_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)

target_compile_options(tcm_protocol PRIVATE ${LIBSSH2_CFLAGS_OTHER})
