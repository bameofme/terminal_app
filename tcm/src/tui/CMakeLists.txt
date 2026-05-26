add_library(tcm_tui STATIC
    Renderer.cpp
    SessionListView.cpp
    SwitcherOverlay.cpp
    KeyHandler.cpp
    SessionForm.cpp
)
target_include_directories(tcm_tui PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CURSES_INCLUDE_DIR}
)
target_link_libraries(tcm_tui PUBLIC
    ${CURSES_LIBRARIES}
    tcm_protocol
)
