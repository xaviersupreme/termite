#pragma once

#include "audio/audio_pipeline.h"
#include "dsp/eq_profile.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace termite {

struct stream_session;
class endpoint_notification_client;

enum class engine_state : std::uint8_t {
    stopped,
    starting,
    running,
    recovering,
    cable_missing,
    unsafe_output,
    unsupported_format,
    failed,
};

struct audio_diagnostics {
    engine_state state{engine_state::stopped};
    std::uint32_t capture_sample_rate{};
    std::uint32_t render_sample_rate{};
    std::uint16_t capture_channels{};
    std::uint16_t render_channels{};
    std::size_t ring_fill_frames{};
    std::size_t target_fill_frames{};
    std::uint64_t capture_overflows{};
    std::uint64_t render_underflows{};
    std::uint64_t restart_count{};
    std::string capture_endpoint_name;
    std::string render_endpoint_name;
    std::string capture_endpoint_id;
    std::string render_endpoint_id;
    std::string recovery_reason;
};

class wasapi_audio_engine {
public:
    wasapi_audio_engine();
    ~wasapi_audio_engine();

    wasapi_audio_engine(const wasapi_audio_engine&) = delete;
    wasapi_audio_engine& operator=(const wasapi_audio_engine&) = delete;

    bool start();
    void stop();
    void set_profile(const eq_profile& profile);
    [[nodiscard]] bool is_running() const noexcept;
    [[nodiscard]] std::string status_text() const;
    [[nodiscard]] audio_diagnostics diagnostics() const;
    [[nodiscard]] audio_monitor_snapshot monitor() const noexcept;

private:
    friend class endpoint_notification_client;
    void audio_loop();
    bool run_stream();
    void capture_worker(stream_session& session);
    void render_worker(stream_session& session);
    void request_recovery() noexcept;
    void publish_profile(const eq_profile& profile) noexcept;
    [[nodiscard]] eq_profile snapshot_profile(std::uint64_t& revision) const noexcept;
    void publish_monitor(const audio_monitor_snapshot& snapshot) noexcept;
    void set_status(std::string value, engine_state state, std::string recovery_reason = {});
    void set_stream_diagnostics(const stream_session& session, engine_state state, std::string recovery_reason = {});

    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> running_{false};
    std::atomic<void*> stop_event_{nullptr};
    std::atomic<void*> recovery_event_{nullptr};
    std::atomic<std::uint64_t> capture_overflows_{};
    std::atomic<std::uint64_t> render_underflows_{};
    std::atomic<std::uint64_t> restart_count_{};
    std::atomic<std::size_t> ring_fill_frames_{};
    std::atomic<std::size_t> target_fill_frames_{};

    std::array<std::atomic<std::uint32_t>, graphic_band_count> band_gain_bits_{};
    std::array<std::atomic<std::uint32_t>, graphic_band_count> band_q_bits_{};
    std::array<std::atomic<std::uint8_t>, graphic_band_count> band_shape_{};
    std::array<std::atomic<bool>, graphic_band_count> band_enabled_{};
    std::atomic<std::uint32_t> preamp_bits_{};
    std::atomic<std::uint32_t> limiter_bits_{};
    std::atomic<bool> profile_enabled_{true};
    std::atomic<bool> bass_enabled_{};
    std::atomic<std::uint32_t> bass_bits_{};
    std::atomic<bool> loudness_enabled_{};
    std::atomic<std::uint32_t> loudness_bits_{};
    std::atomic<bool> clarity_enabled_{};
    std::atomic<std::uint32_t> clarity_bits_{};
    std::atomic<bool> stereo_enabled_{};
    std::atomic<std::uint32_t> stereo_width_bits_{};
    std::atomic<bool> mono_{};
    std::atomic<std::uint32_t> balance_bits_{};
    std::atomic<std::uint64_t> profile_revision_{};

    std::array<std::atomic<std::uint32_t>, audio_monitor_band_count> monitor_input_spectrum_bits_{};
    std::array<std::atomic<std::uint32_t>, audio_monitor_band_count> monitor_output_spectrum_bits_{};
    std::atomic<std::uint32_t> monitor_input_peak_left_bits_{};
    std::atomic<std::uint32_t> monitor_input_peak_right_bits_{};
    std::atomic<std::uint32_t> monitor_input_rms_left_bits_{};
    std::atomic<std::uint32_t> monitor_input_rms_right_bits_{};
    std::atomic<std::uint32_t> monitor_output_peak_left_bits_{};
    std::atomic<std::uint32_t> monitor_output_peak_right_bits_{};
    std::atomic<std::uint32_t> monitor_output_rms_left_bits_{};
    std::atomic<std::uint32_t> monitor_output_rms_right_bits_{};
    std::atomic<std::uint64_t> monitor_limiter_clamps_{};
    std::atomic<bool> monitor_stereo_input_{};
    std::atomic<std::uint64_t> monitor_revision_{};

    std::thread audio_thread_;
    mutable std::mutex status_mutex_;
    std::string status_text_{"Stopped"};
    audio_diagnostics diagnostics_{};
};

}  // namespace termite
