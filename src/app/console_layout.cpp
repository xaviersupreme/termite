#include "app/console_layout.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace termite {
namespace {

constexpr console_rect title_box{0.0F, 0.0F, console_design_width, 24.0F};
constexpr console_rect title_icon_box{6.0F, 4.0F, 15.0F, 15.0F};
constexpr console_rect title_label_box{27.0F, 3.0F, 220.0F, 18.0F};
constexpr console_rect menu_box{0.0F, 24.0F, console_design_width, 24.0F};
constexpr float left_bay_width = 277.0F;
constexpr console_rect left_bay_box{0.0F, menu_box.y + menu_box.height, left_bay_width, console_design_height - (menu_box.y + menu_box.height)};

constexpr float status_inset = 17.0F;
constexpr float status_top_gap = 25.0F;
constexpr console_rect status_box{status_inset, left_bay_box.y + status_top_gap, left_bay_box.width - status_inset - 5.0F, 176.0F};
constexpr console_rect scroll_track{status_box.x + status_box.width - 17.0F, status_box.y + 3.0F, 14.0F, status_box.height - 7.0F};

constexpr console_rect graph_box{left_bay_width, menu_box.y + menu_box.height, console_design_width - left_bay_width, 303.0F};
constexpr float graph_axis_gutter = 37.0F;
constexpr console_rect graph_area{graph_box.x + graph_axis_gutter, graph_box.y + 31.0F, graph_box.width - graph_axis_gutter - 10.0F, graph_box.height - 31.0F - 28.0F};
constexpr console_rect graph_y_axis_box{graph_area.x - 1.0F, graph_area.y, 1.0F, graph_area.height};
constexpr console_rect graph_x_axis_box{graph_area.x, graph_area.y + graph_area.height, graph_area.width, 1.0F};

constexpr console_rect equalizer_box{37.0F, 400.0F, 112.0F, 59.0F};
constexpr console_rect blender_box{37.0F, 476.0F, 110.0F, 172.0F};
constexpr console_rect digital_volume_box{169.0F, 400.0F, 96.0F, 248.0F};

constexpr float fader_track_width = 25.0F;
constexpr float fader_arrow_width = 29.0F;
constexpr float fader_arrow_height = 16.0F;
constexpr float fader_track_height = 93.0F;
constexpr float fader_value_width = 33.0F;
constexpr float fader_value_height = 20.0F;
constexpr float fader_vertical_gap = 4.0F;
constexpr float fader_track_y = graph_box.y + graph_box.height + 50.0F;
constexpr float fader_up_y = fader_track_y - fader_vertical_gap - fader_arrow_height;
constexpr float fader_down_y = fader_track_y + fader_track_height + fader_vertical_gap;
constexpr float fader_value_y = fader_down_y + fader_arrow_height + 7.0F;
// Every fader primitive is derived from this shared column center.  The extra
// left gutter reserves room for the dB labels before the first fader.
constexpr float fader_db_gutter = 75.0F;
constexpr float fader_first_center = graph_box.x + fader_db_gutter + fader_track_width * 0.5F;
constexpr float fader_last_center = graph_area.x + graph_area.width - fader_track_width * 0.5F;
constexpr float fader_pitch = (fader_last_center - fader_first_center) / static_cast<float>(console_fader_count - 1);
constexpr float fader_bank_left = fader_first_center - fader_track_width * 0.5F;
constexpr float fader_bank_width = fader_last_center + fader_track_width * 0.5F - fader_bank_left;

constexpr float bottom_group_top = fader_value_y + fader_value_height + 20.0F;
constexpr float bottom_group_height = 81.0F;
constexpr float bottom_group_gap = 6.0F;
constexpr float bottom_content_left = 426.0F;
constexpr console_rect profiles_box{bottom_content_left, bottom_group_top, 120.0F, bottom_group_height};
constexpr console_rect presets_box{profiles_box.x + profiles_box.width + bottom_group_gap, bottom_group_top, 116.0F, bottom_group_height};
constexpr console_rect smoothing_box{presets_box.x + presets_box.width + bottom_group_gap, bottom_group_top, 185.0F, bottom_group_height};
constexpr console_rect termite_control_box{smoothing_box.x + smoothing_box.width + bottom_group_gap, bottom_group_top, 219.0F, bottom_group_height};

constexpr std::size_t routing_picker_max_rows = 6;
constexpr float routing_picker_header_height = 24.0F;
constexpr float routing_picker_instruction_height = 28.0F;
constexpr float routing_picker_row_height = 24.0F;
constexpr float routing_picker_guidance_height = 25.0F;
constexpr float routing_picker_footer_height = 35.0F;
constexpr float routing_picker_width = 470.0F;

constexpr float left_button_rail_left = status_box.x + 6.0F;
constexpr float left_button_rail_width = status_box.width - 9.0F;
constexpr float left_button_gap = 6.0F;
constexpr float left_button_width = (left_button_rail_width - left_button_gap * 2.0F) / 3.0F;
constexpr float left_button_height = 25.0F;
constexpr float left_button_first_y = status_box.y + status_box.height + 36.0F;
constexpr float left_button_row_gap = 7.0F;
constexpr std::array<console_rect, 8> left_buttons{{
    {left_button_rail_left, left_button_first_y, left_button_width, left_button_height},
    {left_button_rail_left + left_button_width + left_button_gap, left_button_first_y, left_button_width, left_button_height},
    {left_button_rail_left + (left_button_width + left_button_gap) * 2.0F, left_button_first_y, left_button_width, left_button_height},
    {left_button_rail_left, left_button_first_y + left_button_height + left_button_row_gap, left_button_width, left_button_height},
    {left_button_rail_left + left_button_width + left_button_gap, left_button_first_y + left_button_height + left_button_row_gap, left_button_width, left_button_height},
    {left_button_rail_left + (left_button_width + left_button_gap) * 2.0F, left_button_first_y + left_button_height + left_button_row_gap, left_button_width, left_button_height},
    {left_button_rail_left, left_button_first_y + (left_button_height + left_button_row_gap) * 2.0F, left_button_width, left_button_height},
    {left_button_rail_left + left_button_width + left_button_gap, left_button_first_y + (left_button_height + left_button_row_gap) * 2.0F, left_button_width, left_button_height},
}};

constexpr std::array<console_control, 8> left_button_controls{{
    console_control::detect, console_control::reset, console_control::status_sync, console_control::pause,
    console_control::run, console_control::clear_info, console_control::sleep, console_control::default_start,
}};

}  // namespace

