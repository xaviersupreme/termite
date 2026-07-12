#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace termite {

inline constexpr std::size_t graphic_band_count = 20;
// The graphic EQ intentionally remains a 20-band control.  Arbitrary EQ is
// a separate curve processor: it needs enough centres to follow marker edits
// without pretending that twenty fixed sliders are an arbitrary response.
inline constexpr std::size_t arbitrary_band_count = 64;
// vSound 2's arbitrary editor spans 40 Hz..18 kHz.  This range is shared by
// the VCL marker editor and the pipe bridge, so the curve the user draws is
// never silently clamped to a different response in the audio host.
inline constexpr float arbitrary_min_frequency_hz = 40.0F;
inline constexpr float arbitrary_max_frequency_hz = 18000.0F;
inline constexpr std::array<float, graphic_band_count> graphic_band_frequencies{
    40.0F, 55.0F, 75.0F, 103.0F, 141.0F, 194.0F, 265.0F, 364.0F, 498.0F, 683.0F,
    937.0F, 1284.0F, 1760.0F, 2412.0F, 3306.0F, 4532.0F, 6213.0F, 8516.0F, 11673.0F, 16000.0F,
};

enum class filter_shape {
    low_shelf,
    peaking,
    high_shelf,
    low_pass,
    high_pass,
    notch,
};

struct eq_band {
    filter_shape shape{filter_shape::peaking};
    float frequency_hz{1000.0F};
    float gain_db{};
    float q{1.0F};
    bool enabled{true};
};

// These effects deliberately live in the profile instead of in the UI.  A
// profile revision is therefore one complete, click-free capture-side change.
struct tone_effects {
    bool bass_enabled{};
    float bass_db{};
    bool loudness_enabled{};
    float loudness_amount{};  // 0.0 - 1.0
    bool clarity_enabled{};
    float clarity_db{};
    bool stereo_enabled{};
    float stereo_width{1.0F}; // 0.5 - 1.5
    bool mono{};
    float balance{};          // -1.0 (left) - 1.0 (right)
};

enum class eq_processing_mode {
    graphic,
    arbitrary,
};

struct eq_profile {
    std::array<eq_band, graphic_band_count> bands{};
    std::array<eq_band, arbitrary_band_count> arbitrary_bands{};
    eq_processing_mode processing_mode{eq_processing_mode::graphic};
    tone_effects effects{};
    float preamp_db{};
    float limiter_ceiling_db{-1.0F};
    bool enabled{true};

    [[nodiscard]] static eq_profile flat() noexcept;
    [[nodiscard]] static eq_profile preset(std::string_view preset_name) noexcept;
};

[[nodiscard]] float arbitrary_band_frequency(std::size_t index) noexcept;
[[nodiscard]] std::string_view filter_shape_name(filter_shape shape) noexcept;
[[nodiscard]] filter_shape next_filter_shape(filter_shape shape) noexcept;
[[nodiscard]] float profile_response_db(const eq_profile& profile, float sample_rate, float frequency_hz) noexcept;

class biquad_filter {
public:
    void configure(const eq_band& band, float sample_rate) noexcept;
    void reset() noexcept;
    [[nodiscard]] float process(float sample) noexcept;
    [[nodiscard]] float response_db(float sample_rate, float frequency_hz) const noexcept;

private:
    void set_coefficients(double b0, double b1, double b2, double a0, double a1, double a2) noexcept;
    void configure_peaking(float sample_rate, float frequency_hz, float gain_db, float q) noexcept;
    void configure_shelf(float sample_rate, float frequency_hz, float gain_db, float slope, bool high_shelf) noexcept;
    void configure_pass(float sample_rate, float frequency_hz, float q, bool high_pass) noexcept;
    void configure_notch(float sample_rate, float frequency_hz, float q) noexcept;

    double b0_{1.0};
    double b1_{};
    double b2_{};
    double a1_{};
    double a2_{};
    double x1_{};
    double x2_{};
    double y1_{};
    double y2_{};
};

class eq_processor {
public:
    void configure(const eq_profile& profile, float sample_rate, std::size_t channels) noexcept;
    void reset() noexcept;
    void process_interleaved(float* samples, std::size_t frame_count) noexcept;
    void process_interleaved(const float* input, float* output, std::size_t frame_count) noexcept;
    [[nodiscard]] std::uint64_t take_limiter_clamp_count() noexcept;

private:
    static constexpr std::size_t max_channels = 8;
    std::array<std::array<biquad_filter, arbitrary_band_count>, max_channels> filters_{};
    eq_profile profile_{};
    std::size_t channels_{2};
    std::size_t active_band_count_{graphic_band_count};
    float preamp_linear_{1.0F};
    float limiter_ceiling_linear_{0.8912509F};
    std::uint64_t limiter_clamp_count_{};
};

// The complete capture-side processing chain.  It keeps the graphic EQ
// processor separately reusable while giving profile transitions one stateful
// object to crossfade.
class profile_processor {
public:
    void configure(const eq_profile& profile, float sample_rate, std::size_t channels) noexcept;
    void reset() noexcept;
    void process_interleaved(float* samples, std::size_t frame_count) noexcept;
    void process_interleaved(const float* input, float* output, std::size_t frame_count) noexcept;
    [[nodiscard]] std::uint64_t take_limiter_clamp_count() noexcept;

private:
    static constexpr std::size_t max_channels = 8;
    std::array<biquad_filter, max_channels> bass_filters_{};
    std::array<biquad_filter, max_channels> loudness_low_filters_{};
    std::array<biquad_filter, max_channels> loudness_high_filters_{};
    std::array<biquad_filter, max_channels> clarity_filters_{};
    eq_processor graphic_processor_{};
    eq_profile profile_{};
    std::size_t channels_{2};
    float left_balance_{1.0F};
    float right_balance_{1.0F};
};

}  // namespace termite
