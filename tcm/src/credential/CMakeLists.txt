add_library(tcm_credential STATIC
    CredentialStore.cpp
)
target_include_directories(tcm_credential PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(tcm_credential PUBLIC
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)
target_compile_features(tcm_credential PUBLIC cxx_std_20)

