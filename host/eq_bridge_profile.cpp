#include "host/eq_bridge_profile.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace termite {
namespace {

constexpr float bridge_min_frequency = arbitrary_min_frequency_hz;
constexpr float bridge_max_frequency = arbitrary_max_frequency_hz;

float finite_clamp(float value, float low, float high) noexcept {
    return std::isfinite(value) ? std::clamp(value, low, high) : 0.0F;
}

float interpolation_coordinate(float frequency_hz, eq_bridge_x_axis axis) noexcept {
    const auto frequency = finite_clamp(frequency_hz, bridge_min_frequency, bridge_max_frequency);
    return axis == eq_bridge_x_axis::logarithmic ? std::log(frequency) : frequency;
}

}  // namespace

bool valid_eq_bridge_snapshot(const eq_bridge_snapshot_v1& snapshot) noexcept {
    if (snapshot.magic != eq_bridge_magic || snapshot.version != eq_bridge_version || snapshot.bytes != sizeof(snapshot)) return false;
    if (snapshot.mode > static_cast<std::uint8_t>(eq_bridge_mode::arbitrary) ||
        snapshot.interpolation > static_cast<std::uint8_t>(eq_bridge_interpolation::step) ||
        snapshot.x_axis > static_cast<std::uint8_t>(eq_bridge_x_axis::linear) ||
        snapshot.marker_count > eq_bridge_max_markers) return false;
    if (!std::isfinite(snapshot.preamp_db) || !std::isfinite(snapshot.smoothing) || !std::isfinite(snapshot.tension)) return false;
    for (const auto gain : snapshot.graphic_gains) {
        if (!std::isfinite(gain)) return false;
    }
    for (std::size_t index = 0; index < snapshot.marker_count; ++index) {
        if (!std::isfinite(snapshot.markers[index].frequency_hz) || !std::isfinite(snapshot.markers[index].gain_db)) return false;
    }
    return true;
}

float bridge_arbitrary_response_db(const eq_bridge_snapshot_v1& snapshot, float frequency_hz) noexcept {
    const auto count = std::min<std::size_t>(snapshot.marker_count, eq_bridge_max_markers);
    if (count == 0) return 0.0F;

    std::array<eq_bridge_marker_v1, eq_bridge_max_markers> markers{};
    for (std::size_t index = 0; index < count; ++index) {
        markers[index].frequency_hz = finite_clamp(snapshot.markers[index].frequency_hz, bridge_min_frequency, bridge_max_frequency);
        markers[index].gain_db = finite_clamp(snapshot.markers[index].gain_db, -40.0F, 40.0F);
    }
    std::sort(markers.begin(), markers.begin() + static_cast<std::ptrdiff_t>(count), [](const auto& left, const auto& right) {
        return left.frequency_hz < right.frequency_hz;
    });

    const auto axis = static_cast<eq_bridge_x_axis>(snapshot.x_axis);
    const auto position = interpolation_coordinate(frequency_hz, axis);
    const auto first = interpolation_coordinate(markers[0].frequency_hz, axis);
    const auto last = interpolation_coordinate(markers[count - 1].frequency_hz, axis);
    if (position <= first) return markers[0].gain_db;
    if (position >= last) return markers[count - 1].gain_db;

    for (std::size_t right = 1; right < count; ++right) {
        const auto right_position = interpolation_coordinate(markers[right].frequency_hz, axis);
        if (position > right_position) continue;
        const auto left_position = interpolation_coordinate(markers[right - 1].frequency_hz, axis);
        auto amount = (position - left_position) / std::max(0.0001F, right_position - left_position);
        const auto interpolation = static_cast<eq_bridge_interpolation>(snapshot.interpolation);
        if (interpolation == eq_bridge_interpolation::step) {
            amount = 0.0F;
        } else if (interpolation == eq_bridge_interpolation::spline) {
            // Cardinal cubic spline.  The UI and this bridge deliberately use
            // the same control-point model, so the response heard is the one
            // drawn while markers are dragged.
            const auto left_gain = markers[right - 1].gain_db;
            const auto right_gain = markers[right].gain_db;
            const auto previous_gain = markers[right >= 2 ? right - 2 : right - 1].gain_db;
            const auto next_gain = markers[right + 1 < count ? right + 1 : right].gain_db;
            const auto tightness = finite_clamp(snapshot.tension, 0.0F, 1.0F);
            const auto left_tangent = (right_gain - previous_gain) * (1.0F - tightness) * 0.5F;
            const auto right_tangent = (next_gain - left_gain) * (1.0F - tightness) * 0.5F;
            const auto square = amount * amount;
            const auto cube = square * amount;
            return (2.0F * cube - 3.0F * square + 1.0F) * left_gain +
                (cube - 2.0F * square + amount) * left_tangent +
                (-2.0F * cube + 3.0F * square) * right_gain +
                (cube - square) * right_tangent;
        }
        return markers[right - 1].gain_db + (markers[right].gain_db - markers[right - 1].gain_db) * amount;
    }
    return markers[count - 1].gain_db;
}

eq_profile profile_from_bridge_snapshot(const eq_bridge_snapshot_v1& snapshot, const eq_profile& current) noexcept {
    auto profile = current;
    profile.enabled = snapshot.equalizer_enabled != 0;
    profile.preamp_db = finite_clamp(snapshot.preamp_db, -24.0F, 12.0F);

    if (static_cast<eq_bridge_mode>(snapshot.mode) == eq_bridge_mode::graphic) {
        profile.processing_mode = eq_processing_mode::graphic;
        // Smoothing is a Q control, not an average of neighbouring faders.
        // Averaging destroys a single boosted/cut band at high smoothing and
        // makes the audible response disagree with the controls.  Retain each
        // fader gain and broaden its filter as smoothing rises.
        const auto smoothing = finite_clamp(snapshot.smoothing, 0.0F, 100.0F) / 100.0F;
        const auto q = 8.0F - 7.0F * smoothing;
        for (std::size_t index = 0; index < graphic_band_count; ++index) {
            profile.bands[index] = {filter_shape::peaking, graphic_band_frequencies[index],
                                    finite_clamp(snapshot.graphic_gains[index], -20.0F, 20.0F), q, true};
        }
    } else {
        // Fit the marker response onto a dense, logarithmic bank.  This is
        // performed on the bridge/control thread, never the audio callback.
        // A few residual passes compensate for neighbouring peaking filters
        // so a flat curve stays flat and a drawn point lands at its gain.
        profile = eq_profile::flat();
        profile.processing_mode = eq_processing_mode::arbitrary;
        profile.enabled = snapshot.equalizer_enabled != 0;
        profile.preamp_db = finite_clamp(snapshot.preamp_db, -24.0F, 12.0F);
        for (std::size_t index = 0; index < arbitrary_band_count; ++index) {
            profile.arbitrary_bands[index] = {filter_shape::peaking, arbitrary_band_frequency(index), 0.0F, 10.0F, true};
        }
        for (int pass = 0; pass < 5; ++pass) {
            for (std::size_t index = 0; index < arbitrary_band_count; ++index) {
                const auto frequency = profile.arbitrary_bands[index].frequency_hz;
                const auto target = std::clamp(bridge_arbitrary_response_db(snapshot, frequency), -20.0F, 20.0F);
                const auto residual = target - profile_response_db(profile, 48000.0F, frequency);
                profile.arbitrary_bands[index].gain_db = std::clamp(
                    profile.arbitrary_bands[index].gain_db + residual, -20.0F, 20.0F);
            }
        }
    }
    return profile;
}

}  // namespace termite
