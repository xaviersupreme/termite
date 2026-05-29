#include "audio/app_audio_policy.h"

#include <Windows.h>
#include <Audiopolicy.h>
#include <propsys.h>
#include <propkey.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <Mmdeviceapi.h>
#include <propvarutil.h>
#include <roapi.h>
#include <winstring.h>

#include <algorithm>
#include <cwctype>
#include <format>
#include <memory>
#include <optional>
#include <set>
#include <string_view>
#include <utility>

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

class scoped_hstring {
public:
    scoped_hstring() = default;

    explicit scoped_hstring(std::wstring_view value) {
        WindowsCreateString(value.data(), static_cast<UINT32>(value.size()), &value_);
    }

    scoped_hstring(const scoped_hstring&) = delete;
    scoped_hstring& operator=(const scoped_hstring&) = delete;

    ~scoped_hstring() {
        if (value_ != nullptr) WindowsDeleteString(value_);
    }

    [[nodiscard]] HSTRING get() const noexcept { return value_; }
    [[nodiscard]] bool valid() const noexcept { return value_ != nullptr; }

private:
    HSTRING value_{};
};

// This factory is the Windows component used by the Volume Mixer to persist
// desktop per-app endpoints. Microsoft does not publish this contract, so keep
// it isolated here and always provide a diagnostics-backed manual fallback.
struct __declspec(uuid("ab3d4648-e242-459f-b02f-541c70306324")) audio_policy_config_factory : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE unused_01() = 0;
    virtual HRESULT STDMETHODCALLTYPE unused_02() = 0;
    virtual HRESULT STDMETHODCALLTYPE unused_03() = 0;
    virtual HRESULT STDMETHODCALLTYPE unused_04() = 0;
    virtual HRESULT STDMETHODCALLTYPE unused_05() = 0;
    virtual HRESULT STDMETHODCALLTYPE unused_06() = 0;
    virtual HRESULT STDMETHODCALLTYPE unused_07() = 0;
    virtual HRESULT STDMETHODCALLTYPE unused_08() = 0;
    virtual HRESULT STDMETHODCALLTYPE unused_09() = 0;
    virtual HRESULT STDMETHODCALLTYPE unused_10() = 0;
    virtual HRESULT STDMETHODCALLTYPE unused_11() = 0;
    virtual HRESULT STDMETHODCALLTYPE unused_12() = 0;
    virtual HRESULT STDMETHODCALLTYPE unused_13() = 0;
    virtual HRESULT STDMETHODCALLTYPE unused_14() = 0;
    virtual HRESULT STDMETHODCALLTYPE unused_15() = 0;
    virtual HRESULT STDMETHODCALLTYPE unused_16() = 0;
    virtual HRESULT STDMETHODCALLTYPE unused_17() = 0;
    virtual HRESULT STDMETHODCALLTYPE unused_18() = 0;
    virtual HRESULT STDMETHODCALLTYPE unused_19() = 0;
    virtual HRESULT STDMETHODCALLTYPE set_persisted_default_audio_endpoint(UINT32 process_id,
                                                                            EDataFlow flow,
                                                                            ERole role,
                                                                            HSTRING endpoint_id) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_persisted_default_audio_endpoint(UINT32 process_id,
                                                                            EDataFlow flow,
                                                                            ERole role,
                                                                            HSTRING* endpoint_id) = 0;
    virtual HRESULT STDMETHODCALLTYPE clear_all_persisted_application_default_endpoints() = 0;
};

std::wstring lowercase(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t character) {
        return static_cast<wchar_t>(std::towlower(character));
    });
    return value;
}

bool contains_insensitive(std::wstring_view value, std::wstring_view needle) {
    return lowercase(std::wstring{value}).find(lowercase(std::wstring{needle})) != std::wstring::npos;
}

bool paths_match(std::wstring_view left, std::wstring_view right) {
    return _wcsicmp(std::wstring{left}.c_str(), std::wstring{right}.c_str()) == 0;
}

