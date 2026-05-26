add_library(tcm_core STATIC
    Session.cpp
    SessionManager.cpp
    RingBuffer.cpp
    EpollLoop.cpp
    PtyProxy.cpp
)
target_include_directories(tcm_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/core
)
target_link_libraries(tcm_core PUBLIC util)
