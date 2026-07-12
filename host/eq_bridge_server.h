#pragma once

#include "host/eq_bridge_protocol.h"

#include <atomic>
#include <mutex>
#include <optional>
#include <thread>

#include <windows.h>

namespace termite {

inline constexpr UINT eq_bridge_snapshot_message = WM_APP + 42;

class eq_bridge_server {
public:
    eq_bridge_server() = default;
    ~eq_bridge_server();

    eq_bridge_server(const eq_bridge_server&) = delete;
    eq_bridge_server& operator=(const eq_bridge_server&) = delete;

    [[nodiscard]] bool start(HWND target_window);
    void stop();
    [[nodiscard]] std::optional<eq_bridge_snapshot_v1> take_latest();

private:
    void server_loop();
    void publish(eq_bridge_snapshot_v1 snapshot);

    std::atomic<HWND> target_window_{};
    std::atomic_bool stopping_{};
    std::jthread worker_;
    std::mutex latest_mutex_;
    std::optional<eq_bridge_snapshot_v1> latest_;
};

}  // namespace termite