std::wstring process_path(DWORD process_id) {
    const auto process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
    if (process == nullptr) return L"";
    std::wstring path(32768, L'\0');
    DWORD length = static_cast<DWORD>(path.size());
    const auto succeeded = QueryFullProcessImageNameW(process, 0, path.data(), &length);
    CloseHandle(process);
    return succeeded ? path.substr(0, length) : L"";
}

std::vector<DWORD> active_audio_processes_for_executable(const std::wstring& executable_path) {
    std::set<DWORD> processes;
    IMMDeviceEnumerator* raw_enumerator = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&raw_enumerator)))) {
        return {};
    }
    const auto enumerator = adopt_com(raw_enumerator);
    IMMDeviceCollection* raw_devices = nullptr;
    if (FAILED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &raw_devices))) return {};
    const auto devices = adopt_com(raw_devices);
    UINT device_count{};
    devices->GetCount(&device_count);
    for (UINT device_index = 0; device_index < device_count; ++device_index) {
        IMMDevice* raw_device = nullptr;
        if (FAILED(devices->Item(device_index, &raw_device))) continue;
        const auto device = adopt_com(raw_device);
        IAudioSessionManager2* raw_manager = nullptr;
        if (FAILED(device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&raw_manager)))) continue;
        const auto manager = adopt_com(raw_manager);
        IAudioSessionEnumerator* raw_sessions = nullptr;
        if (FAILED(manager->GetSessionEnumerator(&raw_sessions))) continue;
        const auto sessions = adopt_com(raw_sessions);
        int session_count{};
        sessions->GetCount(&session_count);
        for (int session_index = 0; session_index < session_count; ++session_index) {
            IAudioSessionControl* raw_control = nullptr;
            if (FAILED(sessions->GetSession(session_index, &raw_control))) continue;
            const auto control = adopt_com(raw_control);
            AudioSessionState state{};
            if (FAILED(control->GetState(&state)) || state != AudioSessionStateActive) continue;
            IAudioSessionControl2* raw_control2 = nullptr;
            if (FAILED(control->QueryInterface(IID_PPV_ARGS(&raw_control2)))) continue;
            const auto control2 = adopt_com(raw_control2);
            DWORD process_id{};
            if (SUCCEEDED(control2->GetProcessId(&process_id)) && process_id != 0 &&
                paths_match(process_path(process_id), executable_path)) {
                processes.insert(process_id);
            }
        }
    }
    return {processes.begin(), processes.end()};
}

std::wstring hresult_text(HRESULT result) {
    return std::format(L"0x{:08X}", static_cast<unsigned long>(result));
}

