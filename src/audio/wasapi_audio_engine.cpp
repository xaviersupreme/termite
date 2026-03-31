#include "audio/wasapi_audio_engine.h"

#include "audio/audio_pipeline.h"

#include <Windows.h>
#include <Audioclient.h>
#include <Mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ks.h>
#include <ksmedia.h>
#include <wrl/client.h>

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cwctype>
#include <format>
#include <limits>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace termite {
namespace {

using Microsoft::WRL::ComPtr;

struct handle_deleter {
    void operator()(void* value) const noexcept {
        if (value != nullptr) CloseHandle(value);
    }
};

std::string narrow(std::wstring_view value) {
    if (value.empty()) return {};
    const auto length = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (length <= 0) return {};
    std::string result(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), length, nullptr, nullptr);
    return result;
}

std::string hresult_text(HRESULT result) {
    return std::format("Audio error 0x{:08X}", static_cast<std::uint32_t>(result));
}

std::wstring endpoint_name(IMMDevice* device) {
    if (device == nullptr) return {};
    ComPtr<IPropertyStore> store;
    if (FAILED(device->OpenPropertyStore(STGM_READ, &store))) return {};
    PROPVARIANT value{};
    PropVariantInit(&value);
    const auto result = store->GetValue(PKEY_Device_FriendlyName, &value);
    std::wstring name = result == S_OK && value.vt == VT_LPWSTR && value.pwszVal != nullptr ? value.pwszVal : L"";
    PropVariantClear(&value);
    return name;
}

std::string endpoint_id(IMMDevice* device) {
    if (device == nullptr) return {};
    LPWSTR raw{};
    if (FAILED(device->GetId(&raw)) || raw == nullptr) return {};
    const std::wstring id{raw};
    CoTaskMemFree(raw);
    return narrow(id);
}

bool contains_case_insensitive(std::wstring_view value, std::wstring_view needle) {
    if (needle.empty() || value.size() < needle.size()) return false;
    const auto lower = [](wchar_t character) { return static_cast<wchar_t>(std::towlower(character)); };
    for (std::size_t index = 0; index + needle.size() <= value.size(); ++index) {
        bool matched = true;
        for (std::size_t offset = 0; offset < needle.size(); ++offset) {
            if (lower(value[index + offset]) != lower(needle[offset])) {
                matched = false;
                break;
            }
        }
        if (matched) return true;
    }
    return false;
}

bool is_cable_output(IMMDevice* device) {
    return contains_case_insensitive(endpoint_name(device), L"CABLE Output");
}

bool is_cable_render(IMMDevice* device) {
    return contains_case_insensitive(endpoint_name(device), L"CABLE");
}

audio_format to_audio_format(const WAVEFORMATEX& wave) {
    audio_format result;
    result.sample_rate = wave.nSamplesPerSec;
    result.channels = wave.nChannels;
    if (wave.wFormatTag == WAVE_FORMAT_IEEE_FLOAT && wave.wBitsPerSample == 32) {
        result.encoding = sample_encoding::float32;
    } else if (wave.wFormatTag == WAVE_FORMAT_PCM) {
        switch (wave.wBitsPerSample) {
            case 16: result.encoding = sample_encoding::pcm_s16; break;
            case 24: result.encoding = sample_encoding::pcm_s24; break;
            case 32: result.encoding = sample_encoding::pcm_s32; break;
            default: break;
        }
    } else if (wave.wFormatTag == WAVE_FORMAT_EXTENSIBLE && wave.cbSize >= sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)) {
        const auto& extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE&>(wave);
        result.channel_mask = extensible.dwChannelMask;
        if (extensible.SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT && wave.wBitsPerSample == 32) {
            result.encoding = sample_encoding::float32;
        } else if (extensible.SubFormat == KSDATAFORMAT_SUBTYPE_PCM) {
            switch (wave.wBitsPerSample) {
                case 16: result.encoding = sample_encoding::pcm_s16; break;
                case 24: result.encoding = sample_encoding::pcm_s24; break;
                case 32: result.encoding = sample_encoding::pcm_s32; break;
                default: break;
            }
        }
    }
    return result;
}