bool console_rect::contains(console_point point) const noexcept {
    return point.x >= x && point.y >= y && point.x < right() && point.y < bottom();
}

float console_rect::right() const noexcept {
    return x + width;
}

float console_rect::bottom() const noexcept {
    return y + height;
}

console_rect console_layout::title_bar() noexcept {
    return title_box;
}

console_rect console_layout::title_icon() noexcept { return title_icon_box; }
console_rect console_layout::title_label() noexcept { return title_label_box; }

console_rect console_layout::menu_bar() noexcept { return menu_box; }
console_rect console_layout::minimize_button() noexcept {
    return {title_box.right() - 49.0F, 4.0F, 17.0F, 16.0F};
}

console_rect console_layout::close_button() noexcept {
    return {title_box.right() - 27.0F, 4.0F, 17.0F, 16.0F};
}

console_rect console_layout::status_viewport() noexcept {
    return status_box;
}

console_rect console_layout::status_scroll_track() noexcept {
    return scroll_track;
}

console_rect console_layout::notification_label() noexcept {
    return {status_box.x + 3.0F, menu_box.y + menu_box.height, status_box.width - scroll_track.width - 92.0F, status_top_gap - 5.0F};
}

console_rect console_layout::status_scroll_thumb(std::size_t message_count, float scroll_offset) noexcept {
    const auto content_height = std::max(1.0F, static_cast<float>(message_count) * 15.0F);
    const auto visible_height = status_box.height - 8.0F;
    constexpr float arrow_height = 16.0F;
    const auto channel_height = scroll_track.height - arrow_height * 2.0F;
    const auto thumb_height = std::clamp(channel_height * visible_height / content_height, 22.0F, channel_height);
    const auto maximum_scroll = std::max(0.0F, content_height - visible_height);
    const auto position = maximum_scroll == 0.0F ? 0.0F : std::clamp(scroll_offset / maximum_scroll, 0.0F, 1.0F) * (channel_height - thumb_height);
    return {scroll_track.x + 1.0F, scroll_track.y + arrow_height + position, scroll_track.width - 2.0F, thumb_height};
}