std::optional<std::wstring> cable_input_endpoint_id(std::wstring& diagnostic) {
    IMMDeviceEnumerator* raw_enumerator = nullptr;
    const auto create_result = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&raw_enumerator));
    if (FAILED(create_result)) {
        diagnostic = std::format(L"Could not enumerate output devices ({})", hresult_text(create_result));
        return std::nullopt;
    }
    const auto enumerator = adopt_com(raw_enumerator);
    IMMDeviceCollection* raw_devices = nullptr;
    const auto enumerate_result = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &raw_devices);
    if (FAILED(enumerate_result)) {
        diagnostic = std::format(L"Could not enumerate active output devices ({})", hresult_text(enumerate_result));
        return std::nullopt;
    }
    const auto devices = adopt_com(raw_devices);
    UINT count{};
    devices->GetCount(&count);
    for (UINT index = 0; index < count; ++index) {
        IMMDevice* raw_device = nullptr;
        if (FAILED(devices->Item(index, &raw_device))) continue;
        const auto device = adopt_com(raw_device);
        IPropertyStore* raw_store = nullptr;
        if (FAILED(device->OpenPropertyStore(STGM_READ, &raw_store))) continue;
        const auto store = adopt_com(raw_store);
        PROPVARIANT name{};
        PropVariantInit(&name);
        const auto property_result = store->GetValue(PKEY_Device_FriendlyName, &name);
        const bool is_cable_input = SUCCEEDED(property_result) && name.vt == VT_LPWSTR && name.pwszVal != nullptr &&
                                    contains_insensitive(name.pwszVal, L"CABLE Input");
        PropVariantClear(&name);
        if (!is_cable_input) continue;

        LPWSTR raw_id = nullptr;
        const auto id_result = device->GetId(&raw_id);
        if (FAILED(id_result) || raw_id == nullptr) {
            diagnostic = std::format(L"Could not read CABLE Input's endpoint id ({})", hresult_text(id_result));
            return std::nullopt;
        }
        std::wstring id{raw_id};
        CoTaskMemFree(raw_id);
        // The policy component consumes the same device-interface path that
        // Windows stores under DefaultEndpoint, not IMMDevice::GetId()'s
        // shorter endpoint key.
        return std::format(L"\\\\?\\SWD#MMDEVAPI#{}#{{e6327cad-dcec-4949-ae8a-991e976a79d2}}", id);
    }
    diagnostic = L"CABLE Input is not an active Windows output device.";
    return std::nullopt;
}

com_ptr<audio_policy_config_factory> open_policy_factory(std::wstring& diagnostic) {
    scoped_hstring class_name{L"Windows.Media.Internal.AudioPolicyConfig"};
    if (!class_name.valid()) {
        diagnostic = L"Could not create the Windows audio-policy class name.";
        return {nullptr, [](audio_policy_config_factory*) {}};
    }
    audio_policy_config_factory* raw_factory = nullptr;
    const auto result = RoGetActivationFactory(class_name.get(), __uuidof(audio_policy_config_factory), reinterpret_cast<void**>(&raw_factory));
    if (FAILED(result) || raw_factory == nullptr) {
        diagnostic = std::format(L"Windows did not expose automatic app routing ({})", hresult_text(result));
        return {nullptr, [](audio_policy_config_factory*) {}};
    }
    return adopt_com(raw_factory);
}

struct endpoint_preference {
    bool assigned{};
    std::wstring endpoint_id;
};

endpoint_preference get_endpoint_preference(audio_policy_config_factory& factory, DWORD process_id, ERole role) {
    HSTRING raw_endpoint{};
    const auto result = factory.get_persisted_default_audio_endpoint(process_id, eRender, role, &raw_endpoint);
    if (FAILED(result) || raw_endpoint == nullptr) return {};
    const auto text = WindowsGetStringRawBuffer(raw_endpoint, nullptr);
    endpoint_preference preference{true, text == nullptr ? L"" : text};
    WindowsDeleteString(raw_endpoint);
    return preference;
}

bool set_endpoint_preference(audio_policy_config_factory& factory,
                             DWORD process_id,
                             ERole role,
                             const std::wstring* endpoint_id,
                             std::wstring& diagnostic) {
    scoped_hstring endpoint{endpoint_id == nullptr ? std::wstring_view{} : std::wstring_view{*endpoint_id}};
    if (endpoint_id != nullptr && !endpoint.valid()) {
        diagnostic = L"Could not create the Windows endpoint id.";
        return false;
    }
    const auto result = factory.set_persisted_default_audio_endpoint(process_id, eRender, role,
                                                                       endpoint_id == nullptr ? nullptr : endpoint.get());
    if (FAILED(result)) {
        diagnostic = std::format(L"Windows rejected the app route ({})", hresult_text(result));
        return false;
    }
    return true;
}

bool set_all_render_roles(audio_policy_config_factory& factory,
                          DWORD process_id,
                          const std::wstring* console_endpoint,
                          const std::wstring* multimedia_endpoint,
                          std::wstring& diagnostic) {
    return set_endpoint_preference(factory, process_id, eConsole, console_endpoint, diagnostic) &&
           set_endpoint_preference(factory, process_id, eMultimedia, multimedia_endpoint, diagnostic);
}

}  // namespace

