#include "host/termite_host.h"

#include "host/eq_bridge_profile.h"

#include <objbase.h>

#include <algorithm>
#include <array>
#include <cwchar>
#include <filesystem>
#include <string>
#include <string_view>

namespace termite {
namespace {

constexpr wchar_t host_window_class[] = L"TermiteHostWindow";
constexpr UINT_PTR frontend_process_timer_id = 1;

}  // namespace

termite_host::termite_host(HINSTANCE instance)
    : instance_(instance), com_initialization_(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}

termite_host::~termite_host() {
    restore_automatic_routes();
    close_frontend_handle();
    route_bridge_.stop();
    eq_bridge_.stop();
    audio_engine_.stop();
    if (single_instance_mutex_ != nullptr) CloseHandle(single_instance_mutex_);
    if (SUCCEEDED(com_initialization_)) CoUninitialize();
}

int termite_host::run() {
    single_instance_mutex_ = CreateMutexW(nullptr, FALSE, L"Local\\TermiteHost.v1");
    if (single_instance_mutex_ == nullptr) return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"Termite is already running. Close it before starting another copy.",
                    L"Termite", MB_OK | MB_ICONINFORMATION);
        return 0;
    }
    if (!create_message_window()) return 1;
    if (!launch_frontend()) {
        MessageBoxW(nullptr, L"TermiteUI.exe could not be found or started.", L"Termite", MB_OK | MB_ICONERROR);
        DestroyWindow(window_);
        return 1;
    }
    if (!eq_bridge_.start(window_) || !route_bridge_.start(window_)) {
        MessageBoxW(nullptr, L"Termite could not start its local control bridge.", L"Termite", MB_OK | MB_ICONERROR);
        DestroyWindow(window_);
        return 1;
    }

    audio_engine_.set_profile(profile_);
    audio_engine_.start();
    SetTimer(window_, frontend_process_timer_id, 250, nullptr);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    restore_automatic_routes();
    return static_cast<int>(message.wParam);
}

bool termite_host::create_message_window() {
    WNDCLASSEXW definition{};
    definition.cbSize = sizeof(definition);
    definition.lpfnWndProc = &termite_host::window_proc;
    definition.hInstance = instance_;
    definition.lpszClassName = host_window_class;
    if (RegisterClassExW(&definition) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    window_ = CreateWindowExW(0, host_window_class, L"Termite host", 0,
                              0, 0, 0, 0, HWND_MESSAGE, nullptr, instance_, this);
    return window_ != nullptr;
}

LRESULT CALLBACK termite_host::window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* self = reinterpret_cast<termite_host*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        self = static_cast<termite_host*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        // DefWindowProc returns zero for WM_NCCREATE. Returning that value
        // tells Windows to destroy the new window before the host can run.
        return TRUE;
    }
    return self != nullptr ? self->handle_message(message, wparam, lparam) : DefWindowProcW(window, message, wparam, lparam);
}

LRESULT termite_host::handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_TIMER:
            if (wparam == frontend_process_timer_id && frontend_process_ != nullptr &&
                WaitForSingleObject(frontend_process_, 0) == WAIT_OBJECT_0) {
                DestroyWindow(window_);
            }
            return 0;
        case eq_bridge_snapshot_message:
            apply_bridge_snapshot();
            return 0;
        case route_bridge_request_message:
            if (const auto transaction = reinterpret_cast<route_bridge_transaction_v1*>(lparam); transaction != nullptr) {
                process_route_bridge_request(*transaction);
            }
            return 0;
        case WM_DESTROY:
            KillTimer(window_, frontend_process_timer_id);
            route_bridge_.stop();
            eq_bridge_.stop();
            restore_automatic_routes();
            close_frontend_handle();
            audio_engine_.stop();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(window_, message, wparam, lparam);
}

bool termite_host::launch_frontend() {
    std::array<wchar_t, 32768> executable_path{};
    const auto length = GetModuleFileNameW(nullptr, executable_path.data(), static_cast<DWORD>(executable_path.size()));
    if (length == 0 || length >= executable_path.size()) return false;

    const auto host_directory = std::filesystem::path{executable_path.data()}.parent_path();
    const std::array candidates{host_directory / L"TermiteUI.exe"};
    for (const auto& candidate : candidates) {
        if (!std::filesystem::is_regular_file(candidate)) continue;
        std::wstring command_line = L"\"" + candidate.wstring() + L"\" --termite-hosted";
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (!CreateProcessW(candidate.c_str(), command_line.data(), nullptr, nullptr, FALSE, 0,
                            nullptr, candidate.parent_path().c_str(), &startup, &process)) {
            continue;
        }
        CloseHandle(process.hThread);
        frontend_process_ = process.hProcess;
        return true;
    }
    return false;
}