console_rect console_layout::graph_frame() noexcept {
    return graph_box;
}

console_rect console_layout::graph_plot() noexcept {
    return graph_area;
}

console_rect console_layout::graph_y_axis() noexcept { return graph_y_axis_box; }
console_rect console_layout::graph_x_axis() noexcept { return graph_x_axis_box; }

console_rect console_layout::graph_title() noexcept {
    return {graph_box.x, graph_box.y + 5.0F, graph_box.width, 22.0F};
}

console_rect console_layout::graph_gain_label() noexcept {
    const auto axis = graph_y_axis();
    const float pivot_x = axis.x - 19.0F;
    const float pivot_y = graph_area.y + graph_area.height * 0.5F;
    return {pivot_x - 23.0F, pivot_y - 18.0F, 48.0F, 15.0F};
}

console_rect console_layout::graph_frequency_label() noexcept {
    constexpr float label_width = 100.0F;
    return {graph_area.x + (graph_area.width - label_width) * 0.5F, graph_x_axis_box.y, label_width, 17.0F};
}

console_rect console_layout::group_rect(console_group group) noexcept {
    switch (group) {
        case console_group::left_bay: return left_bay_box;
        case console_group::equalizer: return equalizer_box;
        case console_group::blender: return blender_box;
        case console_group::digital_volume: return digital_volume_box;
        case console_group::profiles: return profiles_box;
        case console_group::presets: return presets_box;
        case console_group::smoothing: return smoothing_box;
        case console_group::termite_control: return termite_control_box;
    }
    return {};
}

console_rect console_layout::equalizer_label() noexcept {
    return {equalizer_box.x + 16.0F, equalizer_box.y - 6.0F, equalizer_box.width - 32.0F, 15.0F};
}

console_rect console_layout::equalizer_toggle() noexcept {
    constexpr float toggle_width = 94.0F;
    constexpr float toggle_height = 26.0F;
    return {equalizer_box.x + (equalizer_box.width - toggle_width) * 0.5F, equalizer_box.y + 19.0F, toggle_width, toggle_height};
}

console_rect console_layout::blender_label() noexcept {
    return {blender_box.x + 6.0F, blender_box.y - 6.0F, blender_box.width - 12.0F, 15.0F};
}

console_rect console_layout::blender_dry_display() noexcept {
    constexpr float inset = 6.0F;
    constexpr float gap = 6.0F;
    const float width = (blender_box.width - inset * 2.0F - gap) * 0.5F;
    return {blender_box.x + inset, blender_box.y + 29.0F, width, 27.0F};
}

console_rect console_layout::blender_wet_display() noexcept {
    constexpr float gap = 6.0F;
    const auto dry = blender_dry_display();
    return {dry.x + dry.width + gap, dry.y, dry.width, dry.height};
}

console_rect console_layout::blender_dry_label() noexcept {
    const auto display = blender_dry_display();
    return {display.x, display.y - 17.0F, display.width, 13.0F};
}

console_rect console_layout::blender_wet_label() noexcept {
    const auto display = blender_wet_display();
    return {display.x, display.y - 17.0F, display.width, 13.0F};
}

console_rect console_layout::digital_volume_label() noexcept {
    constexpr float inset = 7.0F;
    return {digital_volume_box.x + inset, digital_volume_box.y - 6.0F, digital_volume_box.width - inset * 2.0F, 15.0F};
}

console_rect console_layout::digital_volume_display() noexcept {
    constexpr float control_inset = 7.0F;
    constexpr float display_inset = 1.0F;
    return {digital_volume_box.x + control_inset + display_inset, digital_volume_box.y + 16.0F, digital_volume_box.width - (control_inset + display_inset) * 2.0F, 35.0F};
}

console_rect console_layout::fader_up(std::size_t index) noexcept {
    const auto center = fader_first_center + static_cast<float>(index) * fader_pitch;
    return {center - fader_arrow_width * 0.5F, fader_up_y, fader_arrow_width, fader_arrow_height};
}

console_rect console_layout::fader_track(std::size_t index) noexcept {
    const auto center = fader_first_center + static_cast<float>(index) * fader_pitch;
    return {center - fader_track_width * 0.5F, fader_track_y, fader_track_width, fader_track_height};
}

