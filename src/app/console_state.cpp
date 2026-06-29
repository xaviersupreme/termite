#include "app/console_state.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace termite {
namespace {

constexpr std::array<std::string_view, 12> preset_ids{
    "bass", "deep_bass", "bass_cut", "loudness", "vocal", "clarity", "warm", "bright", "de_ess", "treble_cut", "late_night", "wide_music",
};
constexpr std::array<std::wstring_view, 12> preset_labels{
    L"Bass boost", L"Deep bass", L"Bass cut", L"Loudness", L"Vocal", L"Clarity", L"Warm", L"Treble boost", L"De-ess", L"Treble cut", L"Late night", L"Wide music",
};

std::wstring widen(const std::string& value) {
    return {value.begin(), value.end()};
}

}  // namespace

console_state::console_state() {
    append_notice(L"Start up mode: Saved settings");
    append_notice(L"Termite is ready. Route selected apps to CABLE Input.");
    append_notice(L"Right-click a graphic band for its filter controls.");
}

const eq_profile& console_state::profile() const noexcept { return profile_; }
const std::vector<std::wstring>& console_state::notices() const noexcept { return notices_; }
float console_state::scroll_offset() const noexcept { return scroll_offset_; }
console_tab console_state::active_tab() const noexcept { return active_tab_; }
bool console_state::grid_visible() const noexcept { return grid_visible_; }
std::size_t console_state::background_index() const noexcept { return background_index_; }

bool console_state::is_selected(console_control control) const noexcept {
    switch (control) {
        case console_control::equalizer_on: return profile_.enabled;
        case console_control::equalizer_off: return !profile_.enabled;
        case console_control::pause: return paused_;
        case console_control::sleep: return sleeping_;
        case console_control::default_start: return default_start_saved_;
        case console_control::grid: return grid_visible_;
        default: return false;
    }
}

std::wstring console_state::preset_label() const {
    return preset_index_ < 0 ? std::wstring{preset_labels.front()} : std::wstring{preset_labels[static_cast<std::size_t>(preset_index_)]};
}

std::size_t console_state::preset_count() noexcept { return preset_ids.size(); }

std::wstring_view console_state::preset_name(std::size_t index) noexcept {
    return index < preset_labels.size() ? preset_labels[index] : std::wstring_view{};
}

int console_state::selected_preset_index() const noexcept { return preset_index_; }

console_persistent_state console_state::persistent_state() const {
    return {profile_, preset_index_, paused_, sleeping_, default_start_saved_, grid_visible_, background_index_, active_tab_};
}

