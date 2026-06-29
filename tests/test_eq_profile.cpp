#include "dsp/eq_profile.h"

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <array>
#include <cassert>
#include <cmath>
#include <vector>

namespace {

bool approximately_equal(float left, float right, float tolerance = 0.15F) {
    return std::abs(left - right) <= tolerance;
}

}  // namespace

int main() {
    const auto flat = termite::eq_profile::flat();
    assert(flat.enabled);
    assert(flat.bands.size() == termite::graphic_band_count);
    for (const auto& band : flat.bands) {
        assert(approximately_equal(termite::profile_response_db(flat, 48000.0F, band.frequency_hz), 0.0F, 0.02F));
    }
    constexpr std::array<float, 9> response_checkpoints{20.0F, 30.0F, 45.0F, 80.0F, 120.0F, 300.0F, 750.0F, 2000.0F, 12000.0F};
    for (const auto frequency_hz : response_checkpoints) {
        assert(approximately_equal(termite::profile_response_db(flat, 48000.0F, frequency_hz), 0.0F, 0.02F));
    }

    auto peak_profile = termite::eq_profile::flat();
    peak_profile.bands[3] = {termite::filter_shape::peaking, 1000.0F, 9.0F, 1.0F};
    assert(termite::profile_response_db(peak_profile, 48000.0F, 1000.0F) > 8.7F);
    assert(termite::profile_response_db(peak_profile, 48000.0F, 100.0F) < 0.5F);

    auto low_pass_profile = termite::eq_profile::flat();
    low_pass_profile.bands[8] = {termite::filter_shape::low_pass, 1000.0F, 0.0F, 0.707F};
    assert(termite::profile_response_db(low_pass_profile, 48000.0F, 8000.0F) < -30.0F);

    for (std::size_t index = 0; index < termite::graphic_band_count; ++index) {
        auto maximum_slider = termite::eq_profile::flat();
        maximum_slider.bands[index].gain_db = 20.0F;
        const auto response = termite::profile_response_db(maximum_slider, 48000.0F, maximum_slider.bands[index].frequency_hz);
        assert(response > 19.7F);
    }

    const auto bass = termite::eq_profile::preset("bass");
    assert(bass.bands[0].gain_db > 0.0F);
    assert(bass.preamp_db < 0.0F);

    auto effects_profile = termite::eq_profile::flat();
    effects_profile.effects.bass_enabled = true;
    effects_profile.effects.bass_db = 8.0F;
    assert(termite::profile_response_db(effects_profile, 48000.0F, 40.0F) > 6.0F);
    effects_profile.effects.clarity_enabled = true;
    effects_profile.effects.clarity_db = 6.0F;
    assert(termite::profile_response_db(effects_profile, 48000.0F, 12000.0F) > 5.0F);
    effects_profile.effects.loudness_enabled = true;
    effects_profile.effects.loudness_amount = 1.0F;
    assert(termite::profile_response_db(effects_profile, 48000.0F, 80.0F) > 10.0F);

    termite::profile_processor effects_processor;
    auto spatial_profile = termite::eq_profile::flat();
    spatial_profile.effects.stereo_enabled = true;
    spatial_profile.effects.stereo_width = 1.5F;
    effects_processor.configure(spatial_profile, 48000.0F, 2);
    std::array<float, 2> spatial{0.4F, -0.4F};
    effects_processor.process_interleaved(spatial.data(), 1);
    assert(std::abs(spatial[0]) > 0.5F && std::abs(spatial[1]) > 0.5F);
    spatial_profile.effects.mono = true;
    effects_processor.configure(spatial_profile, 48000.0F, 2);
    spatial = {0.4F, -0.2F};
    effects_processor.process_interleaved(spatial.data(), 1);
    assert(approximately_equal(spatial[0], spatial[1], 0.001F));
    spatial_profile.effects.balance = -1.0F;
    spatial_profile.effects.mono = false;
    effects_processor.configure(spatial_profile, 48000.0F, 2);
    spatial = {0.3F, 0.3F};
    effects_processor.process_interleaved(spatial.data(), 1);
    assert(std::abs(spatial[0]) > std::abs(spatial[1]) * 10.0F);

    termite::profile_processor old_processor;
    termite::profile_processor new_processor;
    auto old_profile = termite::eq_profile::flat();
    auto new_profile = termite::eq_profile::flat();
    new_profile.effects.bass_enabled = true;
    new_profile.effects.bass_db = 9.0F;
    new_profile.effects.stereo_enabled = true;
    new_profile.effects.stereo_width = 1.3F;
    old_processor.configure(old_profile, 48000.0F, 2);
    new_processor.configure(new_profile, 48000.0F, 2);
    std::array<float, 64> transition_input{};
    std::array<float, 64> transition_old{};
    std::array<float, 64> transition_new{};
    std::array<float, 64> transition_output{};
    for (std::size_t index = 0; index < transition_input.size(); ++index) transition_input[index] = std::sin(static_cast<float>(index) * 0.21F) * 0.3F;
    old_processor.process_interleaved(transition_input.data(), transition_old.data(), transition_input.size() / 2);
    new_processor.process_interleaved(transition_input.data(), transition_new.data(), transition_input.size() / 2);
    for (std::size_t frame = 0; frame < transition_input.size() / 2; ++frame) {
        const auto amount = static_cast<float>(frame + 1) / static_cast<float>(transition_input.size() / 2);
        for (std::size_t channel = 0; channel < 2; ++channel) {
            const auto sample = transition_old[frame * 2 + channel] * std::cos(amount * 0.785398163F) + transition_new[frame * 2 + channel] * std::sin(amount * 0.785398163F);
            transition_output[frame * 2 + channel] = sample;
            assert(std::isfinite(sample) && std::abs(sample) <= 1.0F);
        }
    }

    auto limiter_profile = termite::eq_profile::flat();
    limiter_profile.preamp_db = 12.0F;
    limiter_profile.limiter_ceiling_db = -3.0F;
    termite::eq_processor processor;
    processor.configure(limiter_profile, 48000.0F, 2);
    std::vector<float> samples(128, 1.0F);
    processor.process_interleaved(samples.data(), samples.size() / 2);
    for (const auto sample : samples) {
        assert(std::abs(sample) <= 0.708F);
    }

    limiter_profile.enabled = false;
    processor.configure(limiter_profile, 48000.0F, 2);
    samples.assign(8, 0.42F);
    processor.process_interleaved(samples.data(), 4);
    for (const auto sample : samples) {
        assert(sample == 0.42F);
    }

    limiter_profile.enabled = true;
    processor.configure(limiter_profile, 48000.0F, 2);
    std::vector<float> input(128, 0.25F);
    std::vector<float> output(input.size());
    processor.process_interleaved(input.data(), output.data(), input.size() / 2);
    for (std::size_t index = 0; index < input.size(); ++index) {
        assert(std::isfinite(output[index]));
        assert(std::abs(output[index]) <= 0.708F);
        assert(input[index] == 0.25F);
    }

    constexpr std::array shapes{
        termite::filter_shape::peaking, termite::filter_shape::low_shelf, termite::filter_shape::high_shelf,
        termite::filter_shape::low_pass, termite::filter_shape::high_pass, termite::filter_shape::notch,
    };
    for (const auto shape : shapes) {
        auto shaped = termite::eq_profile::flat();
        shaped.bands[7].shape = shape;
        shaped.bands[7].gain_db = 12.0F;
        shaped.bands[7].q = 4.0F;
        processor.configure(shaped, 48000.0F, 2);
        for (std::size_t index = 0; index < input.size(); ++index) {
            input[index] = std::sin(static_cast<float>(index) * 0.13F) * 0.4F;
        }
        processor.process_interleaved(input.data(), output.data(), input.size() / 2);
        for (const auto sample : output) {
            assert(std::isfinite(sample));
            assert(std::abs(sample) <= 1.0F);
        }
    }
}
