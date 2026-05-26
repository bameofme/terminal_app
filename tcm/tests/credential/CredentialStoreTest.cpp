#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "credential/CredentialStore.hpp"

namespace {

// RAII helper: auto-delete a temp file on scope exit
struct TempFile {
    std::string path;
    explicit TempFile(const std::string& suffix = ".bin") {
        path = std::filesystem::temp_directory_path() /
               ("tcm_cred_test_" + std::to_string(::getpid()) + suffix);
    }
    ~TempFile() { std::filesystem::remove(path); }
};

// ---------------------------------------------------------------------------
// 1. store() + retrieve() round-trip
TEST(CredentialStore, StoreAndRetrieve) {
    tcm::CredentialStore cs;
    cs.store("user1", "s3cr3t!");
    auto result = cs.retrieve("user1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "s3cr3t!");
}

// ---------------------------------------------------------------------------
// 2. retrieve() unknown id → nullopt
TEST(CredentialStore, RetrieveUnknownReturnsNullopt) {
    tcm::CredentialStore cs;
    EXPECT_FALSE(cs.retrieve("nonexistent").has_value());
}

// ---------------------------------------------------------------------------
// 3. remove() → retrieve returns nullopt
TEST(CredentialStore, RemoveThenRetrieveReturnsNullopt) {
    tcm::CredentialStore cs;
    cs.store("key", "value");
    EXPECT_TRUE(cs.remove("key"));
    EXPECT_FALSE(cs.retrieve("key").has_value());
}

// ---------------------------------------------------------------------------
// 4. contains() true after store, false after remove
TEST(CredentialStore, ContainsAfterStoreAndRemove) {
    tcm::CredentialStore cs;
    cs.store("id", "pw");
    EXPECT_TRUE(cs.contains("id"));
    cs.remove("id");
    EXPECT_FALSE(cs.contains("id"));
}

// ---------------------------------------------------------------------------
// 5. listIds() returns correct ids
TEST(CredentialStore, ListIds) {
    tcm::CredentialStore cs;
    cs.store("alpha", "a");
    cs.store("beta", "b");
    cs.store("gamma", "c");

    auto ids = cs.listIds();
    ASSERT_EQ(ids.size(), 3u);
    EXPECT_TRUE(std::find(ids.begin(), ids.end(), "alpha") != ids.end());
    EXPECT_TRUE(std::find(ids.begin(), ids.end(), "beta") != ids.end());
    EXPECT_TRUE(std::find(ids.begin(), ids.end(), "gamma") != ids.end());
}

// ---------------------------------------------------------------------------
// 6. save() + load() round-trip
TEST(CredentialStore, SaveLoadRoundTrip) {
    TempFile tmp;
    {
        tcm::CredentialStore cs(tmp.path, "mysecretpass");
        cs.store("user", "hunter2");
        ASSERT_TRUE(cs.save());
    }
    {
        tcm::CredentialStore cs(tmp.path, "mysecretpass");
        ASSERT_TRUE(cs.load());
        auto result = cs.retrieve("user");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, "hunter2");
    }
}

// ---------------------------------------------------------------------------
// 7. save() file does not contain plaintext secret
TEST(CredentialStore, SavedFileHasNoPlaintext) {
    TempFile tmp;
    const std::string secret = "very_secret_password_xyz";
    {
        tcm::CredentialStore cs(tmp.path, "passphrase");
        cs.store("acct", secret);
        ASSERT_TRUE(cs.save());
    }
    // Read raw bytes of the file
    std::ifstream f(tmp.path, std::ios::binary);
    ASSERT_TRUE(f.is_open());
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    EXPECT_EQ(content.find(secret), std::string::npos)
        << "Plaintext secret found in encrypted file!";
}

// ---------------------------------------------------------------------------
// 8. load() with wrong passphrase → returns false
TEST(CredentialStore, LoadWithWrongPassphraseReturnsFalse) {
    TempFile tmp;
    {
        tcm::CredentialStore cs(tmp.path, "correct_passphrase");
        cs.store("id", "value");
        ASSERT_TRUE(cs.save());
    }
    {
        tcm::CredentialStore cs(tmp.path, "wrong_passphrase");
        EXPECT_FALSE(cs.load());
    }
}

// ---------------------------------------------------------------------------
// 9. Multiple credentials persist through save/load
TEST(CredentialStore, MultipleCredentialsPersist) {
    TempFile tmp;
    {
        tcm::CredentialStore cs(tmp.path, "pass");
        cs.store("srv1", "pw1");
        cs.store("srv2", "pw2");
        cs.store("srv3", "pw3");
        ASSERT_TRUE(cs.save());
    }
    {
        tcm::CredentialStore cs(tmp.path, "pass");
        ASSERT_TRUE(cs.load());
        EXPECT_EQ(cs.retrieve("srv1"), "pw1");
        EXPECT_EQ(cs.retrieve("srv2"), "pw2");
        EXPECT_EQ(cs.retrieve("srv3"), "pw3");
        EXPECT_EQ(cs.listIds().size(), 3u);
    }
}

// ---------------------------------------------------------------------------
// 10. clear() removes all in-memory credentials
TEST(CredentialStore, ClearRemovesAllInMemory) {
    tcm::CredentialStore cs;
    cs.store("a", "1");
    cs.store("b", "2");
    cs.clear();
    EXPECT_TRUE(cs.listIds().empty());
    EXPECT_FALSE(cs.contains("a"));
    EXPECT_FALSE(cs.contains("b"));
}

}  // namespace