console_rect console_layout::fader_down(std::size_t index) noexcept {
    const auto track = fader_track(index);
    return {track.x - (fader_arrow_width - fader_track_width) * 0.5F, fader_down_y, fader_arrow_width, fader_arrow_height};
}

console_rect console_layout::fader_value(std::size_t index) noexcept {
    const auto center = fader_first_center + static_cast<float>(index) * fader_pitch;
    return {center - fader_value_width * 0.5F, fader_value_y, fader_value_width, fader_value_height};
}

console_rect console_layout::fader_bank() noexcept {
    return {fader_bank_left, fader_track_y, fader_bank_width, fader_track_height};
}

console_rect console_layout::fader_frequency_label(std::size_t index) noexcept {
    constexpr float width = 43.0F;
    const auto center = fader_first_center + static_cast<float>(index) * fader_pitch;
    return {center - width * 0.5F, fader_up_y - 25.0F, width, 17.0F};
}

console_rect console_layout::graph_db_label(std::size_t index) noexcept {
    constexpr std::array label_widths{38.0F, 28.0F, 39.0F};
    if (index >= label_widths.size()) {
        return {};
    }

    constexpr float label_gap = 5.0F;
    const auto first_track = fader_track(0);
    const float x = first_track.x - label_gap - label_widths[index];
    constexpr float label_height = 13.0F;
    switch (index) {
        case 0: return {x, first_track.y - label_height * 0.5F, label_widths[index], label_height};
        case 1: return {x, first_track.y + (first_track.height - label_height) * 0.5F, label_widths[index], label_height};
        case 2: return {x, first_track.bottom() - label_height * 0.5F, label_widths[index], label_height};
        default: return {};
    }
}

console_rect console_layout::smoothing_value() noexcept {
    constexpr float button_width = 16.0F;
    constexpr float display_width = 108.0F;
    constexpr float gap = 7.0F;
    const float content_width = button_width * 2.0F + display_width + gap * 2.0F;
    const float x = smoothing_box.x + (smoothing_box.width - content_width) * 0.5F;
    return {x + button_width + gap, smoothing_box.y + 48.0F, display_width, 25.0F};
}

std::size_t console_layout::routing_picker_visible_rows(std::size_t candidate_count) noexcept {
    return std::clamp(candidate_count, std::size_t{1}, routing_picker_max_rows);
}

console_rect console_layout::routing_picker_frame(std::size_t candidate_count) noexcept {
    const auto rows = static_cast<float>(routing_picker_visible_rows(candidate_count));
    const float height = routing_picker_header_height + routing_picker_instruction_height + rows * routing_picker_row_height +
                         routing_picker_guidance_height + routing_picker_footer_height;
    return {graph_box.x + 85.0F, graph_box.y + 16.0F, routing_picker_width, height};
}

console_rect console_layout::routing_picker_row(std::size_t candidate_count, std::size_t visible_index) noexcept {
    const auto frame = routing_picker_frame(candidate_count);
    if (visible_index >= routing_picker_visible_rows(candidate_count)) return {};
    return {frame.x + 7.0F,
            frame.y + routing_picker_header_height + routing_picker_instruction_height + static_cast<float>(visible_index) * routing_picker_row_height,
            frame.width - 14.0F,
            routing_picker_row_height - 2.0F};
}

console_rect console_layout::routing_picker_refresh_button(std::size_t candidate_count) noexcept {
    const auto frame = routing_picker_frame(candidate_count);
    return {frame.x + 8.0F, frame.bottom() - 29.0F, 72.0F, 23.0F};
}

console_rect console_layout::routing_picker_open_button(std::size_t candidate_count) noexcept {
    const auto refresh = routing_picker_refresh_button(candidate_count);
    return {refresh.right() + 6.0F, refresh.y, 145.0F, refresh.height};
}

console_rect console_layout::routing_picker_close_button(std::size_t candidate_count) noexcept {
    const auto frame = routing_picker_frame(candidate_count);
    return {frame.right() - 68.0F, frame.bottom() - 29.0F, 60.0F, 23.0F};
}

float console_layout::snap_fader_gain(float gain_db) noexcept {
    return std::clamp(std::round(gain_db), -20.0F, 20.0F);
}