void termite_host::close_frontend_handle() noexcept {
    if (frontend_process_ == nullptr) return;
    CloseHandle(frontend_process_);
    frontend_process_ = nullptr;
}

void termite_host::apply_bridge_snapshot() {
    const auto snapshot = eq_bridge_.take_latest();
    if (!snapshot.has_value()) return;
    profile_ = profile_from_bridge_snapshot(*snapshot, profile_);
    audio_engine_.set_profile(profile_);
}

void termite_host::process_route_bridge_request(route_bridge_transaction_v1& transaction) {
    const auto& request = transaction.request;
    auto& response = transaction.response;
    response = {};
    response.sequence = request.sequence;

    const auto set_message = [&response](std::wstring_view text) {
        wcsncpy_s(response.message, text.data(), _TRUNCATE);
    };
    const auto paths_match = [](std::wstring_view left, std::wstring_view right) {
        return _wcsicmp(std::wstring{left}.c_str(), std::wstring{right}.c_str()) == 0;
    };
    const auto request_path_length = wcsnlen(request.executable_path, route_bridge_path_chars);
    const std::wstring requested_path{request.executable_path, request_path_length};

    if (request.command == static_cast<std::uint8_t>(route_bridge_command::set_route) &&
        (requested_path.empty() || request_path_length == route_bridge_path_chars)) {
        response.status = static_cast<std::uint32_t>(route_bridge_status::rejected);
        set_message(L"Choose an open app before changing its route.");
        return;
    }

    auto candidates = session_router_.open_apps();
    const auto find_automatic_route = [this, &paths_match](std::wstring_view executable) {
        return std::find_if(automatic_routes_.begin(), automatic_routes_.end(), [&paths_match, executable](const app_audio_route_snapshot& route) {
            return paths_match(route.executable_path, executable);
        });
    };

    if (request.command == static_cast<std::uint8_t>(route_bridge_command::set_route)) {
        const auto candidate = std::find_if(candidates.begin(), candidates.end(), [&paths_match, &requested_path](const routing_candidate& value) {
            return paths_match(value.executable_path, requested_path);
        });
        if (candidate == candidates.end()) {
            response.status = static_cast<std::uint32_t>(route_bridge_status::failed);
            set_message(L"That app is no longer open. Refresh the list and try again.");
        } else if (request.route_to_cable != 0) {
            if (find_automatic_route(candidate->executable_path) != automatic_routes_.end()) {
                set_message(L"Termite already routes this app to CABLE Input.");
            } else {
                app_audio_route_snapshot previous_route;
                std::wstring diagnostic;
                if (session_router_.route_to_cable(*candidate, previous_route, diagnostic)) {
                    automatic_routes_.push_back(std::move(previous_route));
                    set_message(diagnostic);
                } else {
                    response.status = static_cast<std::uint32_t>(route_bridge_status::failed);
                    set_message(diagnostic);
                }
            }
        } else {
            const auto route = find_automatic_route(candidate->executable_path);
            app_audio_route_snapshot fallback_route;
            const auto* route_to_restore = route != automatic_routes_.end() ? &*route : &fallback_route;
            if (route == automatic_routes_.end()) fallback_route.executable_path = candidate->executable_path;
            std::wstring diagnostic;
            if (session_router_.restore_route(*route_to_restore, diagnostic)) {
                if (route != automatic_routes_.end()) automatic_routes_.erase(route);
                set_message(diagnostic);
            } else {
                response.status = static_cast<std::uint32_t>(route_bridge_status::failed);
                set_message(diagnostic);
            }
        }
        candidates = session_router_.open_apps();
    }

    const auto app_count = std::min<std::size_t>(candidates.size(), route_bridge_max_apps);
    response.app_count = static_cast<std::uint32_t>(app_count);
    for (std::size_t index = 0; index < app_count; ++index) {
        const auto& source = candidates[index];
        auto& destination = response.apps[index];
        wcsncpy_s(destination.executable_path, source.executable_path.c_str(), _TRUNCATE);
        wcsncpy_s(destination.display_name, source.display_name.c_str(), _TRUNCATE);
        destination.open_window_count = static_cast<std::uint32_t>(source.open_window_count);
        destination.active_session_count = static_cast<std::uint32_t>(source.active_session_count);
        destination.routed_to_cable = source.saved_route_to_cable ||
                                      find_automatic_route(source.executable_path) != automatic_routes_.end();
        destination.reserved[0] = source.routed_to_cable ? 1 : 0;
    }
    if (response.message[0] == L'\0') {
        set_message(app_count == 0 ? L"No eligible open apps found." : L"Tick an app to route it through CABLE Input.");
    }
}

void termite_host::restore_automatic_routes() {
    for (const auto& route : automatic_routes_) {
        std::wstring diagnostic;
        static_cast<void>(session_router_.restore_route(route, diagnostic));
    }
    automatic_routes_.clear();
}

}  // namespace termite
