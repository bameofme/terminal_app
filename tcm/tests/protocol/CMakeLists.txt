tcm_add_test(test_protocol test_protocol_placeholder.cpp)
target_link_libraries(test_protocol PRIVATE tcm_protocol)

tcm_add_test(test_ssh_connection SshConnectionTest.cpp)
target_link_libraries(test_ssh_connection PRIVATE tcm_protocol)

tcm_add_test(test_serial_connection SerialConnectionTest.cpp)
target_link_libraries(test_serial_connection PRIVATE tcm_protocol)

tcm_add_test(test_telnet_connection TelnetConnectionTest.cpp)
target_link_libraries(test_telnet_connection PRIVATE tcm_protocol)

tcm_add_test(test_connection_factory ConnectionFactoryTest.cpp)
target_link_libraries(test_connection_factory PRIVATE tcm_protocol)

tcm_add_test(test_integration_protocol IntegrationTest.cpp)
target_link_libraries(test_integration_protocol PRIVATE
    tcm_protocol
    tcm_core
    nlohmann_json::nlohmann_json
)
