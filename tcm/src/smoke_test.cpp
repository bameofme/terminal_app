/**
 * Smoke test — standalone executable, no gtest
 *
 * Run:  ./build/tcm_smoke_test
 * Exit: 0 = all OK, 1 = at least one failure
 */

#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>

#include "core/EventBus.hpp"
#include "core/PtyProxy.hpp"
#include "core/RingBuffer.hpp"
#include "core/Session.hpp"
#include "core/SessionManager.hpp"
#include "logger/AsciicastLogger.hpp"

int main() {
    bool ok = true;

    // -----------------------------------------------------------------------
    // Test 1: EventBus
    // -----------------------------------------------------------------------
    {
        tcm::EventBus bus;
        int count = 0;
        auto token = bus.subscribe<tcm::SessionAddedEvent>([&](const auto& /*e*/) {
            count++;
        });
        bus.publish(tcm::SessionAddedEvent{"test-id"});
        bus.unsubscribe(token);
        bool pass = (count == 1);
        ok &= pass;
        std::cout << (pass ? "[OK]" : "[FAIL]") << " EventBus: publish/subscribe\n";
    }

    // -----------------------------------------------------------------------
    // Test 2: SessionManager
    // -----------------------------------------------------------------------
    {
        tcm::EventBus     bus;
        tcm::SessionManager sm(bus);

        tcm::Session s;
        s.name = "test";
        s.type = tcm::SessionType::SSH;
        auto id = sm.add(std::move(s));

        bool addOk = (sm.count() == 1);
        sm.remove(id);
        bool removeOk = (sm.count() == 0);

        bool pass = addOk && removeOk;
        ok &= pass;
        std::cout << (pass ? "[OK]" : "[FAIL]") << " SessionManager: add/remove\n";
    }

    // -----------------------------------------------------------------------
    // Test 3: RingBuffer
    // -----------------------------------------------------------------------
    {
        tcm::RingBuffer buf;
        uint8_t data[] = {1, 2, 3, 4, 5};
        buf.write(data, 5);
        uint8_t out[5];
        auto n     = buf.read(out, 5);
        bool match = (n == 5 && std::memcmp(data, out, 5) == 0);
        ok &= match;
        std::cout << (match ? "[OK]" : "[FAIL]") << " RingBuffer: write/read\n";
    }

    // -----------------------------------------------------------------------
    // Test 4: PtyProxy spawn /bin/echo
    // -----------------------------------------------------------------------
    {
        tcm::EventBus   bus;
        tcm::RingBuffer rb;
        tcm::PtyProxy   pty(rb, bus, "smoke-test");

        bool spawned = pty.spawn({"/bin/echo", "hello"});
        ok &= spawned;
        std::cout << (spawned ? "[OK]" : "[FAIL]") << " PtyProxy: spawn /bin/echo\n";

        if (spawned) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            pty.kill();
        }
    }

    // -----------------------------------------------------------------------
    // Test 5: AsciicastLogger
    // -----------------------------------------------------------------------
    {
        tcm::AsciicastLogger logger;
        bool started = logger.start("smoke-test", 80, 24, "/tmp");
        ok &= started;

        if (started) {
            const uint8_t d[] = "hello\r\n";
            logger.logOutput(d, 7);
            logger.stop();
        }
        std::cout << (started ? "[OK]" : "[FAIL]") << " AsciicastLogger: start/log/stop\n";
    }

    // -----------------------------------------------------------------------
    // Summary
    // -----------------------------------------------------------------------
    std::cout << '\n'
              << (ok ? "All smoke tests PASSED" : "Some smoke tests FAILED")
              << '\n';
    return ok ? 0 : 1;
}
