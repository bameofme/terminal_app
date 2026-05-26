#include "tui/SessionForm.hpp"
#include "tui/Renderer.hpp"
#include "core/EventBus.hpp"
#include "protocol/SessionConfig.hpp"

#include <gtest/gtest.h>
#include <ncurses.h>

using namespace tcm;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class SessionFormTest : public ::testing::Test {
protected:
    EventBus bus;
};

// ---------------------------------------------------------------------------
// 1. Form mới có type = SSH mặc định
// ---------------------------------------------------------------------------
TEST_F(SessionFormTest, DefaultTypeIsSsh) {
    SessionForm form(bus);
    EXPECT_EQ(SessionType::SSH, form.getConfig().type);
}

// ---------------------------------------------------------------------------
// 2. reset() clear tất cả fields
// ---------------------------------------------------------------------------
TEST_F(SessionFormTest, ResetClearsAllFields) {
    SessionForm form(bus);

    // Populate Name (field 0)
    form.handleKey('n');
    // Navigate to Host (field 2 for SSH)
    form.handleKey(KEY_DOWN); // → Type
    form.handleKey(KEY_DOWN); // → Host
    form.handleKey('h');

    form.reset();

    auto cfg = form.getConfig();
    EXPECT_EQ("", cfg.name);
    ASSERT_TRUE(cfg.ssh.has_value());
    EXPECT_EQ("", cfg.ssh->host);
    EXPECT_EQ(22, cfg.ssh->port); // default restored
}

// ---------------------------------------------------------------------------
// 3. loadConfig(SshConfig) populate đúng fields
// ---------------------------------------------------------------------------
TEST_F(SessionFormTest, LoadConfigPopulatesSshFields) {
    SessionConfig cfg;
    cfg.name = "prod-server";
    cfg.type = SessionType::SSH;
    SshConfig ssh;
    ssh.host           = "10.0.0.1";
    ssh.port           = 2222;
    ssh.username       = "admin";
    ssh.password       = "p@ss";
    ssh.privateKeyPath = "/home/user/.ssh/id_ed25519";
    cfg.ssh            = ssh;

    SessionForm form(bus);
    form.loadConfig(cfg);

    auto result = form.getConfig();
    EXPECT_EQ("prod-server", result.name);
    EXPECT_EQ(SessionType::SSH, result.type);
    ASSERT_TRUE(result.ssh.has_value());
    EXPECT_EQ("10.0.0.1",                result.ssh->host);
    EXPECT_EQ(2222,                       result.ssh->port);
    EXPECT_EQ("admin",                    result.ssh->username);
    EXPECT_EQ("p@ss",                     result.ssh->password);
    EXPECT_EQ("/home/user/.ssh/id_ed25519", result.ssh->privateKeyPath);
}

// ---------------------------------------------------------------------------
// 4. handleKey(printable) append vào selected field
// ---------------------------------------------------------------------------
TEST_F(SessionFormTest, HandleKeyPrintableAppendsToField) {
    SessionForm form(bus);
    // Field 0 (Name) is selected by default
    form.handleKey('s');
    form.handleKey('r');
    form.handleKey('v');

    EXPECT_EQ("srv", form.getConfig().name);
}

// ---------------------------------------------------------------------------
// 5. handleKey(backspace) remove char cuối
// ---------------------------------------------------------------------------
TEST_F(SessionFormTest, HandleKeyBackspaceRemovesLastChar) {
    SessionForm form(bus);
    form.handleKey('a');
    form.handleKey('b');
    form.handleKey('c');
    form.handleKey(127); // DEL/backspace

    EXPECT_EQ("ab", form.getConfig().name);
}

// ---------------------------------------------------------------------------
// 6. handleKey(ESC) → result = Cancelled
// ---------------------------------------------------------------------------
TEST_F(SessionFormTest, HandleKeyEscSetsCancelled) {
    SessionForm form(bus);
    EXPECT_EQ(FormResult::None, form.getResult());

    form.handleKey(27); // ESC

    EXPECT_EQ(FormResult::Cancelled, form.getResult());
}

// ---------------------------------------------------------------------------
// 7. isValid() false khi name rỗng
// ---------------------------------------------------------------------------
TEST_F(SessionFormTest, IsValidFalseWhenNameEmpty) {
    SessionForm form(bus);
    EXPECT_FALSE(form.isValid());
    EXPECT_FALSE(form.getValidationError().empty());
}

