#include "sound/eq/eq_profile.h"

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

const eq_band& active_band(const eq_profile& profile, std::size_t index) noexcept {
    return profile.processing_mode == eq_processing_mode::arbitrary
        ? profile.arbitrary_bands[index]
        : profile.bands[index];
}

std::size_t active_band_count(const eq_profile& profile) noexcept {
    return profile.processing_mode == eq_processing_mode::arbitrary
        ? arbitrary_band_count
        : graphic_band_count;
}

eq_band bass_shelf(const tone_effects& effects) noexcept {
    return {filter_shape::low_shelf, 95.0F, std::clamp(effects.bass_db, -12.0F, 12.0F), 0.8F, effects.bass_enabled};
}

eq_band loudness_low_shelf(const tone_effects& effects) noexcept {
    return {filter_shape::low_shelf, 110.0F, std::clamp(effects.loudness_amount, 0.0F, 1.0F) * 8.0F, 0.7F, effects.loudness_enabled};
}

eq_band loudness_high_shelf(const tone_effects& effects) noexcept {
    return {filter_shape::high_shelf, 6500.0F, std::clamp(effects.loudness_amount, 0.0F, 1.0F) * 4.0F, 0.7F, effects.loudness_enabled};
}

eq_band clarity_shelf(const tone_effects& effects) noexcept {
    return {filter_shape::high_shelf, 4500.0F, std::clamp(effects.clarity_db, -8.0F, 8.0F), 0.8F, effects.clarity_enabled};
}

void apply_graphic_gains(eq_profile& profile, const std::array<float, graphic_band_count>& gains, float preamp_db) noexcept {
    for (std::size_t index = 0; index < profile.bands.size(); ++index) {
        profile.bands[index].gain_db = gains[index];
    }
    profile.preamp_db = preamp_db;
}

}  // namespace

eq_profile eq_profile::flat() noexcept {
    eq_profile profile;
    for (std::size_t index = 0; index < profile.bands.size(); ++index) {
        profile.bands[index] = {filter_shape::peaking, graphic_band_frequencies[index], 0.0F, 4.32F};
    }
    for (std::size_t index = 0; index < profile.arbitrary_bands.size(); ++index) {
        profile.arbitrary_bands[index] = {filter_shape::peaking, arbitrary_band_frequency(index), 0.0F, 10.0F};
    }
    return profile;
}

float arbitrary_band_frequency(std::size_t index) noexcept {
    const auto safe_index = std::min(index, arbitrary_band_count - 1);
    const auto ratio = static_cast<float>(safe_index) / static_cast<float>(arbitrary_band_count - 1);
    return arbitrary_min_frequency_hz * std::pow(arbitrary_max_frequency_hz / arbitrary_min_frequency_hz, ratio);
}

