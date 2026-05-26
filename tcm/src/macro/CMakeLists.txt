add_library(tcm_macro STATIC
    Recipe.cpp
    MacroEngine.cpp
)
target_include_directories(tcm_macro PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(tcm_macro PUBLIC
    nlohmann_json::nlohmann_json
    tcm_core
)
target_compile_features(tcm_macro PUBLIC cxx_std_20)

