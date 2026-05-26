# Tests for all modules — helper macro to register test executables
macro(tcm_add_test TARGET_NAME SOURCE_FILE)
    add_executable(${TARGET_NAME} ${SOURCE_FILE})
    target_link_libraries(${TARGET_NAME}
        PRIVATE
            GTest::gtest_main
            GTest::gmock
    )
    gtest_discover_tests(${TARGET_NAME})
endmacro()

add_subdirectory(core)
add_subdirectory(protocol)
add_subdirectory(tui)
add_subdirectory(logger)
add_subdirectory(macro)
add_subdirectory(credential)
