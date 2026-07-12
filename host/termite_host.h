#pragma once

#include "host/eq_bridge_server.h"
#include "host/route_bridge_server.h"
#include "sound/session_router.h"
#include "sound/wasapi_audio_engine.h"
#include "sound/eq/eq_profile.h"

#include <windows.h>

#include <vector>

namespace termite {

// The native side deliberately has no user interface. It owns the audio
// engine and the local bridges; TermiteUI.exe is the only visible application.
class termite_host {
public:
    explicit termite_host(HINSTANCE instance);
    ~termite_host();

    termite_host(const termite_host&) = delete;
    termite_host& operator=(const termite_host&) = delete;

    int run();

private:
    static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

    [[nodiscard]] bool create_message_window();
    [[nodiscard]] LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam);
    [[nodiscard]] bool launch_frontend();
    void close_frontend_handle() noexcept;
    void apply_bridge_snapshot();
    void process_route_bridge_request(route_bridge_transaction_v1& transaction);
    void restore_automatic_routes();

    HINSTANCE instance_{};
    HWND window_{};
    HANDLE single_instance_mutex_{};
    HANDLE frontend_process_{};
    HRESULT com_initialization_{E_FAIL};
    eq_profile profile_{eq_profile::flat()};
    wasapi_audio_engine audio_engine_;
    eq_bridge_server eq_bridge_;
    route_bridge_server route_bridge_;
    session_router session_router_;
    std::vector<app_audio_route_snapshot> automatic_routes_;
};

}  // namespace termite