const char* state_text(engine_state state) {
    switch (state) {
        case engine_state::stopped: return "Stopped";
        case engine_state::starting: return "Starting audio engine";
        case engine_state::running: return "Processing VB-CABLE to the default output";
        case engine_state::recovering: return "Recovering audio devices";
        case engine_state::cable_missing: return "VB-CABLE CABLE Output was not found";
        case engine_state::unsafe_output: return "Default output is VB-CABLE; choose speakers or headphones to avoid feedback";
        case engine_state::unsupported_format: return "An endpoint uses an unsupported shared-mode format";
        case engine_state::failed: return "Audio engine failed";
    }
    return "Audio engine failed";
}

class notification_registration {
public:
    notification_registration(IMMDeviceEnumerator* enumerator, endpoint_notification_client* client)
        : enumerator_(enumerator), client_(client) {}

    notification_registration(const notification_registration&) = delete;
    notification_registration& operator=(const notification_registration&) = delete;

    ~notification_registration();

    bool register_callback();

private:
    IMMDeviceEnumerator* enumerator_{};
    endpoint_notification_client* client_{};
    bool registered_{};
};

}  // namespace

struct stream_session {
    ComPtr<IAudioClient> capture_client;
    ComPtr<IAudioClient> render_client;
    ComPtr<IAudioCaptureClient> capture_service;
    ComPtr<IAudioRenderClient> render_service;
    HANDLE capture_event{};
    HANDLE render_event{};
    UINT32 capture_buffer_frames{};
    UINT32 render_buffer_frames{};
    audio_format capture_format;
    audio_format render_format;
    std::string capture_name;
    std::string render_name;
    std::string capture_id;
    std::string render_id;
    spsc_frame_ring ring;
    streaming_resampler resampler;
    drift_controller drift;
    channel_mapper mapper;
    std::vector<float> capture_scratch;
    std::vector<float> active_dsp_scratch;
    std::vector<float> pending_dsp_scratch;
    std::vector<float> resample_scratch;
    std::vector<float> render_scratch;
    std::atomic<bool> render_primed{};
    std::atomic<bool> failed{};
    std::atomic<HRESULT> failure_result{S_OK};

    ~stream_session() {
        if (capture_event != nullptr) CloseHandle(capture_event);
        if (render_event != nullptr) CloseHandle(render_event);
    }
};

