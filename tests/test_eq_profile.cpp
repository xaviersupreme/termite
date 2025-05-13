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
}
