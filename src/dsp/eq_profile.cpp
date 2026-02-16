#include "dsp/eq_profile.h"

#include <algorithm>
#include <cmath>

namespace termite {
namespace {

constexpr double pi = 3.14159265358979323846;

float db_to_linear(float db) noexcept {
    return std::pow(10.0F, db / 20.0F);
}

float clamp_frequency(float frequency_hz, float sample_rate) noexcept {
    return std::clamp(frequency_hz, 20.0F, sample_rate * 0.45F);
}

float clamp_gain(float gain_db) noexcept {
    return std::clamp(gain_db, -24.0F, 24.0F);
}

float clamp_q(float q) noexcept {
    return std::clamp(q, 0.15F, 12.0F);
}

}  // namespace

eq_profile eq_profile::flat() noexcept {
    eq_profile profile;
    for (std::size_t index = 0; index < profile.bands.size(); ++index) {
        profile.bands[index] = {filter_shape::peaking, graphic_band_frequencies[index], 0.0F, 4.32F};
    }
    return profile;
}

eq_profile eq_profile::preset(std::string_view preset_name) noexcept {
    auto profile = flat();
    if (preset_name == "bass") {
        constexpr std::array<float, 20> gains{7.0F, 7.0F, 6.0F, 5.0F, 4.0F, 2.0F, 0.5F, -1.0F, -1.5F, -1.5F,
            -1.0F, -0.5F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F};
        for (std::size_t index = 0; index < profile.bands.size(); ++index) {
            profile.bands[index].gain_db = gains[index];
        }
        profile.preamp_db = -6.0F;
    } else if (preset_name == "vocal") {
        constexpr std::array<float, 20> gains{-3.0F, -3.0F, -3.0F, -2.5F, -2.0F, -1.5F, -1.0F, 0.0F, 1.0F, 2.0F,
            3.0F, 3.5F, 3.0F, 2.0F, 1.5F, 1.0F, 0.5F, 0.0F, 0.0F, 0.0F};
        for (std::size_t index = 0; index < profile.bands.size(); ++index) {
            profile.bands[index].gain_db = gains[index];
        }
        profile.preamp_db = -3.5F;
    } else if (preset_name == "bright") {
        constexpr std::array<float, 20> gains{0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.5F, 1.0F,
            1.5F, 2.0F, 2.5F, 3.0F, 3.5F, 4.0F, 4.0F, 3.5F, 3.0F, 2.5F};
        for (std::size_t index = 0; index < profile.bands.size(); ++index) {
            profile.bands[index].gain_db = gains[index];
        }
        profile.preamp_db = -4.0F;
    }
    return profile;
}

std::string_view filter_shape_name(filter_shape shape) noexcept {
    switch (shape) {
        case filter_shape::low_shelf: return "LOW SHELF";
        case filter_shape::peaking: return "PEAK";
        case filter_shape::high_shelf: return "HIGH SHELF";
        case filter_shape::low_pass: return "LOW PASS";
        case filter_shape::high_pass: return "HIGH PASS";
        case filter_shape::notch: return "NOTCH";
    }
    return "PEAK";
}

filter_shape next_filter_shape(filter_shape shape) noexcept {
    constexpr std::array shapes{
        filter_shape::low_shelf,
        filter_shape::peaking,
        filter_shape::high_shelf,
        filter_shape::low_pass,
        filter_shape::high_pass,
        filter_shape::notch,
    };
    const auto found = std::find(shapes.begin(), shapes.end(), shape);
    return found == shapes.end() || std::next(found) == shapes.end() ? shapes.front() : *std::next(found);
}

float profile_response_db(const eq_profile& profile, float sample_rate, float frequency_hz) noexcept {
    if (!profile.enabled) {
        return 0.0F;
    }

    float response = profile.preamp_db;
    for (const auto& band : profile.bands) {
        if (!band.enabled) continue;
        biquad_filter filter;
        filter.configure(band, sample_rate);
        response += filter.response_db(sample_rate, frequency_hz);
    }
    return response;
}

void biquad_filter::configure(const eq_band& band, float sample_rate) noexcept {
    const auto gain_filter = band.shape == filter_shape::peaking || band.shape == filter_shape::low_shelf || band.shape == filter_shape::high_shelf;
    if (!band.enabled || sample_rate < 1000.0F || (gain_filter && std::abs(band.gain_db) < 0.0001F)) {
        b0_ = 1.0F;
        b1_ = b2_ = a1_ = a2_ = 0.0F;
        return;
    }

    switch (band.shape) {
        case filter_shape::low_shelf:
            configure_shelf(sample_rate, band.frequency_hz, band.gain_db, band.q, false);
            break;
        case filter_shape::peaking:
            configure_peaking(sample_rate, band.frequency_hz, band.gain_db, band.q);
            break;
        case filter_shape::high_shelf:
            configure_shelf(sample_rate, band.frequency_hz, band.gain_db, band.q, true);
            break;
        case filter_shape::low_pass:
            configure_pass(sample_rate, band.frequency_hz, band.q, false);
            break;
        case filter_shape::high_pass:
            configure_pass(sample_rate, band.frequency_hz, band.q, true);
            break;
        case filter_shape::notch:
            configure_notch(sample_rate, band.frequency_hz, band.q);
            break;
    }
}

void biquad_filter::set_coefficients(double b0, double b1, double b2, double a0, double a1, double a2) noexcept {
    const auto safe_a0 = std::abs(a0) < 0.000000001 ? 1.0 : a0;
    b0_ = b0 / safe_a0;
    b1_ = b1 / safe_a0;
    b2_ = b2 / safe_a0;
    a1_ = a1 / safe_a0;
    a2_ = a2 / safe_a0;
}

void biquad_filter::configure_peaking(float sample_rate, float frequency_hz, float gain_db, float q) noexcept {
    const auto omega = 2.0 * pi * static_cast<double>(clamp_frequency(frequency_hz, sample_rate)) / sample_rate;
    const auto alpha = std::sin(omega) / (2.0 * clamp_q(q));
    const auto cosine = std::cos(omega);
    const auto amplitude = std::pow(10.0, static_cast<double>(clamp_gain(gain_db)) / 40.0);

    set_coefficients(
        1.0 + alpha * amplitude,
        -2.0 * cosine,
        1.0 - alpha * amplitude,
        1.0 + alpha / amplitude,
        -2.0 * cosine,
        1.0 - alpha / amplitude);
}

void biquad_filter::configure_shelf(float sample_rate, float frequency_hz, float gain_db, float slope, bool high_shelf) noexcept {
    const auto omega = 2.0 * pi * static_cast<double>(clamp_frequency(frequency_hz, sample_rate)) / sample_rate;
    const auto cosine = std::cos(omega);
    const auto amplitude = std::pow(10.0, static_cast<double>(clamp_gain(gain_db)) / 40.0);
    const auto root_amplitude = std::sqrt(amplitude);
    const auto shelf_slope = std::clamp(static_cast<double>(slope), 0.15, 1.0);
    const auto alpha = std::sin(omega) * 0.5 * std::sqrt((amplitude + 1.0 / amplitude) * (1.0 / shelf_slope - 1.0) + 2.0);
    const auto beta = 2.0 * root_amplitude * alpha;

    if (high_shelf) {
        set_coefficients(
            amplitude * ((amplitude + 1.0) + (amplitude - 1.0) * cosine + beta),
            -2.0 * amplitude * ((amplitude - 1.0) + (amplitude + 1.0) * cosine),
            amplitude * ((amplitude + 1.0) + (amplitude - 1.0) * cosine - beta),
            (amplitude + 1.0) - (amplitude - 1.0) * cosine + beta,
            2.0 * ((amplitude - 1.0) - (amplitude + 1.0) * cosine),
            (amplitude + 1.0) - (amplitude - 1.0) * cosine - beta);
    } else {
        set_coefficients(
            amplitude * ((amplitude + 1.0) - (amplitude - 1.0) * cosine + beta),
            2.0 * amplitude * ((amplitude - 1.0) - (amplitude + 1.0) * cosine),
            amplitude * ((amplitude + 1.0) - (amplitude - 1.0) * cosine - beta),
            (amplitude + 1.0) + (amplitude - 1.0) * cosine + beta,
            -2.0 * ((amplitude - 1.0) + (amplitude + 1.0) * cosine),
            (amplitude + 1.0) + (amplitude - 1.0) * cosine - beta);
    }
}

void biquad_filter::configure_pass(float sample_rate, float frequency_hz, float q, bool high_pass) noexcept {
    const auto omega = 2.0 * pi * static_cast<double>(clamp_frequency(frequency_hz, sample_rate)) / sample_rate;
    const auto cosine = std::cos(omega);
    const auto alpha = std::sin(omega) / (2.0 * clamp_q(q));
    const auto b0 = high_pass ? (1.0 + cosine) * 0.5 : (1.0 - cosine) * 0.5;
    const auto b1 = high_pass ? -(1.0 + cosine) : 1.0 - cosine;

    set_coefficients(b0, b1, b0, 1.0 + alpha, -2.0 * cosine, 1.0 - alpha);
}

void biquad_filter::configure_notch(float sample_rate, float frequency_hz, float q) noexcept {
    const auto omega = 2.0 * pi * static_cast<double>(clamp_frequency(frequency_hz, sample_rate)) / sample_rate;
    const auto alpha = std::sin(omega) / (2.0 * clamp_q(q));
    const auto cosine = std::cos(omega);
    set_coefficients(1.0, -2.0 * cosine, 1.0, 1.0 + alpha, -2.0 * cosine, 1.0 - alpha);
}

void biquad_filter::reset() noexcept {
    x1_ = x2_ = y1_ = y2_ = 0.0F;
}

float biquad_filter::process(float sample) noexcept {
    const auto output = b0_ * sample + b1_ * x1_ + b2_ * x2_ - a1_ * y1_ - a2_ * y2_;
    x2_ = x1_;
    x1_ = sample;
    y2_ = y1_;
    y1_ = output;
    return static_cast<float>(output);
}

float biquad_filter::response_db(float sample_rate, float frequency_hz) const noexcept {
    const auto omega = 2.0 * pi * static_cast<double>(clamp_frequency(frequency_hz, sample_rate)) / sample_rate;
    const auto cosine = std::cos(omega);
    const auto sine = std::sin(omega);
    const auto cosine_2 = std::cos(2.0F * omega);
    const auto sine_2 = std::sin(2.0F * omega);
    const auto numerator_real = b0_ + b1_ * cosine + b2_ * cosine_2;
    const auto numerator_imaginary = -b1_ * sine - b2_ * sine_2;
    const auto denominator_real = 1.0F + a1_ * cosine + a2_ * cosine_2;
    const auto denominator_imaginary = -a1_ * sine - a2_ * sine_2;
    const auto numerator = numerator_real * numerator_real + numerator_imaginary * numerator_imaginary;
    const auto denominator = denominator_real * denominator_real + denominator_imaginary * denominator_imaginary;
    return static_cast<float>(10.0 * std::log10(std::max(numerator / std::max(denominator, 0.000000000001), 0.000000000001)));
}

void eq_processor::configure(const eq_profile& profile, float sample_rate, std::size_t channels) noexcept {
    profile_ = profile;
    channels_ = std::clamp<std::size_t>(channels, 1, max_channels);
    preamp_linear_ = db_to_linear(std::clamp(profile.preamp_db, -24.0F, 12.0F));
    limiter_ceiling_linear_ = db_to_linear(std::clamp(profile.limiter_ceiling_db, -12.0F, 0.0F));
    for (std::size_t channel = 0; channel < channels_; ++channel) {
        for (std::size_t band = 0; band < graphic_band_count; ++band) {
            filters_[channel][band].configure(profile.bands[band], sample_rate);
        }
    }
    reset();
}

void eq_processor::reset() noexcept {
    for (auto& channel_filters : filters_) {
        for (auto& filter : channel_filters) {
            filter.reset();
        }
    }
}

void eq_processor::process_interleaved(float* samples, std::size_t frame_count) noexcept {
    if (!profile_.enabled) {
        return;
    }
    for (std::size_t frame = 0; frame < frame_count; ++frame) {
        for (std::size_t channel = 0; channel < channels_; ++channel) {
            auto sample = samples[frame * channels_ + channel];
            for (auto& filter : filters_[channel]) {
                sample = filter.process(sample);
            }
            sample *= preamp_linear_;
            samples[frame * channels_ + channel] = std::clamp(sample, -limiter_ceiling_linear_, limiter_ceiling_linear_);
        }
    }
}

}  // namespace termite
