#include "app/console_layout.h"
#include "app/console_state.h"
#include "app/settings_store.h"

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>

namespace {

void test_layout_scaling() {
    const auto size = termite::console_layout::constrain_aspect_ratio({1500.0F, 800.0F});
    assert(std::abs(size.width / size.height - termite::console_design_width / termite::console_design_height) < 0.001F);

    const auto minimum = termite::console_layout::constrain_aspect_ratio({20.0F, 20.0F});
    assert(minimum.width >= termite::console_design_width * 0.75F);
    assert(minimum.height >= termite::console_design_height * 0.75F);

    const auto fitted = termite::console_layout::fit_canvas_to_bounds({1120.0F, 629.0F});
    assert(fitted.width <= 1120.0F);
    assert(fitted.height <= 629.0F);
    assert(std::abs(fitted.width / fitted.height - termite::console_design_width / termite::console_design_height) < 0.001F);

    const auto enlarged = termite::console_layout::fit_canvas_to_bounds({2000.0F, 1200.0F}, 1.20F);
    assert(std::abs(enlarged.width - termite::console_design_width * 1.20F) < 0.001F);
}

void test_hit_testing() {
    const auto close_box = termite::console_layout::close_button();
    const auto close = termite::console_layout::hit_test({close_box.x + close_box.width * 0.5F, close_box.y + close_box.height * 0.5F}, 20, 0.0F);
    assert(close.control == termite::console_control::close);

    const auto title = termite::console_layout::hit_test({220.0F, 10.0F}, 20, 0.0F);
    assert(title.control == termite::console_control::title_drag);

    const auto first_track = termite::console_layout::fader_track(0);
    const auto track = termite::console_layout::hit_test({first_track.x + first_track.width * 0.5F, first_track.y + first_track.height * 0.5F}, 20, 0.0F);
    assert(track.control == termite::console_control::fader_track);
    assert(track.index == 0);

    const auto single_console_track = termite::console_layout::hit_test({first_track.x + first_track.width * 0.5F, first_track.y + first_track.height * 0.5F}, 20, 0.0F,
                                                                          termite::console_tab::effects_rack);
    assert(single_console_track.control == termite::console_control::fader_track);
    assert(termite::console_layout::tab_bar().height == 0.0F);

    const auto first_value = termite::console_layout::fader_value(0);
    const auto value = termite::console_layout::hit_test({first_value.x + first_value.width * 0.5F, first_value.y + first_value.height * 0.5F}, 20, 0.0F);
    assert(value.control == termite::console_control::fader_value);
    assert(value.index == 0);

    const auto thumb = termite::console_layout::status_scroll_thumb(100, 90.0F);
    assert(thumb.height >= 22.0F);
    assert(thumb.y >= termite::console_layout::status_scroll_track().y);

    constexpr termite::console_control left_controls[]{
        termite::console_control::detect,
        termite::console_control::status_sync,
        termite::console_control::clear_info,
    };
    for (const auto control : left_controls) {
        const auto rect = termite::console_layout::control_rect(control);
        assert(rect.width > 0.0F && rect.height > 0.0F);
        const auto hit = termite::console_layout::hit_test({rect.x + rect.width * 0.5F, rect.y + rect.height * 0.5F}, 20, 0.0F);
        assert(hit.control == control);
    }

    // Controls without an implementation are not interactive surfaces. The
    // title no longer carries dead File/Hardware/Themes/Help menu entries.
    for (const auto control : {termite::console_control::file_menu, termite::console_control::hardware_menu,
                               termite::console_control::themes_menu, termite::console_control::help_menu}) {
        const auto rect = termite::console_layout::control_rect(control);
        const auto hit = termite::console_layout::hit_test({rect.x + rect.width * 0.5F, rect.y + rect.height * 0.5F}, 20, 0.0F);
        assert(hit.control != control);
    }
    assert(termite::console_layout::group_rect(termite::console_group::blender).width == 0.0F);
    assert(termite::console_layout::group_rect(termite::console_group::smoothing).width == 0.0F);

    for (const auto control : {termite::console_control::effect_bass_toggle, termite::console_control::effect_loudness_toggle,
                               termite::console_control::effect_clarity_toggle, termite::console_control::effect_stereo_toggle,
                               termite::console_control::effect_mono, termite::console_control::effect_balance_left,
                               termite::console_control::effect_balance_right}) {
        const auto box = termite::console_layout::control_rect(control);
        assert(box.width > 0.0F && box.height > 0.0F);
    }
}

void test_title_and_menu_alignment() {
    const auto title = termite::console_layout::title_bar();
    const auto icon = termite::console_layout::title_icon();
    const auto label = termite::console_layout::title_label();
    assert(icon.x >= title.x && icon.y >= title.y);
    assert(icon.right() <= title.right() && icon.bottom() <= title.bottom());
    assert(label.x >= title.x && label.y >= title.y);
    assert(label.right() <= title.right() && label.bottom() <= title.bottom());
    assert(termite::console_layout::minimize_button().right() < termite::console_layout::close_button().x);
    assert(termite::console_layout::close_button().right() <= title.right());
    assert(std::abs((termite::console_layout::minimize_button().y + termite::console_layout::minimize_button().height * 0.5F) -
                    (title.y + title.height * 0.5F)) < 0.001F);
    assert(std::abs((termite::console_layout::close_button().y + termite::console_layout::close_button().height * 0.5F) -
                    (title.y + title.height * 0.5F)) < 0.001F);

    const auto menu = termite::console_layout::menu_bar();
    const auto graph = termite::console_layout::graph_frame();
    assert(menu.bottom() == graph.y);
    assert(graph.x > 0.0F);
}

void test_graph_and_equalizer_alignment() {
    const auto frame = termite::console_layout::graph_frame();
    const auto plot = termite::console_layout::graph_plot();
    const auto x_axis = termite::console_layout::graph_x_axis();
    const auto y_axis = termite::console_layout::graph_y_axis();
    assert(plot.bottom() == x_axis.y);
    assert(y_axis.bottom() == x_axis.y);
    assert(plot.bottom() <= frame.bottom());

    for (const auto control : {termite::console_control::equalizer_off, termite::console_control::equalizer_on}) {
        const auto hit_box = termite::console_layout::control_rect(control);
        assert(hit_box.height == 26.0F);
    }
    const auto bypass = termite::console_layout::control_rect(termite::console_control::equalizer_off);
    const auto on = termite::console_layout::control_rect(termite::console_control::equalizer_on);
    assert(bypass.right() == on.x);
}

void test_derived_console_geometry() {
    const auto graph = termite::console_layout::graph_frame();
    const auto plot = termite::console_layout::graph_plot();
    const auto fader_bank = termite::console_layout::fader_bank();
    const auto first_track = termite::console_layout::fader_track(0);
    const auto last_track = termite::console_layout::fader_track(termite::console_fader_count - 1);
    assert(std::abs(first_track.x - fader_bank.x) < 0.001F);
    assert(std::abs(last_track.right() - fader_bank.right()) < 0.001F);
    assert(fader_bank.x > plot.x);
    assert(fader_bank.right() < plot.right());
    assert(std::abs((fader_bank.x + fader_bank.width * 0.5F) - (graph.x + graph.width * 0.5F)) < 0.001F);
    for (std::size_t index = 0; index < termite::console_fader_count; ++index) {
        const auto up = termite::console_layout::fader_up(index);
        const auto track = termite::console_layout::fader_track(index);
        const auto down = termite::console_layout::fader_down(index);
        const auto value = termite::console_layout::fader_value(index);
        const auto frequency = termite::console_layout::fader_frequency_label(index);
        const auto center = track.x + track.width * 0.5F;
        assert(std::abs(center - termite::console_layout::graph_x_for_frequency(termite::graphic_band_frequencies[index])) < 0.001F);
        assert(std::abs((up.x + up.width * 0.5F) - center) < 0.001F);
        assert(std::abs((down.x + down.width * 0.5F) - center) < 0.001F);
        assert(std::abs((value.x + value.width * 0.5F) - center) < 0.001F);
        assert(std::abs((frequency.x + frequency.width * 0.5F) - center) < 0.001F);
    }
    for (std::size_t index = 0; index < 3; ++index) {
        assert(termite::console_layout::graph_db_label(index).right() <= first_track.x - 5.0F);
    }
    assert(std::abs((termite::console_layout::graph_db_label(0).y + termite::console_layout::graph_db_label(0).height * 0.5F) - first_track.y) < 0.001F);
    assert(std::abs((termite::console_layout::graph_db_label(1).y + termite::console_layout::graph_db_label(1).height * 0.5F) -
                    (first_track.y + first_track.height * 0.5F)) < 0.001F);
    assert(std::abs((termite::console_layout::graph_db_label(2).y + termite::console_layout::graph_db_label(2).height * 0.5F) - first_track.bottom()) < 0.001F);

    const auto equalizer = termite::console_layout::group_rect(termite::console_group::equalizer);
    const auto toggle = termite::console_layout::equalizer_toggle();
    assert(std::abs((toggle.x + toggle.width * 0.5F) - (equalizer.x + equalizer.width * 0.5F)) < 0.001F);

    const auto profiles = termite::console_layout::group_rect(termite::console_group::profiles);
    const auto presets = termite::console_layout::group_rect(termite::console_group::presets);
    const auto tone = termite::console_layout::group_rect(termite::console_group::tone);
    const auto stereo = termite::console_layout::group_rect(termite::console_group::stereo);
    const auto control = termite::console_layout::group_rect(termite::console_group::termite_control);
    assert(std::abs((presets.x - profiles.right()) - 6.0F) < 0.001F);
    assert(std::abs((tone.x - presets.right()) - 6.0F) < 0.001F);
    assert(std::abs((stereo.x - tone.right()) - 6.0F) < 0.001F);
    assert(std::abs((control.x - stereo.right()) - 6.0F) < 0.001F);
    const auto fader_center = [](std::size_t index) {
        const auto track = termite::console_layout::fader_track(index);
        return track.x + track.width * 0.5F;
    };
    assert(std::abs((profiles.right() + presets.x) * 0.5F - (fader_center(4) + fader_center(5)) * 0.5F) < 0.001F);
    const auto grid = termite::console_layout::control_rect(termite::console_control::grid);
    assert(grid.x >= control.x);
    assert(grid.right() <= control.right());
    const auto detect = termite::console_layout::control_rect(termite::console_control::detect);
    const auto sync = termite::console_layout::control_rect(termite::console_control::status_sync);
    const auto clear_log = termite::console_layout::control_rect(termite::console_control::clear_info);
    assert(std::abs((sync.x - detect.right()) - (clear_log.x - sync.right())) < 0.001F);
    assert(termite::console_layout::control_rect(termite::console_control::reset).width == 0.0F);

    const auto picker = termite::console_layout::routing_picker_frame(9);
    assert(picker.width > 0.0F && picker.height > 0.0F);
    assert(termite::console_layout::routing_picker_visible_rows(9) == 6);
    for (std::size_t index = 0; index < termite::console_layout::routing_picker_visible_rows(9); ++index) {
        const auto row = termite::console_layout::routing_picker_row(9, index);
        assert(row.x >= picker.x && row.right() <= picker.right());
        assert(row.y >= picker.y && row.bottom() <= picker.bottom());
    }
    for (const auto button : {termite::console_layout::routing_picker_refresh_button(9),
                              termite::console_layout::routing_picker_open_button(9),
                              termite::console_layout::routing_picker_mixer_button(9),
                              termite::console_layout::routing_picker_close_button(9)}) {
        assert(button.x >= picker.x && button.right() <= picker.right());
        assert(button.y >= picker.y && button.bottom() <= picker.bottom());
    }

    const auto preset_field = termite::console_layout::control_rect(termite::console_control::preset_cycle);
    const auto preset_popup = termite::console_layout::preset_dropdown_frame();
    assert(preset_popup.width >= preset_field.width);
    assert(preset_popup.bottom() < preset_field.y);
    for (std::size_t index = 0; index < termite::console_state::preset_count(); ++index) {
        const auto item = termite::console_layout::preset_dropdown_item(index);
        assert(item.x >= preset_popup.x && item.right() <= preset_popup.right());
        assert(item.y >= preset_popup.y && item.bottom() <= preset_popup.bottom());
        for (std::size_t other = index + 1; other < termite::console_state::preset_count(); ++other) {
            const auto next = termite::console_layout::preset_dropdown_item(other);
            assert(item.right() <= next.x || next.right() <= item.x || item.bottom() <= next.y || next.bottom() <= item.y);
        }
    }

    const auto diagnostics = termite::console_layout::hardware_diagnostics_frame();
    assert(diagnostics.width > 0.0F && diagnostics.height > 0.0F);
    assert(diagnostics.x >= graph.x && diagnostics.right() <= graph.right());
}

void test_settings_store() {
    const auto path = std::filesystem::temp_directory_path() / "termite-settings-test.json";
    std::error_code error;
    std::filesystem::remove(path, error);
    termite::settings_store store(path);
    termite::termite_settings settings;
    settings.console.profile.bands[4].gain_db = 7.5F;
    settings.console.profile.bands[4].q = 2.3F;
    settings.console.profile.effects.bass_enabled = true;
    settings.console.profile.effects.bass_db = 5.0F;
    settings.console.profile.effects.stereo_enabled = true;
    settings.console.profile.effects.stereo_width = 1.2F;
    settings.console.grid_visible = false;
    settings.console.preset_index = static_cast<int>(termite::console_state::preset_count()) - 1;
    settings.window = {120, 80, 1245, 700, true};
    settings.routing_executables = {L"C:\\Apps\\Player.exe", L"C:\\Apps\\Browser.exe"};
    std::wstring failure;
    assert(store.save(settings, failure));
    const auto loaded = store.load();
    assert(loaded.loaded);
    assert(std::abs(loaded.settings.console.profile.bands[4].gain_db - 7.5F) < 0.001F);
    assert(std::abs(loaded.settings.console.profile.bands[4].q - 2.3F) < 0.001F);
    assert(!loaded.settings.console.grid_visible);
    assert(loaded.settings.console.preset_index == static_cast<int>(termite::console_state::preset_count()) - 1);
    assert(loaded.settings.console.profile.effects.bass_enabled);
    assert(std::abs(loaded.settings.console.profile.effects.bass_db - 5.0F) < 0.001F);
    assert(loaded.settings.console.active_tab == termite::console_tab::graphic_eq);
    assert(loaded.settings.window.valid);
    assert(loaded.settings.routing_executables.size() == 2);

    const auto profile_path = std::filesystem::temp_directory_path() / "termite-custom-profile.termiteeq";
    std::filesystem::remove(profile_path, error);
    auto custom_profile = termite::eq_profile::flat();
    custom_profile.enabled = false;
    custom_profile.preamp_db = -4.0F;
    custom_profile.bands[2].shape = termite::filter_shape::low_shelf;
    custom_profile.bands[2].gain_db = 8.5F;
    custom_profile.bands[2].q = 0.7F;
    custom_profile.effects.clarity_enabled = true;
    custom_profile.effects.clarity_db = 3.0F;
    assert(termite::settings_store::save_profile_file(profile_path, custom_profile, failure));
    const auto custom_loaded = termite::settings_store::load_profile_file(profile_path);
    assert(custom_loaded.loaded);
    assert(!custom_loaded.profile.enabled);
    assert(std::abs(custom_loaded.profile.preamp_db + 4.0F) < 0.001F);
    assert(custom_loaded.profile.bands[2].shape == termite::filter_shape::low_shelf);
    assert(std::abs(custom_loaded.profile.bands[2].gain_db - 8.5F) < 0.001F);
    assert(std::abs(custom_loaded.profile.bands[2].q - 0.7F) < 0.001F);
    assert(custom_loaded.profile.effects.clarity_enabled);
    assert(std::abs(custom_loaded.profile.effects.clarity_db - 3.0F) < 0.001F);
    std::filesystem::remove(profile_path, error);

    const auto legacy_profile_path = std::filesystem::temp_directory_path() / "termite-v1-profile.termiteeq";
    {
        std::ofstream legacy(legacy_profile_path, std::ios::trunc);
        legacy << "{\"format\":\"termite-eq-profile\",\"version\":1,\"profile\":{\"enabled\":true,\"preamp_db\":0,\"limiter_ceiling_db\":-1,\"bands\":[";
        for (std::size_t index = 0; index < termite::graphic_band_count; ++index) {
            if (index != 0) legacy << ',';
            legacy << "{\"shape\":0,\"gain_db\":0,\"q\":1,\"enabled\":true}";
        }
        legacy << "]}}";
    }
    const auto legacy_loaded = termite::settings_store::load_profile_file(legacy_profile_path);
    assert(legacy_loaded.loaded);
    assert(!legacy_loaded.profile.effects.bass_enabled);
    assert(std::abs(legacy_loaded.profile.effects.stereo_width - 1.0F) < 0.001F);
    std::filesystem::remove(legacy_profile_path, error);

    settings.console.profile.bands[4].gain_db = 999.0F;
    settings.console.profile.bands[4].q = 99.0F;
    settings.console.profile.preamp_db = -99.0F;
    settings.console.profile.limiter_ceiling_db = 8.0F;
    assert(store.save(settings, failure));
    const auto clamped = store.load();
    assert(clamped.loaded);
    assert(clamped.settings.console.profile.bands[4].gain_db == 20.0F);
    assert(clamped.settings.console.profile.bands[4].q == 12.0F);
    assert(clamped.settings.console.profile.preamp_db == -24.0F);
    assert(clamped.settings.console.profile.limiter_ceiling_db == 0.0F);

    {
        std::ofstream corrupt(path, std::ios::trunc);
        corrupt << "not json";
    }
    const auto corrupt = store.load();
    assert(!corrupt.loaded);
    assert(!corrupt.notice.empty());
    std::filesystem::remove(path, error);
}

void test_display_layout_centering() {
    const termite::console_rect narrow_value{0.0F, 0.0F, 33.0F, 20.0F};
    const auto signed_decimal = termite::console_layout::measure_display_layout(L"-1.3", narrow_value);
    assert(signed_decimal.digit_width > 0.0F);
    assert(signed_decimal.origin_x >= narrow_value.x);
    assert(signed_decimal.origin_x + signed_decimal.content_width <= narrow_value.right());
    assert(std::abs((signed_decimal.origin_x + signed_decimal.content_width * 0.5F) - 16.5F) < 0.001F);
    assert(signed_decimal.digit_width < narrow_value.height * 0.4F);

    const termite::console_rect mix_value{0.0F, 0.0F, 44.0F, 27.0F};
    const auto mix = termite::console_layout::measure_display_layout(L"100", mix_value);
    assert(mix.origin_x > mix_value.x);
    assert(std::abs((mix.origin_x + mix.content_width * 0.5F) - 22.0F) < 0.001F);
}

void test_console_commands() {
    termite::console_state state;
    assert(state.profile().enabled);

    const auto disable = state.activate(termite::console_control::equalizer_off);
    assert(disable.profile_changed);
    assert(!state.profile().enabled);

    const auto enable = state.activate(termite::console_control::equalizer_on);
    assert(enable.profile_changed);
    assert(state.profile().enabled);

    const auto status = state.activate(termite::console_control::status_sync);
    assert(status.request_engine_status);
    assert(status.open_diagnostics);

    assert(state.set_fader_gain(0, 8.0F));
    assert(std::abs(state.profile().bands[0].gain_db - 8.0F) < 0.01F);
    const auto reset = state.activate(termite::console_control::reset);
    assert(reset.profile_changed);
    assert(std::abs(state.profile().bands[0].gain_db) < 0.01F);
    assert(state.set_fader_gain(0, 999.0F));
    assert(std::abs(state.profile().bands[0].gain_db - 20.0F) < 0.01F);

    assert(termite::console_layout::snap_fader_gain(-1.8F) == -2.0F);
    assert(termite::console_layout::snap_fader_gain(1.8F) == 2.0F);
    assert(state.set_fader_gain(0, -1.9F));
    assert(state.adjust_fader_gain(0, 0.1F));
    assert(std::abs(state.profile().bands[0].gain_db + 1.8F) < 0.01F);
    assert(state.adjust_fader_q(0, 0.5F));
    assert(state.set_fader_shape(0, termite::filter_shape::notch));
    assert(state.set_fader_enabled(0, false));
    assert(!state.profile().bands[0].enabled);
    assert(termite::console_state::preset_count() == 12);
    for (std::size_t index = 0; index < termite::console_state::preset_count(); ++index) {
        assert(!termite::console_state::preset_name(index).empty());
        assert(state.apply_preset(index));
        assert(state.selected_preset_index() == static_cast<int>(index));
        assert(state.profile().enabled);
        for (const auto& band : state.profile().bands) {
            assert(std::isfinite(band.gain_db));
            assert(band.gain_db >= -20.0F && band.gain_db <= 20.0F);
        }
    }
    assert(state.preset_label() == L"Wide music");
    assert(!state.apply_preset(termite::console_state::preset_count()));

    auto custom_profile = termite::eq_profile::flat();
    custom_profile.enabled = false;
    custom_profile.bands[3].gain_db = 6.5F;
    state.set_profile(custom_profile);
    assert(!state.profile().enabled);
    assert(std::abs(state.profile().bands[3].gain_db - 6.5F) < 0.01F);

    const auto grid = state.grid_visible();
    state.activate(termite::console_control::grid);
    assert(state.grid_visible() != grid);

    const auto route = state.activate(termite::console_control::route_apps);
    assert(route.open_routing);
    const auto effects = state.activate(termite::console_control::effect_bass_toggle);
    assert(effects.profile_changed && state.profile().effects.bass_enabled);
    const auto diagnostics = state.activate(termite::console_control::status_sync);
    assert(diagnostics.open_diagnostics);
    assert(!diagnostics.open_routing);
}

void test_every_visible_command() {
    termite::console_state state;
    constexpr termite::console_control controls[]{
        termite::console_control::minimize,
        termite::console_control::close,
        termite::console_control::detect,
        termite::console_control::status_sync,
        termite::console_control::clear_info,
        termite::console_control::equalizer_off,
        termite::console_control::equalizer_on,
        termite::console_control::volume_up,
        termite::console_control::volume_down,
        termite::console_control::profile_open,
        termite::console_control::profile_save,
        termite::console_control::preset_zero,
        termite::console_control::preset_cycle,
        termite::console_control::route_apps,
        termite::console_control::grid,
    };
    for (const auto control : controls) {
        [[maybe_unused]] const auto result = state.activate(control);
    }

    for (std::size_t index = 0; index < termite::console_fader_count; ++index) {
        assert(state.set_fader_gain(index, 11.0F));
        assert(state.adjust_fader_gain(index, -1.0F));
    }
}

}  // namespace

int main() {
    test_layout_scaling();
    test_hit_testing();
    test_title_and_menu_alignment();
    test_graph_and_equalizer_alignment();
    test_derived_console_geometry();
    test_settings_store();
    test_display_layout_centering();
    test_console_commands();
    test_every_visible_command();
}
