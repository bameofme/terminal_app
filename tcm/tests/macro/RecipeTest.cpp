#include <gtest/gtest.h>
#include "macro/Recipe.hpp"

using namespace tcm;

static const std::string kValidJson = R"({
    "name": "ont-login",
    "description": "Login vao ONT",
    "steps": [
        {
            "expect": "login:",
            "send": "admin\n",
            "timeout_ms": 5000,
            "on_timeout": "abort"
        },
        {
            "expect": "Password:",
            "send": "{password}\n",
            "timeout_ms": 3000,
            "on_timeout": "skip"
        },
        {
            "expect": "#",
            "send": "show version\n",
            "timeout_ms": 2000
        }
    ],
    "variables": {
        "password": "secret123"
    }
})";

TEST(RecipeTest, LoadFromStringParsesName) {
    auto recipe = Recipe::loadFromString(kValidJson);
    EXPECT_EQ(recipe.name, "ont-login");
    EXPECT_EQ(recipe.description, "Login vao ONT");
}

TEST(RecipeTest, LoadFromStringParsesStepsOrdered) {
    auto recipe = Recipe::loadFromString(kValidJson);
    ASSERT_EQ(recipe.steps.size(), 3u);
    EXPECT_EQ(recipe.steps[0].expect, "login:");
    EXPECT_EQ(recipe.steps[0].send, "admin\n");
    EXPECT_EQ(recipe.steps[1].expect, "Password:");
    EXPECT_EQ(recipe.steps[2].expect, "#");
}

TEST(RecipeTest, LoadFromStringThrowsOnMissingName) {
    const std::string json = R"({"steps": [{"expect": "login:"}]})";
    EXPECT_THROW(Recipe::loadFromString(json), std::runtime_error);
}

TEST(RecipeTest, LoadFromStringThrowsOnMissingSteps) {
    const std::string json = R"({"name": "test"})";
    EXPECT_THROW(Recipe::loadFromString(json), std::runtime_error);
}

TEST(RecipeTest, LoadFromStringThrowsOnInvalidJson) {
    EXPECT_THROW(Recipe::loadFromString("not json"), std::runtime_error);
}

TEST(RecipeTest, StepDefaultValues) {
    const std::string json = R"({
        "name": "test",
        "steps": [{"expect": "prompt>"}]
    })";
    auto recipe = Recipe::loadFromString(json);
    ASSERT_EQ(recipe.steps.size(), 1u);
    EXPECT_EQ(recipe.steps[0].timeoutMs, 5000);
    EXPECT_EQ(recipe.steps[0].onTimeout, OnTimeoutAction::Abort);
    EXPECT_EQ(recipe.steps[0].send, "");
}

TEST(RecipeTest, StepOnTimeoutParsedCorrectly) {
    auto recipe = Recipe::loadFromString(kValidJson);
    EXPECT_EQ(recipe.steps[0].onTimeout, OnTimeoutAction::Abort);
    EXPECT_EQ(recipe.steps[1].onTimeout, OnTimeoutAction::Skip);
    EXPECT_EQ(recipe.steps[2].onTimeout, OnTimeoutAction::Abort);  // default
    EXPECT_EQ(recipe.steps[1].timeoutMs, 3000);
}

TEST(RecipeTest, VariablesMapParsedCorrectly) {
    auto recipe = Recipe::loadFromString(kValidJson);
    ASSERT_EQ(recipe.variables.count("password"), 1u);
    EXPECT_EQ(recipe.variables.at("password"), "secret123");
}

TEST(RecipeTest, IsValidReturnsFalseForEmptySteps) {
    Recipe recipe;
    recipe.name = "test";
    EXPECT_FALSE(recipe.isValid());
    EXPECT_FALSE(recipe.validationError().empty());
}

TEST(RecipeTest, IsValidReturnsTrueForValidRecipe) {
    auto recipe = Recipe::loadFromString(kValidJson);
    EXPECT_TRUE(recipe.isValid());
    EXPECT_EQ(recipe.validationError(), "");
}

TEST(RecipeTest, OnTimeoutContinueParsed) {
    const std::string json = R"({
        "name": "test",
        "steps": [{"expect": "prompt>", "on_timeout": "continue"}]
    })";
    auto recipe = Recipe::loadFromString(json);
    EXPECT_EQ(recipe.steps[0].onTimeout, OnTimeoutAction::Continue);
}