class endpoint_notification_client final : public IMMNotificationClient {
public:
    explicit endpoint_notification_client(wasapi_audio_engine* owner) : owner_(owner) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** object) override {
        if (object == nullptr) return E_POINTER;
        if (id == __uuidof(IUnknown) || id == __uuidof(IMMNotificationClient)) {
            *object = static_cast<IMMNotificationClient*>(this);
            AddRef();
            return S_OK;
        }
        *object = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return references_.fetch_add(1) + 1U; }
    ULONG STDMETHODCALLTYPE Release() override {
        const auto remaining = references_.fetch_sub(1) - 1U;
        if (remaining == 0) delete this;
        return remaining;
    }

    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR) override {
        if (flow == eRender && role == eMultimedia && owner_ != nullptr) owner_->request_recovery();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override {
        if (owner_ != nullptr) owner_->request_recovery();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override {
        if (owner_ != nullptr) owner_->request_recovery();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override {
        if (owner_ != nullptr) owner_->request_recovery();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }

private:
    std::atomic<ULONG> references_{1};
    wasapi_audio_engine* owner_{};
};

namespace {

notification_registration::~notification_registration() {
    if (registered_ && enumerator_ != nullptr) enumerator_->UnregisterEndpointNotificationCallback(client_);
    if (client_ != nullptr) client_->Release();
}

bool notification_registration::register_callback() {
    if (enumerator_ == nullptr || client_ == nullptr) return false;
    registered_ = SUCCEEDED(enumerator_->RegisterEndpointNotificationCallback(client_));
    return registered_;
}

}  // namespace

wasapi_audio_engine::wasapi_audio_engine() {
    publish_profile(eq_profile::flat());
}

wasapi_audio_engine::~wasapi_audio_engine() {
    stop();
}

bool wasapi_audio_engine::start() {
    if (running_.exchange(true)) return true;

    const auto stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    const auto recovery_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (stop_event == nullptr || recovery_event == nullptr) {
        if (stop_event != nullptr) CloseHandle(stop_event);
        if (recovery_event != nullptr) CloseHandle(recovery_event);
        running_ = false;
        set_status("Could not create audio control events", engine_state::failed);
        return false;
    }
    stop_event_.store(stop_event, std::memory_order_release);
    recovery_event_.store(recovery_event, std::memory_order_release);
    stop_requested_ = false;
    capture_overflows_ = 0;
    render_underflows_ = 0;
    restart_count_ = 0;
    ring_fill_frames_ = 0;
    target_fill_frames_ = 0;
    set_status(state_text(engine_state::starting), engine_state::starting);
    audio_thread_ = std::thread(&wasapi_audio_engine::audio_loop, this);
    return true;
}

void wasapi_audio_engine::stop() {
    stop_requested_ = true;
    if (auto* event = static_cast<HANDLE>(stop_event_.load(std::memory_order_acquire)); event != nullptr) SetEvent(event);
    if (auto* event = static_cast<HANDLE>(recovery_event_.load(std::memory_order_acquire)); event != nullptr) SetEvent(event);
    if (audio_thread_.joinable()) audio_thread_.join();
    if (auto* event = static_cast<HANDLE>(stop_event_.exchange(nullptr, std::memory_order_acq_rel)); event != nullptr) CloseHandle(event);
    if (auto* event = static_cast<HANDLE>(recovery_event_.exchange(nullptr, std::memory_order_acq_rel)); event != nullptr) CloseHandle(event);
    running_ = false;
    ring_fill_frames_ = 0;
    target_fill_frames_ = 0;
    set_status(state_text(engine_state::stopped), engine_state::stopped);
}

void wasapi_audio_engine::set_profile(const eq_profile& profile) {
    publish_profile(profile);
}

bool wasapi_audio_engine::is_running() const noexcept {
    return running_.load(std::memory_order_acquire);
}

std::string wasapi_audio_engine::status_text() const {
    const std::scoped_lock lock(status_mutex_);
    return status_text_;
}

audio_diagnostics wasapi_audio_engine::diagnostics() const {
    const std::scoped_lock lock(status_mutex_);
    auto result = diagnostics_;
    result.ring_fill_frames = ring_fill_frames_.load(std::memory_order_acquire);
    result.target_fill_frames = target_fill_frames_.load(std::memory_order_acquire);
    result.capture_overflows = capture_overflows_.load(std::memory_order_acquire);
    result.render_underflows = render_underflows_.load(std::memory_order_acquire);
    result.restart_count = restart_count_.load(std::memory_order_acquire);
    return result;
}

void wasapi_audio_engine::publish_profile(const eq_profile& profile) noexcept {
    for (std::size_t index = 0; index < graphic_band_count; ++index) {
        band_gain_bits_[index].store(std::bit_cast<std::uint32_t>(profile.bands[index].gain_db), std::memory_order_relaxed);
        band_q_bits_[index].store(std::bit_cast<std::uint32_t>(profile.bands[index].q), std::memory_order_relaxed);
        band_shape_[index].store(static_cast<std::uint8_t>(profile.bands[index].shape), std::memory_order_relaxed);
        band_enabled_[index].store(profile.bands[index].enabled, std::memory_order_relaxed);
    }
    preamp_bits_.store(std::bit_cast<std::uint32_t>(profile.preamp_db), std::memory_order_relaxed);
    limiter_bits_.store(std::bit_cast<std::uint32_t>(profile.limiter_ceiling_db), std::memory_order_relaxed);
    profile_enabled_.store(profile.enabled, std::memory_order_relaxed);
    profile_revision_.fetch_add(1, std::memory_order_release);
}

eq_profile wasapi_audio_engine::snapshot_profile(std::uint64_t& revision) const noexcept {
    eq_profile profile;
    std::uint64_t before{};
    std::uint64_t after{};
    do {
        before = profile_revision_.load(std::memory_order_acquire);
        profile = eq_profile::flat();
        for (std::size_t index = 0; index < graphic_band_count; ++index) {
            profile.bands[index].gain_db = std::bit_cast<float>(band_gain_bits_[index].load(std::memory_order_relaxed));
            profile.bands[index].q = std::bit_cast<float>(band_q_bits_[index].load(std::memory_order_relaxed));
            const auto shape = band_shape_[index].load(std::memory_order_relaxed);
            profile.bands[index].shape = shape <= static_cast<std::uint8_t>(filter_shape::notch) ? static_cast<filter_shape>(shape) : filter_shape::peaking;
            profile.bands[index].enabled = band_enabled_[index].load(std::memory_order_relaxed);
        }
        profile.preamp_db = std::bit_cast<float>(preamp_bits_.load(std::memory_order_relaxed));
        profile.limiter_ceiling_db = std::bit_cast<float>(limiter_bits_.load(std::memory_order_relaxed));
        profile.enabled = profile_enabled_.load(std::memory_order_relaxed);
        after = profile_revision_.load(std::memory_order_acquire);
    } while (before != after);
    revision = after;
    return profile;
}

void wasapi_audio_engine::set_status(std::string value, engine_state state, std::string recovery_reason) {
    const std::scoped_lock lock(status_mutex_);
    status_text_ = std::move(value);
    diagnostics_.state = state;
    diagnostics_.recovery_reason = std::move(recovery_reason);
}

void wasapi_audio_engine::set_stream_diagnostics(const stream_session& session, engine_state state, std::string recovery_reason) {
    const std::scoped_lock lock(status_mutex_);
    diagnostics_.state = state;
    diagnostics_.capture_sample_rate = session.capture_format.sample_rate;
    diagnostics_.render_sample_rate = session.render_format.sample_rate;
    diagnostics_.capture_channels = session.capture_format.channels;
    diagnostics_.render_channels = session.render_format.channels;
    diagnostics_.capture_endpoint_name = session.capture_name;
    diagnostics_.render_endpoint_name = session.render_name;
    diagnostics_.capture_endpoint_id = session.capture_id;
    diagnostics_.render_endpoint_id = session.render_id;
    diagnostics_.recovery_reason = std::move(recovery_reason);
    status_text_ = state == engine_state::running ? "Processing VB-CABLE to " + session.render_name : state_text(state);
}

void wasapi_audio_engine::request_recovery() noexcept {
    if (auto* event = static_cast<HANDLE>(recovery_event_.load(std::memory_order_acquire)); event != nullptr) SetEvent(event);
}

void wasapi_audio_engine::audio_loop() {
    const auto com_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(com_result)) {
        set_status(hresult_text(com_result), engine_state::failed);
        running_ = false;
        return;
    }
    while (!stop_requested_.load(std::memory_order_acquire)) {
        if (auto* event = static_cast<HANDLE>(recovery_event_.load(std::memory_order_acquire)); event != nullptr) ResetEvent(event);
        const auto completed = run_stream();
        if (stop_requested_.load(std::memory_order_acquire)) break;
        if (!completed) {
            restart_count_.fetch_add(1, std::memory_order_acq_rel);
            const auto state = diagnostics().state;
            if (state != engine_state::cable_missing && state != engine_state::unsafe_output && state != engine_state::unsupported_format) {
                set_status("Recovering audio devices", engine_state::recovering, "Endpoint change or stream failure");
            }
            if (auto* event = static_cast<HANDLE>(stop_event_.load(std::memory_order_acquire)); event != nullptr) {
                WaitForSingleObject(event, 500);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }
    }
    CoUninitialize();
    running_ = false;
}

bool wasapi_audio_engine::run_stream() {
    ComPtr<IMMDeviceEnumerator> enumerator;
    auto result = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
    if (FAILED(result)) {
        set_status(hresult_text(result), engine_state::failed);
        return false;
    }
    auto* notification = new endpoint_notification_client(this);
    notification_registration registration(enumerator.Get(), notification);
    registration.register_callback();

    ComPtr<IMMDeviceCollection> capture_devices;
    result = enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &capture_devices);
    if (FAILED(result)) {
        set_status(hresult_text(result), engine_state::failed);
        return false;
    }
    UINT device_count{};
    capture_devices->GetCount(&device_count);
    ComPtr<IMMDevice> capture_device;
    for (UINT index = 0; index < device_count; ++index) {
        ComPtr<IMMDevice> candidate;
        if (SUCCEEDED(capture_devices->Item(index, &candidate)) && is_cable_output(candidate.Get())) {
            capture_device = candidate;
            break;
        }
    }
    if (capture_device == nullptr) {
        set_status(state_text(engine_state::cable_missing), engine_state::cable_missing);
        return false;
    }

    ComPtr<IMMDevice> render_device;
    result = enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &render_device);
    if (FAILED(result)) {
        set_status("No default output device", engine_state::failed);
        return false;
    }
    if (is_cable_render(render_device.Get())) {
        set_status(state_text(engine_state::unsafe_output), engine_state::unsafe_output);
        return false;
    }

    stream_session session;
    session.capture_name = narrow(endpoint_name(capture_device.Get()));
    session.render_name = narrow(endpoint_name(render_device.Get()));
    session.capture_id = endpoint_id(capture_device.Get());
    session.render_id = endpoint_id(render_device.Get());
    result = capture_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(session.capture_client.GetAddressOf()));
    if (FAILED(result)) {
        set_status("Could not open VB-CABLE capture: " + hresult_text(result), engine_state::failed);
        return false;
    }
    result = render_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(session.render_client.GetAddressOf()));
    if (FAILED(result)) {
        set_status("Could not open default output: " + hresult_text(result), engine_state::failed);
        return false;
    }

    WAVEFORMATEX* raw_capture_format{};
    WAVEFORMATEX* raw_render_format{};
    result = session.capture_client->GetMixFormat(&raw_capture_format);
    if (SUCCEEDED(result)) result = session.render_client->GetMixFormat(&raw_render_format);
    const std::unique_ptr<WAVEFORMATEX, decltype(&CoTaskMemFree)> capture_wave(raw_capture_format, CoTaskMemFree);
    const std::unique_ptr<WAVEFORMATEX, decltype(&CoTaskMemFree)> render_wave(raw_render_format, CoTaskMemFree);
    if (FAILED(result) || capture_wave == nullptr || render_wave == nullptr) {
        set_status("Could not negotiate endpoint formats", engine_state::failed);
        return false;
    }
    session.capture_format = to_audio_format(*capture_wave);
    session.render_format = to_audio_format(*render_wave);
    const auto negotiation = negotiate_endpoints(session.capture_format, session.render_format, false);
    if (!negotiation.valid()) {
        set_status(state_text(engine_state::unsupported_format), engine_state::unsupported_format);
        return false;
    }

    result = session.capture_client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 0, 0, capture_wave.get(), nullptr);
    if (FAILED(result)) {
        set_status("Could not initialize VB-CABLE capture: " + hresult_text(result), engine_state::failed);
        return false;
    }
    result = session.render_client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 0, 0, render_wave.get(), nullptr);
    if (FAILED(result)) {
        set_status("Could not initialize default output: " + hresult_text(result), engine_state::failed);
        return false;
    }
    result = session.capture_client->GetService(IID_PPV_ARGS(&session.capture_service));
    if (SUCCEEDED(result)) result = session.render_client->GetService(IID_PPV_ARGS(&session.render_service));
    if (FAILED(result) || session.capture_service == nullptr || session.render_service == nullptr) {
        set_status("Could not open audio stream services", engine_state::failed);
        return false;
    }
    if (FAILED(session.capture_client->GetBufferSize(&session.capture_buffer_frames)) ||
        FAILED(session.render_client->GetBufferSize(&session.render_buffer_frames))) {
        set_status("Could not query endpoint buffer sizes", engine_state::failed);
        return false;
    }
    session.capture_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    session.render_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (session.capture_event == nullptr || session.render_event == nullptr ||
        FAILED(session.capture_client->SetEventHandle(session.capture_event)) ||
        FAILED(session.render_client->SetEventHandle(session.render_event))) {
        set_status("Could not create endpoint events", engine_state::failed);
        return false;
    }

    const auto ring_capacity = std::max<std::size_t>(session.capture_format.sample_rate, session.capture_buffer_frames * 4U);
    if (!session.ring.configure(ring_capacity, session.capture_format.channels) ||
        !session.resampler.configure(session.capture_format.sample_rate, session.render_format.sample_rate, session.capture_format.channels) ||
        !session.mapper.configure(session.capture_format.channels, session.render_format.channels)) {
        set_status("Could not configure audio conversion pipeline", engine_state::unsupported_format);
        return false;
    }
    session.drift.configure(session.capture_format.sample_rate);
    target_fill_frames_.store(session.drift.target_frames(), std::memory_order_release);
    session.capture_scratch.assign(static_cast<std::size_t>(session.capture_buffer_frames) * session.capture_format.channels, 0.0F);
    session.active_dsp_scratch.assign(static_cast<std::size_t>(session.capture_buffer_frames) * session.capture_format.channels, 0.0F);
    session.pending_dsp_scratch.assign(static_cast<std::size_t>(session.capture_buffer_frames) * session.capture_format.channels, 0.0F);
    session.resample_scratch.assign(static_cast<std::size_t>(session.render_buffer_frames) * session.capture_format.channels, 0.0F);
    session.render_scratch.assign(static_cast<std::size_t>(session.render_buffer_frames) * session.render_format.channels, 0.0F);

    result = session.capture_client->Start();
    if (SUCCEEDED(result)) result = session.render_client->Start();
    if (FAILED(result)) {
        session.capture_client->Stop();
        session.render_client->Stop();
        set_status("Could not start audio streams: " + hresult_text(result), engine_state::failed);
        return false;
    }
    set_stream_diagnostics(session, engine_state::running);
    std::thread capture_thread(&wasapi_audio_engine::capture_worker, this, std::ref(session));
    std::thread render_thread(&wasapi_audio_engine::render_worker, this, std::ref(session));
    capture_thread.join();
    render_thread.join();
    session.capture_client->Stop();
    session.render_client->Stop();
    ring_fill_frames_ = 0;
    target_fill_frames_ = 0;

    if (session.failed.load(std::memory_order_acquire)) {
        set_stream_diagnostics(session, engine_state::recovering, hresult_text(session.failure_result.load(std::memory_order_acquire)));
        return false;
    }
    return stop_requested_.load(std::memory_order_acquire);
}