eq_profile eq_profile::preset(std::string_view preset_name) noexcept {
    auto profile = flat();
    if (preset_name == "bass") {
        constexpr std::array<float, 20> gains{7.0F, 7.0F, 6.0F, 5.0F, 4.0F, 2.0F, 0.5F, -1.0F, -1.5F, -1.5F,
            -1.0F, -0.5F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F};
        apply_graphic_gains(profile, gains, -6.0F);
    } else if (preset_name == "deep_bass") {
        constexpr std::array<float, 20> gains{10.0F, 10.0F, 9.0F, 7.0F, 5.0F, 3.0F, 0.5F, -1.0F, -1.5F, -1.5F,
            -1.0F, -0.5F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F};
        apply_graphic_gains(profile, gains, -9.0F);
    } else if (preset_name == "bass_cut") {
        constexpr std::array<float, 20> gains{-10.0F, -10.0F, -9.0F, -8.0F, -6.0F, -4.0F, -3.0F, -1.5F, -0.5F, 0.0F,
            0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F};
        apply_graphic_gains(profile, gains, 0.0F);
    } else if (preset_name == "loudness") {
        constexpr std::array<float, 20> gains{6.0F, 6.0F, 5.0F, 4.0F, 3.0F, 1.0F, 0.0F, -1.0F, -1.0F, -0.5F,
            0.0F, 0.5F, 1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 5.0F, 4.0F, 3.0F};
        apply_graphic_gains(profile, gains, -6.0F);
    } else if (preset_name == "vocal") {
        constexpr std::array<float, 20> gains{-3.0F, -3.0F, -3.0F, -2.5F, -2.0F, -1.5F, -1.0F, 0.0F, 1.0F, 2.0F,
            3.0F, 3.5F, 3.0F, 2.0F, 1.5F, 1.0F, 0.5F, 0.0F, 0.0F, 0.0F};
        apply_graphic_gains(profile, gains, -3.5F);
    } else if (preset_name == "clarity") {
        constexpr std::array<float, 20> gains{-2.0F, -2.0F, -2.0F, -2.0F, -1.0F, -1.0F, -0.5F, 0.0F, 0.5F, 1.5F,
            2.5F, 3.0F, 2.5F, 1.5F, 1.0F, 0.5F, 0.0F, 0.0F, 0.0F, 0.0F};
        apply_graphic_gains(profile, gains, -3.0F);
    } else if (preset_name == "warm") {
        constexpr std::array<float, 20> gains{2.5F, 2.5F, 2.0F, 2.0F, 1.5F, 1.0F, 0.0F, -0.5F, -1.0F, -1.0F,
            -1.0F, -1.5F, -2.0F, -2.5F, -3.0F, -3.0F, -3.0F, -2.5F, -2.0F, -1.5F};
        apply_graphic_gains(profile, gains, -2.5F);
    } else if (preset_name == "bright") {
        constexpr std::array<float, 20> gains{0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.5F, 1.0F,
            1.5F, 2.0F, 2.5F, 3.0F, 3.5F, 4.0F, 4.0F, 3.5F, 3.0F, 2.5F};
        apply_graphic_gains(profile, gains, -4.0F);
    } else if (preset_name == "de_ess") {
        constexpr std::array<float, 20> gains{0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F,
            0.0F, -0.5F, -1.5F, -3.0F, -5.0F, -6.0F, -5.0F, -3.0F, -2.0F, -1.0F};
        apply_graphic_gains(profile, gains, 0.0F);
    } else if (preset_name == "treble_cut") {
        constexpr std::array<float, 20> gains{0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F,
            -0.5F, -1.0F, -2.0F, -3.0F, -4.0F, -5.0F, -6.0F, -6.0F, -5.0F, -4.0F};
        apply_graphic_gains(profile, gains, 0.0F);
    } else if (preset_name == "late_night") {
        profile.effects.loudness_enabled = true;
        profile.effects.loudness_amount = 0.62F;
        profile.effects.bass_enabled = true;
        profile.effects.bass_db = 2.5F;
        profile.preamp_db = -3.0F;
    } else if (preset_name == "wide_music") {
        profile.effects.stereo_enabled = true;
        profile.effects.stereo_width = 1.22F;
        profile.effects.clarity_enabled = true;
        profile.effects.clarity_db = 1.5F;
        profile.preamp_db = -2.0F;
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
    const auto count = active_band_count(profile);
    for (std::size_t index = 0; index < count; ++index) {
        const auto& band = active_band(profile, index);
        if (!band.enabled) continue;
        biquad_filter filter;
        filter.configure(band, sample_rate);
        response += filter.response_db(sample_rate, frequency_hz);
    }
    for (const auto& effect_band : {bass_shelf(profile.effects), loudness_low_shelf(profile.effects),
                                    loudness_high_shelf(profile.effects), clarity_shelf(profile.effects)}) {
        if (!effect_band.enabled) continue;
        biquad_filter filter;
        filter.configure(effect_band, sample_rate);
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
    active_band_count_ = active_band_count(profile);
    const eq_band bypass{filter_shape::peaking, 1000.0F, 0.0F, 1.0F, false};
    for (std::size_t channel = 0; channel < channels_; ++channel) {
        for (std::size_t band = 0; band < active_band_count_; ++band) {
            filters_[channel][band].configure(active_band(profile, band), sample_rate);
        }
        for (std::size_t band = active_band_count_; band < arbitrary_band_count; ++band) {
            filters_[channel][band].configure(bypass, sample_rate);
        }
    }
    reset();
    limiter_clamp_count_ = 0;
}

void eq_processor::reset() noexcept {
    for (auto& channel_filters : filters_) {
        for (auto& filter : channel_filters) {
            filter.reset();
        }
    }
}

void eq_processor::process_interleaved(float* samples, std::size_t frame_count) noexcept {
    process_interleaved(samples, samples, frame_count);
}

void eq_processor::process_interleaved(const float* input, float* output, std::size_t frame_count) noexcept {
    if (input == nullptr || output == nullptr) return;
    if (!profile_.enabled) {
        if (input != output) {
            std::copy_n(input, frame_count * channels_, output);
        }
        return;
    }
    for (std::size_t frame = 0; frame < frame_count; ++frame) {
        for (std::size_t channel = 0; channel < channels_; ++channel) {
            auto sample = input[frame * channels_ + channel];
            for (std::size_t band = 0; band < active_band_count_; ++band) {
                sample = filters_[channel][band].process(sample);
            }
            sample *= preamp_linear_;
            const auto limited = std::clamp(sample, -limiter_ceiling_linear_, limiter_ceiling_linear_);
            if (limited != sample) ++limiter_clamp_count_;
            output[frame * channels_ + channel] = limited;
        }
    }
}

std::uint64_t eq_processor::take_limiter_clamp_count() noexcept {
    const auto count = limiter_clamp_count_;
    limiter_clamp_count_ = 0;
    return count;
}

void profile_processor::configure(const eq_profile& profile, float sample_rate, std::size_t channels) noexcept {
    profile_ = profile;
    channels_ = std::clamp<std::size_t>(channels, 1, max_channels);
    const auto balance = std::clamp(profile.effects.balance, -1.0F, 1.0F);
    const auto pan = (balance + 1.0F) * static_cast<float>(pi * 0.25);
    // sqrt(2) leaves centre at unity while preserving equal-power panning.
    left_balance_ = std::cos(pan) * 1.41421356237F;
    right_balance_ = std::sin(pan) * 1.41421356237F;
    const auto bass = bass_shelf(profile.effects);
    const auto loud_low = loudness_low_shelf(profile.effects);
    const auto loud_high = loudness_high_shelf(profile.effects);
    const auto clarity = clarity_shelf(profile.effects);
    for (std::size_t channel = 0; channel < channels_; ++channel) {
        bass_filters_[channel].configure(bass, sample_rate);
        loudness_low_filters_[channel].configure(loud_low, sample_rate);
        loudness_high_filters_[channel].configure(loud_high, sample_rate);
        clarity_filters_[channel].configure(clarity, sample_rate);
    }
    graphic_processor_.configure(profile, sample_rate, channels_);
    reset();
}

void profile_processor::reset() noexcept {
    for (std::size_t channel = 0; channel < max_channels; ++channel) {
        bass_filters_[channel].reset();
        loudness_low_filters_[channel].reset();
        loudness_high_filters_[channel].reset();
        clarity_filters_[channel].reset();
    }
    graphic_processor_.reset();
}

void profile_processor::process_interleaved(float* samples, std::size_t frame_count) noexcept {
    process_interleaved(samples, samples, frame_count);
}

void profile_processor::process_interleaved(const float* input, float* output, std::size_t frame_count) noexcept {
    if (input == nullptr || output == nullptr) return;
    if (!profile_.enabled) {
        if (input != output) std::copy_n(input, frame_count * channels_, output);
        return;
    }
    for (std::size_t frame = 0; frame < frame_count; ++frame) {
        const auto offset = frame * channels_;
        if (channels_ >= 2) {
            auto left = input[offset];
            auto right = input[offset + 1];
            if (profile_.effects.mono) {
                const auto mono = (left + right) * 0.5F;
                left = mono;
                right = mono;
            } else if (profile_.effects.stereo_enabled) {
                const auto mid = (left + right) * 0.5F;
                const auto side = (left - right) * 0.5F * std::clamp(profile_.effects.stereo_width, 0.5F, 1.5F);
                left = mid + side;
                right = mid - side;
            }
            output[offset] = left * left_balance_;
            output[offset + 1] = right * right_balance_;
            for (std::size_t channel = 2; channel < channels_; ++channel) output[offset + channel] = input[offset + channel];
        } else {
            // Width and mono never manufacture stereo from a mono endpoint.
            output[offset] = input[offset];
        }
        for (std::size_t channel = 0; channel < channels_; ++channel) {
            auto sample = output[offset + channel];
            sample = bass_filters_[channel].process(sample);
            sample = loudness_low_filters_[channel].process(sample);
            sample = loudness_high_filters_[channel].process(sample);
            output[offset + channel] = clarity_filters_[channel].process(sample);
        }
    }
    graphic_processor_.process_interleaved(output, output, frame_count);
}

std::uint64_t profile_processor::take_limiter_clamp_count() noexcept {
    return graphic_processor_.take_limiter_clamp_count();
}

}  // namespace termite
