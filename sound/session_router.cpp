#include "sound/session_router.h"

#include "sound/routing_policy.h"

#include <Windows.h>
#include <Audioclient.h>
#include <Audiopolicy.h>
#include <propkey.h>
#include <functiondiscoverykeys_devpkey.h>
#include <Mmdeviceapi.h>
#include <shellapi.h>

#include <algorithm>
#include <cwctype>
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

bool endpoint_is_cable_input(IMMDevice* device) {
    IPropertyStore* raw_store{};
    if (device == nullptr || FAILED(device->OpenPropertyStore(STGM_READ, &raw_store))) return false;
    const auto store = adopt_com(raw_store);
    PROPVARIANT value{};
    PropVariantInit(&value);
    const auto result = store->GetValue(PKEY_Device_FriendlyName, &value);
    const std::wstring name = result == S_OK && value.vt == VT_LPWSTR && value.pwszVal != nullptr ? value.pwszVal : L"";
    PropVariantClear(&value);
    std::wstring lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t value) { return static_cast<wchar_t>(std::towlower(value)); });
    return lower.find(L"cable input") != std::wstring::npos;
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
    IMMDeviceCollection* raw_devices{};
    if (FAILED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &raw_devices))) return sessions;
    const auto devices = adopt_com(raw_devices);
    UINT device_count{};
    devices->GetCount(&device_count);
    std::map<std::wstring, audio_session_info> grouped;
    for (UINT device_index = 0; device_index < device_count; ++device_index) {
        IMMDevice* raw_device{};
        if (FAILED(devices->Item(device_index, &raw_device))) continue;
        const auto device = adopt_com(raw_device);
        IAudioSessionManager2* raw_manager{};
        if (FAILED(device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&raw_manager)))) continue;
        const auto manager = adopt_com(raw_manager);
        IAudioSessionEnumerator* raw_sessions{};
        if (FAILED(manager->GetSessionEnumerator(&raw_sessions))) continue;
        const auto session_list = adopt_com(raw_sessions);
        int count{};
        session_list->GetCount(&count);
        const auto cable = endpoint_is_cable_input(device.get());
        for (int index = 0; index < count; ++index) {
        IAudioSessionControl* raw_control = nullptr;
        if (FAILED(session_list->GetSession(index, &raw_control))) continue;
        const auto control = adopt_com(raw_control);
        IAudioSessionControl2* raw_control2 = nullptr;
        if (FAILED(control->QueryInterface(IID_PPV_ARGS(&raw_control2)))) continue;
        const auto control2 = adopt_com(raw_control2);
        DWORD process_id{};
        if (FAILED(control2->GetProcessId(&process_id)) || process_id == 0) continue;
        LPWSTR raw_name = nullptr;
        control->GetDisplayName(&raw_name);
        std::wstring display_name = raw_name != nullptr ? raw_name : L"";
        CoTaskMemFree(raw_name);
        const auto executable = process_path(process_id);
        if (display_name.empty()) display_name = executable;
        const auto key = normalized_executable_key(executable);
        auto [item, inserted] = grouped.try_emplace(key);
        if (inserted) item->second = {process_id, executable, display_name, cable};
        else item->second.routed_to_cable = item->second.routed_to_cable || cable;
        }
    }
    for (auto& [_, session] : grouped) sessions.push_back(std::move(session));
    return sessions;
}

std::vector<routing_candidate> session_router::open_apps() const {
    std::vector<routing_candidate> apps;
    DWORD current_session{};
    ProcessIdToSessionId(GetCurrentProcessId(), &current_session);
    open_app_enumerator enumeration{current_session, GetCurrentProcessId()};
    EnumWindows(&collect_open_app, reinterpret_cast<LPARAM>(&enumeration));
    const auto sessions = active_sessions();
    for (const auto& session : sessions) {
        const auto found = enumeration.grouped.find(normalized_executable_key(session.executable_path));
        if (found == enumeration.grouped.end()) continue;
        ++found->second.active_session_count;
        found->second.routed_to_cable = found->second.routed_to_cable || session.routed_to_cable;
    }
    // A persisted per-app endpoint can exist even while an app is silent. Keep
    // it separate from the live session endpoint: an existing Firefox stream
    // can remain on Windows output until playback restarts after routing.
    const app_audio_policy policy;
    for (auto& [_, candidate] : enumeration.grouped) {
        candidate.saved_route_to_cable =
            policy.is_executable_routed_to_cable(candidate.executable_path);
    }
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
