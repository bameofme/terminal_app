tcm_add_test(test_core test_core_placeholder.cpp)
target_link_libraries(test_core PRIVATE tcm_core)

tcm_add_test(test_IConnectionTest IConnectionTest.cpp)
target_link_libraries(test_IConnectionTest PRIVATE tcm_core)

tcm_add_test(test_SessionManagerTest SessionManagerTest.cpp)
target_link_libraries(test_SessionManagerTest PRIVATE tcm_core)

tcm_add_test(test_EventBusTest EventBusTest.cpp)
target_link_libraries(test_EventBusTest PRIVATE tcm_core)

tcm_add_test(test_RingBufferTest RingBufferTest.cpp)
target_link_libraries(test_RingBufferTest PRIVATE tcm_core)

tcm_add_test(test_EpollLoopTest EpollLoopTest.cpp)
target_link_libraries(test_EpollLoopTest PRIVATE tcm_core)

tcm_add_test(test_PtyProxyTest PtyProxyTest.cpp)
target_link_libraries(test_PtyProxyTest PRIVATE tcm_core GTest::gmock)

tcm_add_test(test_integration_core IntegrationTest.cpp)
target_link_libraries(test_integration_core PRIVATE
    tcm_core
    GTest::gmock
)

tcm_add_test(test_error_recovery ErrorRecoveryTest.cpp)
target_link_libraries(test_error_recovery PRIVATE
    tcm_core
    tcm_credential
    tcm_logger
    tcm_macro
    tcm_tui
)
