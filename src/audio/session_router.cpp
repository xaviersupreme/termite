#include "audio/session_router.h"

#include "audio/routing_policy.h"

#include <Windows.h>
#include <Audioclient.h>
#include <Audiopolicy.h>
#include <Mmdeviceapi.h>
#include <shellapi.h>

#include <algorithm>
#include <map>
#include <memory>
#include <set>

namespace termite {
namespace {

template <typename type>
using com_ptr = std::unique_ptr<type, void (*)(type*)>;

template <typename type>
com_ptr<type> adopt_com(type* pointer) {
    return {pointer, [](type* value) {
        if (value != nullptr) value->Release();
    }};
}

std::wstring process_path(DWORD process_id) {
    const auto process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
    if (process == nullptr) return L"";
    std::wstring path(MAX_PATH, L'\0');
    DWORD length = static_cast<DWORD>(path.size());
    const auto succeeded = QueryFullProcessImageNameW(process, 0, path.data(), &length);
    CloseHandle(process);
    return succeeded ? path.substr(0, length) : L"";
}

struct open_app_enumerator {
    DWORD current_session{};
    DWORD current_process{};
    std::map<std::wstring, routing_candidate> grouped;
};

BOOL CALLBACK collect_open_app(HWND window, LPARAM parameter) {
    auto& enumeration = *reinterpret_cast<open_app_enumerator*>(parameter);
    DWORD process_id{};
    GetWindowThreadProcessId(window, &process_id);
    if (process_id == 0 || process_id == enumeration.current_process || window == GetShellWindow() ||
        !IsWindowVisible(window) || GetWindow(window, GW_OWNER) != nullptr) return TRUE;
    const auto style = static_cast<DWORD>(GetWindowLongPtrW(window, GWL_STYLE));
    const auto extended = static_cast<DWORD>(GetWindowLongPtrW(window, GWL_EXSTYLE));
    if ((style & WS_CHILD) != 0 || (extended & WS_EX_TOOLWINDOW) != 0 || GetWindowTextLengthW(window) == 0) return TRUE;

    DWORD process_session{};
    const auto executable = process_path(process_id);
    const open_app_metadata metadata{
        process_id,
        ProcessIdToSessionId(process_id, &process_session) != FALSE && process_session == enumeration.current_session,
        true,
        executable,
    };
    if (!is_eligible_open_app(metadata)) return TRUE;

    const auto key = normalized_executable_key(executable);
    auto [iterator, inserted] = enumeration.grouped.try_emplace(key);
    auto& candidate = iterator->second;
    if (inserted) {
        candidate.executable_path = executable;
        candidate.display_name = executable_display_name(executable);
    }
    ++candidate.open_window_count;
    return TRUE;
}

}  // namespace

std::vector<audio_session_info> session_router::active_sessions() const {
    std::vector<audio_session_info> sessions;
    IMMDeviceEnumerator* raw_enumerator = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&raw_enumerator)))) {
        return sessions;
    }
    const auto enumerator = adopt_com(raw_enumerator);
    IMMDevice* raw_device = nullptr;
    if (FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &raw_device))) return sessions;
    const auto device = adopt_com(raw_device);
    IAudioSessionManager2* raw_manager = nullptr;
    if (FAILED(device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&raw_manager)))) {
        return sessions;
    }
    const auto manager = adopt_com(raw_manager);
    IAudioSessionEnumerator* raw_sessions = nullptr;
    if (FAILED(manager->GetSessionEnumerator(&raw_sessions))) return sessions;
    const auto session_list = adopt_com(raw_sessions);
    int count{};
    session_list->GetCount(&count);
    std::set<DWORD> seen_processes;
    for (int index = 0; index < count; ++index) {
        IAudioSessionControl* raw_control = nullptr;
        if (FAILED(session_list->GetSession(index, &raw_control))) continue;
        const auto control = adopt_com(raw_control);
        IAudioSessionControl2* raw_control2 = nullptr;
        if (FAILED(control->QueryInterface(IID_PPV_ARGS(&raw_control2)))) continue;
        const auto control2 = adopt_com(raw_control2);
        DWORD process_id{};
        if (FAILED(control2->GetProcessId(&process_id)) || process_id == 0 || !seen_processes.insert(process_id).second) continue;
        LPWSTR raw_name = nullptr;
        control->GetDisplayName(&raw_name);
        std::wstring display_name = raw_name != nullptr ? raw_name : L"";
        CoTaskMemFree(raw_name);
        const auto executable = process_path(process_id);
        if (display_name.empty()) display_name = executable;
        sessions.push_back({process_id, executable, display_name, false});
    }
    return sessions;
}

std::vector<routing_candidate> session_router::open_apps() const {
    std::vector<routing_candidate> apps;
    DWORD current_session{};
    ProcessIdToSessionId(GetCurrentProcessId(), &current_session);
    open_app_enumerator enumeration{current_session, GetCurrentProcessId()};
    EnumWindows(&collect_open_app, reinterpret_cast<LPARAM>(&enumeration));
    apps.reserve(enumeration.grouped.size());
    for (auto& [_, candidate] : enumeration.grouped) apps.push_back(std::move(candidate));
    std::sort(apps.begin(), apps.end(), [](const routing_candidate& left, const routing_candidate& right) {
        return left.display_name < right.display_name;
    });
    return apps;
}

std::vector<routing_candidate> session_router::eligible_sessions() const {
    return open_apps();
}

bool session_router::route_to_cable(const audio_session_info&, std::wstring& failure_reason) const {
    failure_reason = L"Use the Route apps picker to select a visible app before applying its route.";
    return false;
}

bool session_router::route_to_cable(const routing_candidate& candidate,
                                    app_audio_route_snapshot& previous_route,
                                    std::wstring& diagnostic) const {
    return app_audio_policy{}.route_executable_to_cable(candidate.executable_path, previous_route, diagnostic);
}

bool session_router::restore_route(const app_audio_route_snapshot& previous_route,
                                   std::wstring& diagnostic) const {
    return app_audio_policy{}.restore_executable_route(previous_route, diagnostic);
}

void session_router::open_manual_routing_settings() {
    ShellExecuteW(nullptr, L"open", L"ms-settings:apps-volume", nullptr, nullptr, SW_SHOWNORMAL);
}

}  // namespace termite