bool app_audio_policy::route_executable_to_cable(const std::wstring& executable_path,
                                                  app_audio_route_snapshot& previous_route,
                                                  std::wstring& diagnostic) const {
    const auto processes = active_audio_processes_for_executable(executable_path);
    if (processes.empty()) {
        diagnostic = L"The selected app does not have an active audio stream yet. Start playback, then try again.";
        return false;
    }
    const auto cable_endpoint = cable_input_endpoint_id(diagnostic);
    if (!cable_endpoint.has_value()) return false;
    const auto factory = open_policy_factory(diagnostic);
    if (factory == nullptr) return false;

    previous_route = {};
    previous_route.executable_path = executable_path;
    previous_route.process_ids.assign(processes.begin(), processes.end());
    const auto console = get_endpoint_preference(*factory, processes.front(), eConsole);
    const auto multimedia = get_endpoint_preference(*factory, processes.front(), eMultimedia);
    previous_route.had_console_endpoint = console.assigned;
    previous_route.console_endpoint_id = console.endpoint_id;
    previous_route.had_multimedia_endpoint = multimedia.assigned;
    previous_route.multimedia_endpoint_id = multimedia.endpoint_id;

    for (const auto process_id : processes) {
        if (!set_all_render_roles(*factory, process_id, &*cable_endpoint, &*cable_endpoint, diagnostic)) return false;
    }
    diagnostic = std::format(L"Routed {} running process{} to CABLE Input.", processes.size(), processes.size() == 1 ? L"" : L"es");
    return true;
}

bool app_audio_policy::restore_executable_route(const app_audio_route_snapshot& previous_route,
                                                 std::wstring& diagnostic) const {
    if (previous_route.executable_path.empty()) return true;
    std::set<DWORD> process_ids;
    for (const auto process_id : previous_route.process_ids) {
        if (paths_match(process_path(process_id), previous_route.executable_path)) {
            process_ids.insert(process_id);
        }
    }
    // Browser and media apps can create their actual render client after the
    // route was applied. Restore every current audio process as well as the
    // original snapshot so those later streams do not remain on CABLE Input.
    for (const auto process_id : active_audio_processes_for_executable(previous_route.executable_path)) {
        process_ids.insert(process_id);
    }
    const std::vector<DWORD> processes{process_ids.begin(), process_ids.end()};
    if (processes.empty()) {
        diagnostic = L"The routed process has already exited. Its per-process route will not affect a new app instance.";
        return false;
    }
    const auto factory = open_policy_factory(diagnostic);
    if (factory == nullptr) return false;

    const std::wstring* console = previous_route.had_console_endpoint ? &previous_route.console_endpoint_id : nullptr;
    const std::wstring* multimedia = previous_route.had_multimedia_endpoint ? &previous_route.multimedia_endpoint_id : nullptr;
    for (const auto process_id : processes) {
        if (!set_all_render_roles(*factory, process_id, console, multimedia, diagnostic)) return false;
    }
    diagnostic = L"Restored the app's previous Windows output preference.";
    return true;
}

bool app_audio_policy::is_executable_routed_to_cable(const std::wstring& executable_path) const {
    std::wstring ignored;
    const auto processes = active_audio_processes_for_executable(executable_path);
    if (processes.empty()) return false;
    const auto cable_endpoint = cable_input_endpoint_id(ignored);
    if (!cable_endpoint.has_value()) return false;
    const auto factory = open_policy_factory(ignored);
    if (factory == nullptr) return false;
    const auto multimedia = get_endpoint_preference(*factory, processes.front(), eMultimedia);
    return multimedia.assigned && paths_match(multimedia.endpoint_id, *cable_endpoint);
}

}  // namespace termite
