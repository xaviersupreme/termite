#include "app/console_layout.h"
#include "app/console_state.h"

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <cassert>
#include <cmath>

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

    const auto first_value = termite::console_layout::fader_value(0);
    const auto value = termite::console_layout::hit_test({first_value.x + first_value.width * 0.5F, first_value.y + first_value.height * 0.5F}, 20, 0.0F);
    assert(value.control == termite::console_control::fader_value);
    assert(value.index == 0);

    const auto thumb = termite::console_layout::status_scroll_thumb(100, 90.0F);
    assert(thumb.height >= 22.0F);
    assert(thumb.y >= termite::console_layout::status_scroll_track().y);

    constexpr termite::console_control left_controls[]{
        termite::console_control::detect,
        termite::console_control::reset,
        termite::console_control::status_sync,
        termite::console_control::pause,
        termite::console_control::run,
        termite::console_control::clear_info,
        termite::console_control::sleep,
        termite::console_control::default_start,
    };
    for (const auto control : left_controls) {
        const auto rect = termite::console_layout::control_rect(control);
        assert(rect.width > 0.0F && rect.height > 0.0F);
        const auto hit = termite::console_layout::hit_test({rect.x + rect.width * 0.5F, rect.y + rect.height * 0.5F}, 20, 0.0F);
        assert(hit.control == control);
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
    const auto plot = termite::console_layout::graph_plot();
    const auto fader_bank = termite::console_layout::fader_bank();
    const auto first_track = termite::console_layout::fader_track(0);
    const auto last_track = termite::console_layout::fader_track(termite::console_fader_count - 1);
    assert(std::abs(first_track.x - fader_bank.x) < 0.001F);
    assert(std::abs(last_track.right() - fader_bank.right()) < 0.001F);
    assert(fader_bank.x > plot.x);
    assert(std::abs(fader_bank.right() - plot.right()) < 0.001F);
    for (std::size_t index = 0; index < termite::console_fader_count; ++index) {
        const auto up = termite::console_layout::fader_up(index);
        const auto track = termite::console_layout::fader_track(index);
        const auto down = termite::console_layout::fader_down(index);
        const auto value = termite::console_layout::fader_value(index);
        const auto frequency = termite::console_layout::fader_frequency_label(index);
        const auto center = track.x + track.width * 0.5F;
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
    const auto smoothing = termite::console_layout::group_rect(termite::console_group::smoothing);
    const auto control = termite::console_layout::group_rect(termite::console_group::termite_control);
    assert(std::abs((presets.x - profiles.right()) - 6.0F) < 0.001F);
    assert(std::abs((smoothing.x - presets.right()) - 6.0F) < 0.001F);
    assert(std::abs((control.x - smoothing.right()) - 6.0F) < 0.001F);

    const auto smoothing_reset = termite::console_layout::control_rect(termite::console_control::smoothing_reset);
    assert(std::abs((smoothing_reset.x + smoothing_reset.width * 0.5F) - (smoothing.x + smoothing.width * 0.5F)) < 0.001F);
    const auto grid = termite::console_layout::control_rect(termite::console_control::grid);
    assert(grid.x >= control.right());
    const auto detect = termite::console_layout::control_rect(termite::console_control::detect);
    const auto reset = termite::console_layout::control_rect(termite::console_control::reset);
    const auto sync = termite::console_layout::control_rect(termite::console_control::status_sync);
    assert(std::abs((reset.x - detect.right()) - (sync.x - reset.right())) < 0.001F);

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
                              termite::console_layout::routing_picker_close_button(9)}) {
        assert(button.x >= picker.x && button.right() <= picker.right());
        assert(button.y >= picker.y && button.bottom() <= picker.bottom());
    }
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

    const auto grid = state.grid_visible();
    state.activate(termite::console_control::grid);
    assert(state.grid_visible() != grid);

    const auto background = state.background_index();
    state.activate(termite::console_control::themes_menu);
    assert(state.background_index() == background);

    const auto route = state.activate(termite::console_control::route_apps);
    assert(route.open_routing);
}

void test_every_visible_command() {
    termite::console_state state;
    constexpr termite::console_control controls[]{
        termite::console_control::minimize,
        termite::console_control::close,
        termite::console_control::file_menu,
        termite::console_control::hardware_menu,
        termite::console_control::themes_menu,
        termite::console_control::help_menu,
        termite::console_control::detect,
        termite::console_control::reset,
        termite::console_control::status_sync,
        termite::console_control::pause,
        termite::console_control::run,
        termite::console_control::clear_info,
        termite::console_control::sleep,
        termite::console_control::default_start,
        termite::console_control::equalizer_off,
        termite::console_control::equalizer_on,
        termite::console_control::blender_increase,
        termite::console_control::blender_decrease,
        termite::console_control::volume_up,
        termite::console_control::volume_down,
        termite::console_control::profile_open,
        termite::console_control::profile_save,
        termite::console_control::preset_zero,
        termite::console_control::preset_cycle,
        termite::console_control::smoothing_reset,
        termite::console_control::smoothing_decrease,
        termite::console_control::smoothing_increase,
        termite::console_control::route_apps,
        termite::console_control::export_response,
        termite::console_control::grid,
        termite::console_control::help_button,
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
    test_display_layout_centering();
    test_console_commands();
    test_every_visible_command();
}