void wasapi_audio_engine::capture_worker(stream_session& session) {
    const auto com_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(com_result)) {
        session.failure_result = com_result;
        session.failed = true;
        request_recovery();
        return;
    }
    constexpr std::uint32_t profile_transition_ms = 10;
    eq_processor active_processor;
    eq_processor pending_processor;
    std::uint64_t active_revision = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t pending_revision{};
    std::size_t transition_frame{};
    std::size_t transition_frames{};
    const HANDLE events[]{session.capture_event,
                          static_cast<HANDLE>(stop_event_.load(std::memory_order_acquire)),
                          static_cast<HANDLE>(recovery_event_.load(std::memory_order_acquire))};
    while (!stop_requested_.load(std::memory_order_acquire) && !session.failed.load(std::memory_order_acquire)) {
        const auto waited = WaitForMultipleObjects(std::size(events), events, FALSE, 500);
        if (waited == WAIT_OBJECT_0 + 1U || waited == WAIT_OBJECT_0 + 2U) break;
        if (waited != WAIT_OBJECT_0) continue;
        UINT32 packet_frames{};
        HRESULT result{};
        while (SUCCEEDED(result = session.capture_service->GetNextPacketSize(&packet_frames)) && packet_frames != 0) {
            BYTE* capture_data{};
            DWORD flags{};
            result = session.capture_service->GetBuffer(&capture_data, &packet_frames, &flags, nullptr, nullptr);
            if (FAILED(result)) break;
            if (packet_frames > session.capture_buffer_frames) {
                session.capture_service->ReleaseBuffer(packet_frames);
                result = E_FAIL;
                break;
            }
            const auto sample_count = static_cast<std::size_t>(packet_frames) * session.capture_format.channels;
            if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0) {
                std::fill_n(session.capture_scratch.data(), sample_count, 0.0F);
            } else if (!decode_interleaved({reinterpret_cast<const std::byte*>(capture_data), sample_count * session.capture_format.bytes_per_sample()},
                                           session.capture_format, packet_frames, session.capture_scratch.data())) {
                session.capture_service->ReleaseBuffer(packet_frames);
                result = E_FAIL;
                break;
            }
            const auto requested_revision = profile_revision_.load(std::memory_order_acquire);
            if (active_revision == std::numeric_limits<std::uint64_t>::max()) {
                const auto profile = snapshot_profile(active_revision);
                active_processor.configure(profile, static_cast<float>(session.capture_format.sample_rate), session.capture_format.channels);
            }
            if (transition_frames == 0 && requested_revision != active_revision) {
                const auto profile = snapshot_profile(pending_revision);
                pending_processor.configure(profile, static_cast<float>(session.capture_format.sample_rate), session.capture_format.channels);
                transition_frame = 0;
                transition_frames = std::max<std::size_t>(1, static_cast<std::size_t>(session.capture_format.sample_rate) * profile_transition_ms / 1000U);
            }

            active_processor.process_interleaved(session.capture_scratch.data(), session.active_dsp_scratch.data(), packet_frames);
            if (transition_frames != 0) {
                pending_processor.process_interleaved(session.capture_scratch.data(), session.pending_dsp_scratch.data(), packet_frames);
                equal_power_crossfade(session.active_dsp_scratch.data(), session.pending_dsp_scratch.data(), session.active_dsp_scratch.data(),
                                      packet_frames, session.capture_format.channels, transition_frame, transition_frames);
                transition_frame += packet_frames;
                if (transition_frame >= transition_frames) {
                    active_processor = pending_processor;
                    active_revision = pending_revision;
                    transition_frames = 0;
                }
            }
            const auto pushed = session.ring.push(session.active_dsp_scratch.data(), packet_frames);
            if (pushed < packet_frames) capture_overflows_.fetch_add(packet_frames - pushed, std::memory_order_acq_rel);
            session.capture_service->ReleaseBuffer(packet_frames);
            ring_fill_frames_.store(session.ring.available_read(), std::memory_order_release);
        }
        if (FAILED(result)) {
            session.failure_result = result;
            session.failed = true;
            request_recovery();
            break;
        }
    }
    CoUninitialize();
}

