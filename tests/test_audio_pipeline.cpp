#include "sound/audio_pipeline.h"
#include "sound/routing_policy.h"

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <vector>

namespace {

bool close_to(float left, float right, float tolerance = 0.002F) {
    return std::abs(left - right) <= tolerance;
}

void test_sample_conversion() {
    const termite::audio_format formats[]{
        {termite::sample_encoding::float32, 48000, 2, 0},
        {termite::sample_encoding::pcm_s16, 48000, 2, 0},
        {termite::sample_encoding::pcm_s24, 48000, 2, 0},
        {termite::sample_encoding::pcm_s32, 48000, 2, 0},
    };
    constexpr std::array<float, 4> input{-1.0F, -0.25F, 0.25F, 1.0F};
    for (const auto& format : formats) {
        std::vector<std::byte> encoded(input.size() * format.bytes_per_sample());
        assert(termite::encode_interleaved(input.data(), format, 2, encoded));
        std::array<float, 4> decoded{};
        assert(termite::decode_interleaved(encoded, format, 2, decoded.data()));
        for (std::size_t index = 0; index < input.size(); ++index) assert(close_to(decoded[index], input[index], 0.0002F));
    }

    const termite::audio_format unsupported{termite::sample_encoding::unsupported, 48000, 2, 0};
    std::array<std::byte, 8> buffer{};
    assert(!termite::decode_interleaved(buffer, unsupported, 1, reinterpret_cast<float*>(buffer.data())));
}

void test_ring_buffer() {
    termite::spsc_frame_ring ring;
    assert(ring.configure(4, 2));
    constexpr std::array<float, 12> first{0.0F, 0.1F, 0.2F, 0.3F, 0.4F, 0.5F, 0.6F, 0.7F, 0.8F, 0.9F, -0.2F, -0.1F};
    assert(ring.push(first.data(), 6) == 4);
    assert(ring.available_read() == 4);
    std::array<float, 4> read{};
    assert(ring.read(read.data(), 2) == 2);
    assert(read[0] == 0.0F && read[3] == 0.3F);
    constexpr std::array<float, 6> second{-0.8F, -0.7F, -0.6F, -0.5F, -0.4F, -0.3F};
    assert(ring.push(second.data(), 3) == 2);
    assert(ring.available_read() == 4);
    std::array<float, 8> remaining{};
    assert(ring.read(remaining.data(), 4) == 4);
    assert(remaining[0] == 0.4F && remaining[1] == 0.5F);
    assert(remaining[4] == -0.8F && remaining[5] == -0.7F);
}

void test_channel_mapping() {
    termite::channel_mapper mapper;
    std::array<float, termite::audio_max_channels> output{};

    assert(mapper.configure(1, 2));
    mapper.map_frame(std::array<float, 1>{0.4F}.data(), output.data());
    assert(close_to(output[0], 0.4F) && close_to(output[1], 0.4F));

    assert(mapper.configure(2, 1));
    mapper.map_frame(std::array<float, 2>{0.2F, 0.6F}.data(), output.data());
    assert(close_to(output[0], 0.4F));

    assert(mapper.configure(2, 6));
    mapper.map_frame(std::array<float, 2>{-0.5F, 0.5F}.data(), output.data());
    assert(close_to(output[0], -0.5F) && close_to(output[1], 0.5F));
    for (std::size_t channel = 2; channel < 6; ++channel) assert(close_to(output[channel], 0.0F));

    assert(mapper.configure(6, 2));
    mapper.map_frame(std::array<float, 6>{0.2F, 0.4F, 0.1F, 0.0F, 0.1F, 0.2F}.data(), output.data());
    assert(output[0] > 0.2F && output[1] > 0.4F);
}

void test_resampling_and_drift() {
    termite::spsc_frame_ring ring;
    assert(ring.configure(256, 1));
    std::array<float, 128> source{};
    source.fill(0.25F);
    assert(ring.push(source.data(), source.size()) == source.size());

    termite::streaming_resampler resampler;
    assert(resampler.configure(48000, 44100, 1));
    std::array<float, 64> output{};
    const auto produced = resampler.render(ring, output.data(), output.size(), resampler.nominal_input_frames_per_output());
    assert(produced > 20);
    for (std::size_t index = 8; index < produced; ++index) {
        assert(std::isfinite(output[index]));
        assert(close_to(output[index], 0.25F, 0.03F));
    }

    termite::drift_controller controller;
    controller.configure(48000, 120);
    const auto empty_correction = controller.update(0);
    const auto full_correction = controller.update(controller.target_frames() * 2);
    assert(empty_correction < 0.0F);
    assert(full_correction > empty_correction);
    assert(std::abs(full_correction) <= 0.0025F);
}

void test_profile_crossfade() {
    constexpr std::array<float, 16> active{
        0.25F, 0.25F, 0.25F, 0.25F, 0.25F, 0.25F, 0.25F, 0.25F,
        0.25F, 0.25F, 0.25F, 0.25F, 0.25F, 0.25F, 0.25F, 0.25F,
    };
    constexpr std::array<float, 16> pending{
        -0.25F, -0.25F, -0.25F, -0.25F, -0.25F, -0.25F, -0.25F, -0.25F,
        -0.25F, -0.25F, -0.25F, -0.25F, -0.25F, -0.25F, -0.25F, -0.25F,
    };
    std::array<float, 16> output{};
    termite::equal_power_crossfade(active.data(), pending.data(), output.data(), 8, 2, 0, 8);
    for (const auto sample : output) {
        assert(std::isfinite(sample));
        assert(std::abs(sample) <= 1.0F);
    }
    assert(output.front() > 0.0F);
    assert(output.back() < 0.0F);
    for (std::size_t index = 1; index < output.size(); ++index) {
        assert(std::abs(output[index] - output[index - 1]) < 0.12F);
    }
}

void test_endpoint_negotiation() {
    const termite::audio_format capture{termite::sample_encoding::float32, 44100, 2, 0};
    const termite::audio_format render{termite::sample_encoding::pcm_s24, 48000, 6, 0};
    assert(termite::negotiate_endpoints(capture, render, false).valid());
    const auto unsafe = termite::negotiate_endpoints(capture, render, true);
    assert(unsafe.error == termite::endpoint_negotiation_error::unsafe_render_endpoint);
    const termite::audio_format unsupported{termite::sample_encoding::unsupported, 48000, 2, 0};
    assert(termite::negotiate_endpoints(unsupported, render, false).error == termite::endpoint_negotiation_error::unsupported_capture_format);
}

void test_monitor_analyzer() {
    termite::audio_monitor_analyzer analyzer;
    analyzer.configure(48000, 2);
    std::vector<float> input(4800 * 2);
    std::vector<float> output(input.size());
    for (std::size_t frame = 0; frame < 4800; ++frame) {
        const auto sample = std::sin(6.28318530718F * 1000.0F * static_cast<float>(frame) / 48000.0F) * 0.5F;
        input[frame * 2] = sample;
        input[frame * 2 + 1] = sample;
        output[frame * 2] = sample * 0.25F;
        output[frame * 2 + 1] = sample * 0.25F;
    }
    assert(analyzer.process(input.data(), output.data(), 4800, 3));
    const auto& snapshot = analyzer.snapshot();
    std::size_t strongest{};
    for (std::size_t index = 1; index < termite::audio_monitor_band_count; ++index) {
        if (snapshot.input_spectrum_db[index] > snapshot.input_spectrum_db[strongest]) strongest = index;
    }
    assert(termite::monitor_band_frequency_hz(strongest) > 700.0F && termite::monitor_band_frequency_hz(strongest) < 1400.0F);
    assert(snapshot.input_rms_left > snapshot.output_rms_left);
    assert(snapshot.stereo_input);
    assert(snapshot.limiter_clamp_count >= 3);
}

void test_routing_policy() {
    termite::routing_process_metadata browser{4242, true, false, true, false, L"C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe"};
    assert(termite::is_eligible_routing_process(browser));
    browser.executable_path = L"C:\\Windows\\System32\\svchost.exe";
    assert(!termite::is_eligible_routing_process(browser));
    browser.executable_path = L"C:\\Apps\\Player.exe";
    browser.has_visible_window = true;
    assert(termite::is_eligible_routing_process(browser));
    browser.system_sounds = true;
    assert(!termite::is_eligible_routing_process(browser));

    termite::open_app_metadata open_app{4242, true, true, L"C:\\Apps\\Player.exe"};
    assert(termite::is_eligible_open_app(open_app));
    open_app.has_visible_top_level_window = false;
    assert(!termite::is_eligible_open_app(open_app));
    open_app.has_visible_top_level_window = true;
    open_app.current_user_session = false;
    assert(!termite::is_eligible_open_app(open_app));
    open_app.current_user_session = true;
    open_app.executable_path = L"C:\\Windows\\SystemApps\\MicrosoftWindows.Client.CBS_cw5n1h2txyewy\\TextInputHost.exe";
    assert(!termite::is_eligible_open_app(open_app));

    assert(termite::normalized_executable_key(L"C:\\Apps\\PLAYER.EXE") == L"c:\\apps\\player.exe");
    assert(termite::executable_display_name(L"C:\\Apps\\Player.exe") == L"Player");
}

}  // namespace

int main() {
    test_sample_conversion();
    test_ring_buffer();
    test_channel_mapping();
    test_resampling_and_drift();
    test_profile_crossfade();
    test_endpoint_negotiation();
    test_monitor_analyzer();
    test_routing_policy();
}