float console_layout::display_glyph_units(wchar_t character) noexcept {
    switch (character) {
        case L'.': return 0.34F;
        case L'-': return 0.66F;
        default: return 1.0F;
    }
}

console_display_layout console_layout::measure_display_layout(std::wstring_view text, console_rect bounds) noexcept {
    if (text.empty() || bounds.width <= 0.0F || bounds.height <= 0.0F) {
        return {};
    }

    constexpr float gap_units = 0.16F;
    const float pad = std::max(2.0F, bounds.height * 0.18F);
    const float digit_height = std::max(1.0F, bounds.height - pad * 2.0F);
    float units = gap_units * static_cast<float>(text.size() - 1);
    for (const auto character : text) {
        units += display_glyph_units(character);
    }
    const float available_width = std::max(1.0F, bounds.width - pad * 2.0F);
    const float digit_width = std::min(digit_height * 0.57F, available_width / std::max(1.0F, units));
    const float content_width = units * digit_width;
    return {bounds.x + (bounds.width - content_width) * 0.5F, content_width, digit_width};
}

console_rect console_layout::control_rect(console_control control) noexcept {
    switch (control) {
        case console_control::file_menu: return {10.0F, 27.0F, 28.0F, 19.0F};
        case console_control::hardware_menu: return {42.0F, 27.0F, 60.0F, 19.0F};
        case console_control::themes_menu: return {106.0F, 27.0F, 44.0F, 19.0F};
        case console_control::help_menu: return {154.0F, 27.0F, 29.0F, 19.0F};
        case console_control::detect: return left_buttons[0];
        case console_control::reset: return left_buttons[1];
        case console_control::status_sync: return left_buttons[2];
        case console_control::pause: return left_buttons[3];
        case console_control::run: return left_buttons[4];
        case console_control::clear_info: return left_buttons[5];
        case console_control::sleep: return left_buttons[6];
        case console_control::default_start: return left_buttons[7];
        case console_control::equalizer_off: {
            const auto toggle = equalizer_toggle();
            return {toggle.x, toggle.y, toggle.width * 0.5F, toggle.height};
        }
        case console_control::equalizer_on: {
            const auto toggle = equalizer_toggle();
            return {toggle.x + toggle.width * 0.5F, toggle.y, toggle.width * 0.5F, toggle.height};
        }
        case console_control::blender_increase: {
            constexpr float inset = 6.0F;
            return {blender_box.x + inset, blender_box.y + 63.0F, blender_box.width - inset * 2.0F, 48.0F};
        }
        case console_control::blender_decrease: {
            constexpr float inset = 6.0F;
            return {blender_box.x + inset, blender_box.y + 118.0F, blender_box.width - inset * 2.0F, 48.0F};
        }
        case console_control::volume_up: {
            constexpr float inset = 7.0F;
            return {digital_volume_box.x + inset, digital_volume_box.y + 66.0F, digital_volume_box.width - inset * 2.0F, 82.0F};
        }
        case console_control::volume_down: {
            constexpr float inset = 7.0F;
            return {digital_volume_box.x + inset, digital_volume_box.y + 157.0F, digital_volume_box.width - inset * 2.0F, 82.0F};
        }
        case console_control::profile_open: {
            constexpr float button_width = 96.0F;
            return {profiles_box.x + (profiles_box.width - button_width) * 0.5F, profiles_box.y + 16.0F, button_width, 25.0F};
        }
        case console_control::profile_save: {
            constexpr float button_width = 96.0F;
            return {profiles_box.x + (profiles_box.width - button_width) * 0.5F, profiles_box.y + 48.0F, button_width, 25.0F};
        }
        case console_control::preset_zero: {
            constexpr float button_width = 96.0F;
            return {presets_box.x + (presets_box.width - button_width) * 0.5F, presets_box.y + 16.0F, button_width, 25.0F};
        }
        case console_control::preset_cycle: {
            constexpr float button_width = 96.0F;
            return {presets_box.x + (presets_box.width - button_width) * 0.5F, presets_box.y + 48.0F, button_width, 25.0F};
        }
        case console_control::smoothing_reset: {
            constexpr float button_width = 70.0F;
            return {smoothing_box.x + (smoothing_box.width - button_width) * 0.5F, smoothing_box.y + 16.0F, button_width, 25.0F};
        }
        case console_control::smoothing_decrease: {
            constexpr float button_width = 16.0F;
            constexpr float gap = 7.0F;
            const auto display = smoothing_value();
            return {display.x - gap - button_width, display.y, button_width, display.height};
        }
        case console_control::smoothing_increase: {
            constexpr float button_width = 16.0F;
            constexpr float gap = 7.0F;
            const auto display = smoothing_value();
            return {display.right() + gap, display.y, button_width, display.height};
        }
        case console_control::route_apps: {
            constexpr float inset = 12.0F;
            constexpr float gap = 6.0F;
            const float width = (termite_control_box.width - inset * 2.0F - gap) * 0.5F;
            return {termite_control_box.x + inset, termite_control_box.y + 16.0F, width, 25.0F};
        }
        case console_control::export_response: {
            constexpr float inset = 12.0F;
            constexpr float gap = 6.0F;
            const float width = (termite_control_box.width - inset * 2.0F - gap) * 0.5F;
            return {termite_control_box.x + inset + width + gap, termite_control_box.y + 16.0F, width, 25.0F};
        }
        case console_control::grid: {
            constexpr float utility_gap = 8.0F;
            return {termite_control_box.x + termite_control_box.width + utility_gap, termite_control_box.y + 16.0F, 70.0F, 25.0F};
        }
        case console_control::help_button: {
            constexpr float utility_gap = 8.0F;
            return {termite_control_box.x + termite_control_box.width + utility_gap, termite_control_box.y + 48.0F, 70.0F, 25.0F};
        }
        default: return {};
    }
}