console_action_result console_state::activate(console_control control) {
    console_action_result result;
    switch (control) {
        case console_control::tab_graphic_eq: active_tab_ = console_tab::graphic_eq; break;
        case console_control::tab_effects_rack: active_tab_ = console_tab::effects_rack; break;
        case console_control::tab_apps: active_tab_ = console_tab::apps; break;
        case console_control::tab_monitor: active_tab_ = console_tab::monitor; break;
        case console_control::minimize:
            result.minimize = true;
            break;
        case console_control::close:
            result.close = true;
            break;
        case console_control::hardware_menu:
            append_notice(L"Hardware diagnostics opened.");
            result.open_diagnostics = true;
            break;
        case console_control::help_menu:
            append_notice(L"Use Route apps to send an open app to CABLE Input while Termite is running.");
            break;
        case console_control::route_apps:
            active_tab_ = console_tab::apps;
            append_notice(L"Choose open apps, then Termite will route their active audio to CABLE Input.");
            result.open_routing = true;
            break;
        case console_control::file_menu:
            append_notice(L"The current profile is saved automatically in Termite settings.");
            break;
        case console_control::themes_menu:
            append_notice(L"Native vsound-style background is active.");
            break;
        case console_control::detect:
            append_notice(L"Detecting the VB-CABLE audio path.");
            result.restart_audio = true;
            break;
        case console_control::reset:
        case console_control::preset_zero:
            reset_profile();
            append_notice(L"All graphic equalizer bands reset to zero.");
            result.profile_changed = true;
            break;
        case console_control::status_sync:
            append_notice(L"Audio diagnostics opened.");
            result.request_engine_status = true;
            result.open_diagnostics = true;
            break;
        case console_control::pause:
            paused_ = true;
            sleeping_ = false;
            profile_.enabled = false;
            append_notice(L"Audio processing paused.");
            result.profile_changed = true;
            break;
        case console_control::run:
            paused_ = false;
            sleeping_ = false;
            profile_.enabled = true;
            append_notice(L"Audio processing running.");
            result.profile_changed = true;
            break;
        case console_control::clear_info:
            notices_.clear();
            append_notice(L"Information panel cleared.");
            break;
        case console_control::sleep:
            sleeping_ = true;
            paused_ = false;
            profile_.enabled = false;
            append_notice(L"Sleep mode enabled. Use Run to resume.");
            result.profile_changed = true;
            break;
        case console_control::default_start:
            default_start_saved_ = !default_start_saved_;
            append_notice(default_start_saved_ ? L"Default start marked as saved." : L"Default start cleared.");
            break;
        case console_control::equalizer_off:
            profile_.enabled = false;
            append_notice(L"Equalizer bypassed.");
            result.profile_changed = true;
            break;
        case console_control::equalizer_on:
            profile_.enabled = true;
            paused_ = false;
            sleeping_ = false;
            append_notice(L"Equalizer enabled.");
            result.profile_changed = true;
            break;
        case console_control::volume_up:
            profile_.preamp_db = std::min(12.0F, profile_.preamp_db + 1.0F);
            append_notice(L"Digital volume increased.");
            result.profile_changed = true;
            break;
        case console_control::volume_down:
            profile_.preamp_db = std::max(-20.0F, profile_.preamp_db - 1.0F);
            append_notice(L"Digital volume decreased.");
            result.profile_changed = true;
            break;
        case console_control::profile_open:
        case console_control::profile_save:
            break;
        case console_control::preset_cycle:
            apply_next_preset();
            result.profile_changed = true;
            break;
        case console_control::export_response:
            append_notice(L"Response export is not part of v1.");
            break;
        case console_control::grid:
            grid_visible_ = !grid_visible_;
            append_notice(grid_visible_ ? L"Graph grid enabled." : L"Graph grid disabled.");
            break;
        case console_control::help_button:
            append_notice(L"Use Route apps to send an open app to CABLE Input while Termite is running.");
            break;
        case console_control::effect_bass_toggle:
            profile_.effects.bass_enabled = !profile_.effects.bass_enabled;
            result.profile_changed = true;
            break;
        case console_control::effect_bass_down:
            profile_.effects.bass_db = std::max(-12.0F, profile_.effects.bass_db - 1.0F);
            result.profile_changed = true;
            break;
        case console_control::effect_bass_up:
            profile_.effects.bass_db = std::min(12.0F, profile_.effects.bass_db + 1.0F);
            result.profile_changed = true;
            break;
        case console_control::effect_loudness_toggle:
            profile_.effects.loudness_enabled = !profile_.effects.loudness_enabled;
            result.profile_changed = true;
            break;
        case console_control::effect_loudness_down:
            profile_.effects.loudness_amount = std::max(0.0F, profile_.effects.loudness_amount - 0.05F);
            result.profile_changed = true;
            break;
        case console_control::effect_loudness_up:
            profile_.effects.loudness_amount = std::min(1.0F, profile_.effects.loudness_amount + 0.05F);
            result.profile_changed = true;
            break;
        case console_control::effect_clarity_toggle:
            profile_.effects.clarity_enabled = !profile_.effects.clarity_enabled;
            result.profile_changed = true;
            break;
        case console_control::effect_clarity_down:
            profile_.effects.clarity_db = std::max(-8.0F, profile_.effects.clarity_db - 1.0F);
            result.profile_changed = true;
            break;
        case console_control::effect_clarity_up:
            profile_.effects.clarity_db = std::min(8.0F, profile_.effects.clarity_db + 1.0F);
            result.profile_changed = true;
            break;
        case console_control::effect_stereo_toggle:
            profile_.effects.stereo_enabled = !profile_.effects.stereo_enabled;
            result.profile_changed = true;
            break;
        case console_control::effect_width_down:
            profile_.effects.stereo_width = std::max(0.5F, profile_.effects.stereo_width - 0.05F);
            result.profile_changed = true;
            break;
        case console_control::effect_width_up:
            profile_.effects.stereo_width = std::min(1.5F, profile_.effects.stereo_width + 0.05F);
            result.profile_changed = true;
            break;
        case console_control::effect_mono:
            profile_.effects.mono = !profile_.effects.mono;
            result.profile_changed = true;
            break;
        case console_control::effect_balance_left:
            profile_.effects.balance = std::max(-1.0F, profile_.effects.balance - 0.1F);
            result.profile_changed = true;
            break;
        case console_control::effect_balance_right:
            profile_.effects.balance = std::min(1.0F, profile_.effects.balance + 0.1F);
            result.profile_changed = true;
            break;
        case console_control::effect_reset:
            profile_.effects = {};
            result.profile_changed = true;
            break;
        default:
            break;
    }
    return result;
}

bool console_state::set_fader_gain(std::size_t index, float gain_db) {
    if (index >= profile_.bands.size()) return false;
    const auto clamped = std::clamp(gain_db, -20.0F, 20.0F);
    if (profile_.bands[index].gain_db == clamped) return false;
    profile_.bands[index].gain_db = clamped;
    return true;
}

bool console_state::adjust_fader_gain(std::size_t index, float delta_db) {
    if (index >= profile_.bands.size()) return false;
    return set_fader_gain(index, profile_.bands[index].gain_db + delta_db);
}

bool console_state::adjust_fader_q(std::size_t index, float delta_q) {
    if (index >= profile_.bands.size()) return false;
    auto& q = profile_.bands[index].q;
    const auto updated = std::clamp(q + delta_q, 0.15F, 12.0F);
    if (q == updated) return false;
    q = updated;
    return true;
}

