tcm_add_test(test_recipe   RecipeTest.cpp)
target_link_libraries(test_recipe PRIVATE tcm_macro)

tcm_add_test(test_macro_engine   MacroEngineTest.cpp)
target_link_libraries(test_macro_engine PRIVATE tcm_macro GTest::gmock)