console_hit console_layout::hit_test(console_point point, std::size_t message_count, float scroll_offset) noexcept {
    if (minimize_button().contains(point)) return {console_control::minimize};
    if (close_button().contains(point)) return {console_control::close};
    if (status_scroll_thumb(message_count, scroll_offset).contains(point)) return {console_control::scroll_thumb};

    for (std::size_t index = 0; index < console_fader_count; ++index) {
        if (fader_up(index).contains(point)) return {console_control::fader_up, static_cast<int>(index)};
        if (fader_track(index).contains(point)) return {console_control::fader_track, static_cast<int>(index)};
        if (fader_down(index).contains(point)) return {console_control::fader_down, static_cast<int>(index)};
        if (fader_value(index).contains(point)) return {console_control::fader_value, static_cast<int>(index)};
    }

    for (std::size_t index = 0; index < left_buttons.size(); ++index) {
        if (left_buttons[index].contains(point)) return {left_button_controls[index]};
    }

    constexpr std::array controls{
        console_control::file_menu, console_control::hardware_menu, console_control::themes_menu, console_control::help_menu,
        console_control::equalizer_off, console_control::equalizer_on, console_control::blender_increase, console_control::blender_decrease,
        console_control::volume_up, console_control::volume_down, console_control::profile_open, console_control::profile_save,
        console_control::preset_zero, console_control::preset_cycle, console_control::smoothing_reset, console_control::smoothing_decrease,
        console_control::smoothing_increase, console_control::route_apps, console_control::export_response,
        console_control::grid, console_control::help_button,
    };
    for (const auto control : controls) {
        if (control_rect(control).contains(point)) return {control};
    }

    if (title_bar().contains(point)) return {console_control::title_drag};
    return {};
}

console_size console_layout::constrain_aspect_ratio(console_size requested, float minimum_scale) noexcept {
    const auto minimum_width = console_design_width * minimum_scale;
    const auto minimum_height = console_design_height * minimum_scale;
    auto width = std::max(requested.width, minimum_width);
    auto height = std::max(requested.height, minimum_height);
    const auto scale = std::min(width / console_design_width, height / console_design_height);
    return {console_design_width * scale, console_design_height * scale};
}

console_size console_layout::fit_canvas_to_bounds(console_size available, float preferred_scale) noexcept {
    const auto safe_width = std::max(1.0F, available.width);
    const auto safe_height = std::max(1.0F, available.height);
    const auto fit_scale = std::min(safe_width / console_design_width, safe_height / console_design_height);
    const auto scale = std::clamp(std::min(preferred_scale, fit_scale), 0.1F, 1.0F);
    return {console_design_width * scale, console_design_height * scale};
}

}  // namespace termite