// ---------------------------------------------------------------------------
// 8. isValid() true khi tất cả required fields có giá trị (SSH: name+host+username)
// ---------------------------------------------------------------------------
TEST_F(SessionFormTest, IsValidTrueWhenRequiredFieldsFilled) {
    SessionForm form(bus);

    // Name (field 0)
    form.handleKey('m');
    form.handleKey('y');

    // Host (field 2)
    form.handleKey(KEY_DOWN); // → Type (1)
    form.handleKey(KEY_DOWN); // → Host (2)
    form.handleKey('h');

    // Username (field 4)
    form.handleKey(KEY_DOWN); // → Port (3)
    form.handleKey(KEY_DOWN); // → Username (4)
    form.handleKey('u');

    EXPECT_TRUE(form.isValid());
    EXPECT_EQ("", form.getValidationError());
}

// ---------------------------------------------------------------------------
// 9. getConfig() trả về đúng SessionConfig
// ---------------------------------------------------------------------------
TEST_F(SessionFormTest, GetConfigReturnsSshConfig) {
    SessionConfig cfg;
    cfg.name = "web";
    cfg.type = SessionType::SSH;
    SshConfig ssh;
    ssh.host     = "web.example.com";
    ssh.port     = 22;
    ssh.username = "deploy";
    ssh.password = "";
    cfg.ssh      = ssh;

    SessionForm form(bus);
    form.loadConfig(cfg);

    auto result = form.getConfig();
    EXPECT_EQ("web",              result.name);
    EXPECT_EQ(SessionType::SSH,   result.type);
    ASSERT_TRUE(result.ssh.has_value());
    EXPECT_EQ("web.example.com",  result.ssh->host);
    EXPECT_EQ(22,                 result.ssh->port);
    EXPECT_EQ("deploy",           result.ssh->username);
}

// ---------------------------------------------------------------------------
// 10. selectNextType() cycle SSH→Serial→Telnet→SSH (via Tab on Type field)
// ---------------------------------------------------------------------------
TEST_F(SessionFormTest, SelectNextTypeCycles) {
    SessionForm form(bus);
    EXPECT_EQ(SessionType::SSH, form.getConfig().type);

    // Navigate to Type field (index 1)
    form.handleKey(KEY_DOWN); // field 0 → 1

    // SSH → Serial
    form.handleKey('\t');
    EXPECT_EQ(SessionType::Serial, form.getConfig().type);

    // Serial → Telnet
    form.handleKey('\t');
    EXPECT_EQ(SessionType::Telnet, form.getConfig().type);

    // Telnet → SSH
    form.handleKey('\t');
    EXPECT_EQ(SessionType::SSH, form.getConfig().type);
}

// ---------------------------------------------------------------------------
// 11. handleKey(KEY_DOWN) moves to next field
// ---------------------------------------------------------------------------
TEST_F(SessionFormTest, HandleKeyDownMovesToNextField) {
    SessionForm form(bus);

    // Type 'n' into Name (field 0)
    form.handleKey('n');

    // Move to Host (field 2 for SSH: 0→1→2)
    form.handleKey(KEY_DOWN); // → Type (1)
    form.handleKey(KEY_DOWN); // → Host (2)
    form.handleKey('h');

    auto cfg = form.getConfig();
    EXPECT_EQ("n", cfg.name);
    ASSERT_TRUE(cfg.ssh.has_value());
    EXPECT_EQ("h", cfg.ssh->host);
}

// ---------------------------------------------------------------------------
// 12. handleKey(KEY_UP) moves to prev field
// ---------------------------------------------------------------------------
TEST_F(SessionFormTest, HandleKeyUpMovesToPrevField) {
    SessionForm form(bus);

    // Navigate to Host (field 2)
    form.handleKey(KEY_DOWN); // → 1
    form.handleKey(KEY_DOWN); // → 2
    form.handleKey('h');      // Host = "h"

    // Go back to Name (field 0)
    form.handleKey(KEY_UP); // → 1 (Type)
    form.handleKey(KEY_UP); // → 0 (Name)
    form.handleKey('n');    // Name = "n"

    auto cfg = form.getConfig();
    EXPECT_EQ("n", cfg.name);
    ASSERT_TRUE(cfg.ssh.has_value());
    EXPECT_EQ("h", cfg.ssh->host);
}
