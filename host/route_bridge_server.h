#pragma once

#include "host/route_bridge_protocol.h"

#include <atomic>
#include <thread>

#include <windows.h>

namespace termite {

class route_bridge_server {
public:
    route_bridge_server() = default;
    ~route_bridge_server();

    route_bridge_server(const route_bridge_server&) = delete;
    route_bridge_server& operator=(const route_bridge_server&) = delete;

    [[nodiscard]] bool start(HWND target_window);
    void stop();

private:
    void server_loop();

    std::atomic<HWND> target_window_{};
    std::atomic_bool stopping_{};
    std::jthread worker_;
};

}  // namespace termite