void wasapi_audio_engine::render_worker(stream_session& session) {
    const auto com_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(com_result)) {
        session.failure_result = com_result;
        session.failed = true;
        request_recovery();
        return;
    }
    const HANDLE events[]{session.render_event,
                          static_cast<HANDLE>(stop_event_.load(std::memory_order_acquire)),
                          static_cast<HANDLE>(recovery_event_.load(std::memory_order_acquire))};
    while (!stop_requested_.load(std::memory_order_acquire) && !session.failed.load(std::memory_order_acquire)) {
        const auto waited = WaitForMultipleObjects(std::size(events), events, FALSE, 500);
        if (waited == WAIT_OBJECT_0 + 1U || waited == WAIT_OBJECT_0 + 2U) break;
        if (waited != WAIT_OBJECT_0) continue;
        UINT32 padding{};
        auto result = session.render_client->GetCurrentPadding(&padding);
        if (FAILED(result)) {
            session.failure_result = result;
            session.failed = true;
            request_recovery();
            break;
        }
        const auto writable_frames = session.render_buffer_frames - padding;
        if (writable_frames == 0) continue;
        BYTE* render_data{};
        result = session.render_service->GetBuffer(writable_frames, &render_data);
        if (FAILED(result)) {
            session.failure_result = result;
            session.failed = true;
            request_recovery();
            break;
        }
        const auto buffered_frames = session.ring.available_read();
        if (!session.render_primed.load(std::memory_order_acquire)) {
            if (buffered_frames < session.drift.target_frames()) {
                result = session.render_service->ReleaseBuffer(writable_frames, AUDCLNT_BUFFERFLAGS_SILENT);
                if (FAILED(result)) {
                    session.failure_result = result;
                    session.failed = true;
                    request_recovery();
                    break;
                }
                ring_fill_frames_.store(buffered_frames, std::memory_order_release);
                continue;
            }
            session.render_primed.store(true, std::memory_order_release);
        }
        const auto correction = session.drift.update(buffered_frames);
        const auto ratio = session.resampler.nominal_input_frames_per_output() * (1.0F + correction);
        const auto produced = session.resampler.render(session.ring, session.resample_scratch.data(), writable_frames, ratio);
        std::fill_n(session.render_scratch.data(), static_cast<std::size_t>(writable_frames) * session.render_format.channels, 0.0F);
        for (std::size_t frame = 0; frame < produced; ++frame) {
            session.mapper.map_frame(session.resample_scratch.data() + frame * session.capture_format.channels,
                                     session.render_scratch.data() + frame * session.render_format.channels);
        }
        if (produced < writable_frames) render_underflows_.fetch_add(writable_frames - produced, std::memory_order_acq_rel);
        if (!encode_interleaved(session.render_scratch.data(), session.render_format, writable_frames,
                                {reinterpret_cast<std::byte*>(render_data), static_cast<std::size_t>(writable_frames) * session.render_format.channels * session.render_format.bytes_per_sample()})) {
            session.render_service->ReleaseBuffer(writable_frames, AUDCLNT_BUFFERFLAGS_SILENT);
            session.failure_result = E_FAIL;
            session.failed = true;
            request_recovery();
            break;
        }
        result = session.render_service->ReleaseBuffer(writable_frames, 0);
        if (FAILED(result)) {
            session.failure_result = result;
            session.failed = true;
            request_recovery();
            break;
        }
        ring_fill_frames_.store(session.ring.available_read(), std::memory_order_release);
    }
    CoUninitialize();
}

}  // namespace termite
