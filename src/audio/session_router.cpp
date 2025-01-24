#include "audio/session_router.h"

#include "audio/routing_policy.h"

#include <Windows.h>
#include <Audioclient.h>
#include <Audiopolicy.h>
#include <Mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
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

bool contains_case_insensitive(const std::wstring& value, std::wstring_view needle) {
    if (value.size() < needle.size()) return false;
    for (std::size_t index = 0; index + needle.size() <= value.size(); ++index) {
        bool matched = true;
        for (std::size_t offset = 0; offset < needle.size(); ++offset) {
            if (std::towlower(value[index + offset]) != std::towlower(needle[offset])) {
                matched = false;
                break;
            }
        }
        if (matched) return true;
    }
    return false;
}

std::wstring device_name(IMMDevice* device) {
    if (device == nullptr) return {};
    IPropertyStore* raw_store = nullptr;
    if (FAILED(device->OpenPropertyStore(STGM_READ, &raw_store))) return {};
    const auto store = adopt_com(raw_store);
    PROPVARIANT name{};
    const auto result = store->GetValue(PKEY_Device_FriendlyName, &name);
    const std::wstring value = result == S_OK && name.vt == VT_LPWSTR && name.pwszVal != nullptr ? name.pwszVal : L"";
    PropVariantClear(&name);
    return value;
}

struct window_probe {
    DWORD process_id{};
    bool visible{};
};

BOOL CALLBACK find_visible_window(HWND window, LPARAM parameter) {
    auto& probe = *reinterpret_cast<window_probe*>(parameter);
    DWORD process_id{};
    GetWindowThreadProcessId(window, &process_id);
    if (process_id != probe.process_id || !IsWindowVisible(window) || GetWindow(window, GW_OWNER) != nullptr) return TRUE;
    const auto style = static_cast<DWORD>(GetWindowLongPtrW(window, GWL_STYLE));
    const auto extended = static_cast<DWORD>(GetWindowLongPtrW(window, GWL_EXSTYLE));
    if ((style & WS_CHILD) != 0 || (extended & WS_EX_TOOLWINDOW) != 0 || GetWindowTextLengthW(window) == 0) return TRUE;
    probe.visible = true;
    return FALSE;
}

bool has_visible_window(DWORD process_id) {
    window_probe probe{process_id};
    EnumWindows(&find_visible_window, reinterpret_cast<LPARAM>(&probe));
    return probe.visible;
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

std::vector<routing_candidate> session_router::eligible_sessions() const {
    std::vector<routing_candidate> sessions;
    IMMDeviceEnumerator* raw_enumerator = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&raw_enumerator)))) {
        return sessions;
    }
    const auto enumerator = adopt_com(raw_enumerator);
    IMMDeviceCollection* raw_devices = nullptr;
    if (FAILED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &raw_devices))) return sessions;
    const auto devices = adopt_com(raw_devices);
    UINT device_count{};
    devices->GetCount(&device_count);
    DWORD current_session{};
    ProcessIdToSessionId(GetCurrentProcessId(), &current_session);
    std::map<std::wstring, routing_candidate> grouped;

    for (UINT device_index = 0; device_index < device_count; ++device_index) {
        IMMDevice* raw_device = nullptr;
        if (FAILED(devices->Item(device_index, &raw_device))) continue;
        const auto device = adopt_com(raw_device);
        const bool cable_input = contains_case_insensitive(device_name(device.get()), L"CABLE Input");
        IAudioSessionManager2* raw_manager = nullptr;
        if (FAILED(device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&raw_manager)))) continue;
        const auto manager = adopt_com(raw_manager);
        IAudioSessionEnumerator* raw_sessions = nullptr;
        if (FAILED(manager->GetSessionEnumerator(&raw_sessions))) continue;
        const auto session_list = adopt_com(raw_sessions);
        int count{};
        session_list->GetCount(&count);
        for (int index = 0; index < count; ++index) {
            IAudioSessionControl* raw_control = nullptr;
            if (FAILED(session_list->GetSession(index, &raw_control))) continue;
            const auto control = adopt_com(raw_control);
            AudioSessionState state{};
            if (FAILED(control->GetState(&state)) || state != AudioSessionStateActive) continue;
            IAudioSessionControl2* raw_control2 = nullptr;
            if (FAILED(control->QueryInterface(IID_PPV_ARGS(&raw_control2)))) continue;
            const auto control2 = adopt_com(raw_control2);
            DWORD process_id{};
            if (FAILED(control2->GetProcessId(&process_id))) continue;
            DWORD process_session{};
            const auto executable = process_path(process_id);
            routing_process_metadata metadata{
                process_id,
                true,
                control2->IsSystemSoundsSession() == S_OK,
                ProcessIdToSessionId(process_id, &process_session) != FALSE && process_session == current_session,
                has_visible_window(process_id),
                executable,
            };
            if (!is_eligible_routing_process(metadata)) continue;

            LPWSTR raw_name = nullptr;
            control->GetDisplayName(&raw_name);
            const std::wstring display_name = raw_name != nullptr && raw_name[0] != L'\0' ? raw_name : executable_display_name(executable);
            CoTaskMemFree(raw_name);
            const auto key = normalized_executable_key(executable);
            auto [iterator, inserted] = grouped.try_emplace(key);
            auto& candidate = iterator->second;
            if (inserted) {
                candidate.executable_path = executable;
                candidate.display_name = display_name;
            }
            ++candidate.active_session_count;
            candidate.routed_to_cable = candidate.routed_to_cable || cable_input;
        }
    }
    sessions.reserve(grouped.size());
    for (auto& [_, candidate] : grouped) sessions.push_back(std::move(candidate));
    std::sort(sessions.begin(), sessions.end(), [](const routing_candidate& left, const routing_candidate& right) {
        return left.display_name < right.display_name;
    });
    return sessions;
}

bool session_router::route_to_cable(const audio_session_info&, std::wstring& failure_reason) const {
    // Windows exposes session enumeration publicly, but not a supported API for changing
    // another application's endpoint. Keep this boundary explicit and reliable.
    failure_reason = L"Windows does not provide a supported per-app output assignment API. Open Sound settings and set the app output to CABLE Input.";
    return false;
}

void session_router::open_manual_routing_settings() {
    ShellExecuteW(nullptr, L"open", L"ms-settings:apps-volume", nullptr, nullptr, SW_SHOWNORMAL);
}

}  // namespace termite