bool console_state::set_fader_shape(std::size_t index, filter_shape shape) {
    if (index >= profile_.bands.size()) return false;
    auto& band = profile_.bands[index];
    if (band.shape == shape) return false;
    band.shape = shape;
    append_notice(L"Graphic band " + std::to_wstring(index + 1) + L" filter changed to " + widen(std::string{filter_shape_name(shape)}) + L".");
    return true;
}

bool console_state::set_fader_enabled(std::size_t index, bool enabled) {
    if (index >= profile_.bands.size() || profile_.bands[index].enabled == enabled) return false;
    profile_.bands[index].enabled = enabled;
    append_notice(L"Graphic band " + std::to_wstring(index + 1) + (enabled ? L" enabled." : L" bypassed."));
    return true;
}

bool console_state::apply_preset(std::size_t index) {
    if (index >= preset_ids.size()) return false;
    preset_index_ = static_cast<int>(index);
    profile_ = eq_profile::preset(preset_ids[index]);
    append_notice(preset_label() + L" preset applied.");
    return true;
}

void console_state::set_profile(eq_profile profile) {
    profile_ = std::move(profile);
    preset_index_ = -1;
    paused_ = false;
    sleeping_ = false;
}

void console_state::set_active_tab(console_tab tab) noexcept { active_tab_ = tab; }

void console_state::restore_persistent_state(const console_persistent_state& settings) {
    profile_ = settings.profile;
    for (std::size_t index = 0; index < graphic_band_count; ++index) {
        auto& band = profile_.bands[index];
        band.frequency_hz = graphic_band_frequencies[index];
        band.gain_db = std::isfinite(band.gain_db) ? std::clamp(band.gain_db, -20.0F, 20.0F) : 0.0F;
        band.q = std::isfinite(band.q) ? std::clamp(band.q, 0.15F, 12.0F) : 1.0F;
        if (static_cast<std::size_t>(band.shape) > static_cast<std::size_t>(filter_shape::notch)) band.shape = filter_shape::peaking;
    }
    profile_.preamp_db = std::isfinite(profile_.preamp_db) ? std::clamp(profile_.preamp_db, -24.0F, 12.0F) : 0.0F;
    profile_.limiter_ceiling_db = std::isfinite(profile_.limiter_ceiling_db) ? std::clamp(profile_.limiter_ceiling_db, -12.0F, 0.0F) : -1.0F;
    profile_.effects.bass_db = std::isfinite(profile_.effects.bass_db) ? std::clamp(profile_.effects.bass_db, -12.0F, 12.0F) : 0.0F;
    profile_.effects.loudness_amount = std::isfinite(profile_.effects.loudness_amount) ? std::clamp(profile_.effects.loudness_amount, 0.0F, 1.0F) : 0.0F;
    profile_.effects.clarity_db = std::isfinite(profile_.effects.clarity_db) ? std::clamp(profile_.effects.clarity_db, -8.0F, 8.0F) : 0.0F;
    profile_.effects.stereo_width = std::isfinite(profile_.effects.stereo_width) ? std::clamp(profile_.effects.stereo_width, 0.5F, 1.5F) : 1.0F;
    profile_.effects.balance = std::isfinite(profile_.effects.balance) ? std::clamp(profile_.effects.balance, -1.0F, 1.0F) : 0.0F;
    preset_index_ = std::clamp(settings.preset_index, -1, static_cast<int>(preset_ids.size()) - 1);
    paused_ = settings.paused;
    sleeping_ = settings.sleeping;
    if (paused_ || sleeping_) profile_.enabled = false;
    default_start_saved_ = settings.default_start_saved;
    grid_visible_ = settings.grid_visible;
    background_index_ = settings.background_index % 6U;
    active_tab_ = static_cast<std::size_t>(settings.active_tab) < console_tab_count ? settings.active_tab : console_tab::graphic_eq;
    append_notice(L"Saved settings restored.");
}

void console_state::set_scroll_offset(float offset) noexcept {
    const auto content_height = std::max(1.0F, static_cast<float>(notices_.size()) * 15.0F);
    const auto max_scroll = std::max(0.0F, content_height - (console_layout::status_viewport().height - 8.0F));
    scroll_offset_ = std::clamp(offset, 0.0F, max_scroll);
}

void console_state::append_engine_status(const std::string& status) {
    append_notice(L"Engine: " + widen(status));
}

void console_state::append_notice(std::wstring message) {
    if (notices_.size() == 64) notices_.erase(notices_.begin());
    notices_.push_back(std::move(message));
    const auto content_height = static_cast<float>(notices_.size()) * 15.0F;
    set_scroll_offset(std::max(0.0F, content_height - (console_layout::status_viewport().height - 8.0F)));
}

void console_state::reset_profile() {
    profile_ = eq_profile::flat();
    preset_index_ = -1;
}

void console_state::apply_next_preset() {
    const auto next = static_cast<std::size_t>((preset_index_ + 1) % static_cast<int>(preset_ids.size()));
    apply_preset(next);
}

}  // namespace termite
