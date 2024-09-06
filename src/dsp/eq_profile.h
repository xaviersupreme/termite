#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace termite {

inline constexpr std::size_t graphic_band_count = 20;

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

struct eq_profile {
    std::array<eq_band, graphic_band_count> bands{};
    float preamp_db{};
    float limiter_ceiling_db{-1.0F};
    bool enabled{true};

    [[nodiscard]] static eq_profile flat() noexcept;
    [[nodiscard]] static eq_profile preset(std::string_view preset_name) noexcept;
};

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

private:
    static constexpr std::size_t max_channels = 8;
    std::array<std::array<biquad_filter, graphic_band_count>, max_channels> filters_{};
    eq_profile profile_{};
    std::size_t channels_{2};
    float preamp_linear_{1.0F};
    float limiter_ceiling_linear_{0.8912509F};
};

}  // namespace termite
