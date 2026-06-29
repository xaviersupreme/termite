#include "app/console_layout.h"
#include "app/console_state.h"
#include "dsp/eq_profile.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace termite {
namespace {

constexpr console_rect title_box{0.0F, 0.0F, console_design_width, 24.0F};
constexpr console_rect title_icon_box{6.0F, 4.0F, 15.0F, 15.0F};
constexpr console_rect title_label_box{27.0F, 3.0F, 220.0F, 18.0F};
constexpr console_rect menu_box{277.0F, 24.0F, console_design_width - 277.0F, 30.0F};
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
constexpr float graph_frequency_min_hz = 30.0F;
constexpr float graph_first_band_hz = graphic_band_frequencies.front();
constexpr float graph_last_band_hz = graphic_band_frequencies.back();
constexpr float graph_frequency_max_hz = 18000.0F;

constexpr console_rect equalizer_box{37.0F, 376.0F, 112.0F, 59.0F};
constexpr console_rect blender_box{};
constexpr console_rect digital_volume_box{37.0F, 452.0F, 228.0F, 172.0F};

constexpr console_rect page_box{left_bay_width, 54.0F, console_design_width - left_bay_width, 500.0F};
constexpr float effects_card_width = 260.0F;
constexpr float effects_card_height = 116.0F;

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
// Use equal outer gutters so the fader bank is centered in the equalizer workspace.
constexpr float fader_last_center = graph_box.x + graph_box.width - fader_db_gutter - fader_track_width * 0.5F;

constexpr float bottom_group_top = fader_value_y + fader_value_height + 20.0F;
constexpr float bottom_group_height = 81.0F;
constexpr float bottom_group_gap = 6.0F;

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
constexpr std::array<console_rect, 3> left_buttons{{
    {left_button_rail_left, left_button_first_y, left_button_width, left_button_height},
    {left_button_rail_left + left_button_width + left_button_gap, left_button_first_y, left_button_width, left_button_height},
    {left_button_rail_left + (left_button_width + left_button_gap) * 2.0F, left_button_first_y, left_button_width, left_button_height},
}};

constexpr std::array<console_control, 3> left_button_controls{{
    console_control::detect, console_control::status_sync, console_control::clear_info,
}};

float logarithmic_fraction(float value, float low, float high) noexcept {
    const auto clamped = std::clamp(value, low, high);
    return std::log(clamped / low) / std::log(high / low);
}

float fader_center(std::size_t index) noexcept {
    return console_layout::graph_x_for_frequency(graphic_band_frequencies[std::min(index, graphic_band_frequencies.size() - 1)]);
}

console_rect bottom_group_rect(std::size_t first_column, std::size_t last_column) noexcept {
    const auto first = fader_center(first_column);
    const auto last = fader_center(last_column);
    const auto left_boundary = (fader_center(first_column - 1) + first) * 0.5F;
    const auto right_boundary = (last + fader_center(last_column + 1)) * 0.5F;
    const float inset = bottom_group_gap * 0.5F;
    return {left_boundary + inset, bottom_group_top, right_boundary - left_boundary - bottom_group_gap, bottom_group_height};
}

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
console_rect console_layout::tab_bar() noexcept { return menu_box; }
console_rect console_layout::tab_rect(console_tab tab) noexcept {
    constexpr float tab_width = 132.0F;
    constexpr float tab_gap = 3.0F;
    const auto index = static_cast<std::size_t>(tab);
    return {menu_box.x + 10.0F + static_cast<float>(index) * (tab_width + tab_gap), menu_box.y + 3.0F, tab_width, menu_box.height - 3.0F};
}
console_rect console_layout::page_rect() noexcept { return page_box; }
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

float console_layout::graph_x_for_frequency(float frequency_hz) noexcept {
    const auto frequency = std::clamp(frequency_hz, graph_frequency_min_hz, graph_frequency_max_hz);
    if (frequency <= graph_first_band_hz) {
        const auto fraction = logarithmic_fraction(frequency, graph_frequency_min_hz, graph_first_band_hz);
        return std::lerp(graph_area.x, fader_first_center, fraction);
    }
    if (frequency >= graph_last_band_hz) {
        const auto fraction = logarithmic_fraction(frequency, graph_last_band_hz, graph_frequency_max_hz);
        return std::lerp(fader_last_center, graph_area.right(), fraction);
    }
    const auto fraction = logarithmic_fraction(frequency, graph_first_band_hz, graph_last_band_hz);
    return std::lerp(fader_first_center, fader_last_center, fraction);
}

console_rect console_layout::group_rect(console_group group) noexcept {
    switch (group) {
        case console_group::left_bay: return left_bay_box;
        case console_group::equalizer: return equalizer_box;
        case console_group::blender: return blender_box;
        case console_group::digital_volume: return digital_volume_box;
        // Bottom boundaries fall midway between fader columns. This keeps the
        // group frames on the same shared vertical grid as the EQ itself.
        case console_group::profiles: return bottom_group_rect(2, 6);
        case console_group::presets: return bottom_group_rect(7, 11);
        case console_group::smoothing: return {};
        case console_group::termite_control: return bottom_group_rect(12, 17);
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
    const auto center = fader_center(index);
    return {center - fader_arrow_width * 0.5F, fader_up_y, fader_arrow_width, fader_arrow_height};
}

console_rect console_layout::fader_track(std::size_t index) noexcept {
    const auto center = fader_center(index);
    return {center - fader_track_width * 0.5F, fader_track_y, fader_track_width, fader_track_height};
}

console_rect console_layout::fader_down(std::size_t index) noexcept {
    const auto track = fader_track(index);
    return {track.x - (fader_arrow_width - fader_track_width) * 0.5F, fader_down_y, fader_arrow_width, fader_arrow_height};
}

console_rect console_layout::fader_value(std::size_t index) noexcept {
    const auto center = fader_center(index);
    return {center - fader_value_width * 0.5F, fader_value_y, fader_value_width, fader_value_height};
}

console_rect console_layout::fader_bank() noexcept {
    const auto first = fader_track(0);
    const auto last = fader_track(console_fader_count - 1);
    return {first.x, fader_track_y, last.right() - first.x, fader_track_height};
}

console_rect console_layout::fader_frequency_label(std::size_t index) noexcept {
    constexpr float width = 43.0F;
    const auto center = fader_center(index);
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

console_rect console_layout::effects_card(std::size_t index) noexcept {
    if (index >= 5) return {};
    constexpr float gap = 14.0F;
    const auto column = index % 3U;
    const auto row = index / 3U;
    const auto total = effects_card_width * 3.0F + gap * 2.0F;
    const auto x = page_box.x + (page_box.width - total) * 0.5F + static_cast<float>(column) * (effects_card_width + gap);
    return {x, page_box.y + 20.0F + static_cast<float>(row) * (effects_card_height + gap), effects_card_width, effects_card_height};
}

console_rect console_layout::apps_list_frame() noexcept {
    return {page_box.x + 28.0F, page_box.y + 24.0F, page_box.width - 56.0F, 355.0F};
}

console_rect console_layout::apps_row(std::size_t index) noexcept {
    const auto frame = apps_list_frame();
    constexpr float top = 31.0F;
    constexpr float height = 34.0F;
    if (index >= 9) return {};
    return {frame.x + 7.0F, frame.y + top + static_cast<float>(index) * height, frame.width - 14.0F, height - 3.0F};
}

console_rect console_layout::monitor_spectrum_frame(bool output) noexcept {
    const auto width = (page_box.width - 74.0F) * 0.5F;
    return {page_box.x + 24.0F + (output ? width + 26.0F : 0.0F), page_box.y + 26.0F, width, 220.0F};
}

console_rect console_layout::monitor_meter_frame() noexcept {
    return {page_box.x + 24.0F, page_box.y + 267.0F, page_box.width - 48.0F, 145.0F};
}

console_rect console_layout::smoothing_value() noexcept {
    constexpr float button_width = 16.0F;
    constexpr float display_width = 108.0F;
    constexpr float gap = 7.0F;
    const float content_width = button_width * 2.0F + display_width + gap * 2.0F;
    const auto smoothing = group_rect(console_group::smoothing);
    const float x = smoothing.x + (smoothing.width - content_width) * 0.5F;
    return {x + button_width + gap, smoothing.y + 48.0F, display_width, 25.0F};
}

console_rect console_layout::preset_dropdown_frame() noexcept {
    constexpr float padding = 2.0F;
    constexpr float row_height = 22.0F;
    constexpr float field_gap = 2.0F;
    constexpr std::size_t rows_per_column = 5;
    constexpr float column_gap = 2.0F;
    const auto field = control_rect(console_control::preset_cycle);
    const auto columns = std::max<std::size_t>(1, (console_state::preset_count() + rows_per_column - 1) / rows_per_column);
    const float item_width = field.width - padding * 2.0F;
    const float width = padding * 2.0F + item_width * static_cast<float>(columns) + column_gap * static_cast<float>(columns - 1);
    const float height = padding * 2.0F + row_height * static_cast<float>(std::min(rows_per_column, console_state::preset_count()));
    return {field.x + (field.width - width) * 0.5F, field.y - field_gap - height, width, height};
}

console_rect console_layout::preset_dropdown_item(std::size_t index) noexcept {
    constexpr float padding = 2.0F;
    constexpr float row_height = 22.0F;
    constexpr std::size_t rows_per_column = 5;
    constexpr float column_gap = 2.0F;
    if (index >= console_state::preset_count()) return {};
    const auto frame = preset_dropdown_frame();
    const auto field = control_rect(console_control::preset_cycle);
    const float item_width = field.width - padding * 2.0F;
    const auto column = index / rows_per_column;
    const auto row = index % rows_per_column;
    return {frame.x + padding + static_cast<float>(column) * (item_width + column_gap),
            frame.y + padding + static_cast<float>(row) * row_height,
            item_width,
            row_height - 1.0F};
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
    return {refresh.right() + 6.0F, refresh.y, 110.0F, refresh.height};
}

console_rect console_layout::routing_picker_mixer_button(std::size_t candidate_count) noexcept {
    const auto route = routing_picker_open_button(candidate_count);
    return {route.right() + 6.0F, route.y, 126.0F, route.height};
}

console_rect console_layout::routing_picker_close_button(std::size_t candidate_count) noexcept {
    const auto frame = routing_picker_frame(candidate_count);
    return {frame.right() - 68.0F, frame.bottom() - 29.0F, 60.0F, 23.0F};
}

console_rect console_layout::hardware_diagnostics_frame() noexcept {
    return {graph_box.x + 72.0F, graph_box.y + 23.0F, graph_box.width - 144.0F, 168.0F};
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
        case console_control::tab_graphic_eq: return tab_rect(console_tab::graphic_eq);
        case console_control::tab_effects_rack: return tab_rect(console_tab::effects_rack);
        case console_control::tab_apps: return tab_rect(console_tab::apps);
        case console_control::tab_monitor: return tab_rect(console_tab::monitor);
        case console_control::file_menu: return {10.0F, 27.0F, 28.0F, 19.0F};
        case console_control::hardware_menu: return {42.0F, 27.0F, 60.0F, 19.0F};
        case console_control::themes_menu: return {106.0F, 27.0F, 44.0F, 19.0F};
        case console_control::help_menu: return {154.0F, 27.0F, 29.0F, 19.0F};
        case console_control::detect: return left_buttons[0];
        case console_control::status_sync: return left_buttons[1];
        case console_control::clear_info: return left_buttons[2];
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
            return {digital_volume_box.x + inset, digital_volume_box.y + 63.0F, digital_volume_box.width - inset * 2.0F, 48.0F};
        }
        case console_control::volume_down: {
            constexpr float inset = 7.0F;
            return {digital_volume_box.x + inset, digital_volume_box.y + 118.0F, digital_volume_box.width - inset * 2.0F, 48.0F};
        }
        case console_control::profile_open: {
            constexpr float button_width = 96.0F;
            const auto profiles = group_rect(console_group::profiles);
            return {profiles.x + (profiles.width - button_width) * 0.5F, profiles.y + 16.0F, button_width, 25.0F};
        }
        case console_control::profile_save: {
            constexpr float button_width = 96.0F;
            const auto profiles = group_rect(console_group::profiles);
            return {profiles.x + (profiles.width - button_width) * 0.5F, profiles.y + 48.0F, button_width, 25.0F};
        }
        case console_control::preset_zero: {
            constexpr float button_width = 96.0F;
            const auto presets = group_rect(console_group::presets);
            return {presets.x + (presets.width - button_width) * 0.5F, presets.y + 16.0F, button_width, 25.0F};
        }
        case console_control::preset_cycle: {
            constexpr float button_width = 96.0F;
            const auto presets = group_rect(console_group::presets);
            return {presets.x + (presets.width - button_width) * 0.5F, presets.y + 48.0F, button_width, 25.0F};
        }
        case console_control::smoothing_reset: {
            constexpr float button_width = 70.0F;
            const auto smoothing = group_rect(console_group::smoothing);
            return {smoothing.x + (smoothing.width - button_width) * 0.5F, smoothing.y + 16.0F, button_width, 25.0F};
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
            const auto termite_control = group_rect(console_group::termite_control);
            return {termite_control.x + inset, termite_control.y + 28.0F, termite_control.width - inset * 2.0F, 25.0F};
        }
        case console_control::export_response: {
            constexpr float inset = 12.0F;
            constexpr float gap = 6.0F;
            const auto termite_control = group_rect(console_group::termite_control);
            const float width = (termite_control.width - inset * 2.0F - gap) * 0.5F;
            return {termite_control.x + inset + width + gap, termite_control.y + 16.0F, width, 25.0F};
        }
        case console_control::grid: {
            constexpr float utility_gap = 8.0F;
            const auto termite_control = group_rect(console_group::termite_control);
            return {termite_control.right() + utility_gap, termite_control.y + 28.0F, 70.0F, 25.0F};
        }
        case console_control::help_button: {
            constexpr float utility_gap = 8.0F;
            const auto termite_control = group_rect(console_group::termite_control);
            return {termite_control.right() + utility_gap, termite_control.y + 48.0F, 70.0F, 25.0F};
        }
        case console_control::effect_bass_toggle: { const auto card = effects_card(0); return {card.x + 14.0F, card.y + 39.0F, 82.0F, 25.0F}; }
        case console_control::effect_bass_down: { const auto card = effects_card(0); return {card.x + 111.0F, card.y + 39.0F, 25.0F, 25.0F}; }
        case console_control::effect_bass_up: { const auto card = effects_card(0); return {card.x + 212.0F, card.y + 39.0F, 25.0F, 25.0F}; }
        case console_control::effect_loudness_toggle: { const auto card = effects_card(1); return {card.x + 14.0F, card.y + 39.0F, 82.0F, 25.0F}; }
        case console_control::effect_loudness_down: { const auto card = effects_card(1); return {card.x + 111.0F, card.y + 39.0F, 25.0F, 25.0F}; }
        case console_control::effect_loudness_up: { const auto card = effects_card(1); return {card.x + 212.0F, card.y + 39.0F, 25.0F, 25.0F}; }
        case console_control::effect_clarity_toggle: { const auto card = effects_card(2); return {card.x + 14.0F, card.y + 39.0F, 82.0F, 25.0F}; }
        case console_control::effect_clarity_down: { const auto card = effects_card(2); return {card.x + 111.0F, card.y + 39.0F, 25.0F, 25.0F}; }
        case console_control::effect_clarity_up: { const auto card = effects_card(2); return {card.x + 212.0F, card.y + 39.0F, 25.0F, 25.0F}; }
        case console_control::effect_stereo_toggle: { const auto card = effects_card(3); return {card.x + 14.0F, card.y + 39.0F, 82.0F, 25.0F}; }
        case console_control::effect_width_down: { const auto card = effects_card(3); return {card.x + 111.0F, card.y + 39.0F, 25.0F, 25.0F}; }
        case console_control::effect_width_up: { const auto card = effects_card(3); return {card.x + 212.0F, card.y + 39.0F, 25.0F, 25.0F}; }
        case console_control::effect_mono: { const auto card = effects_card(3); return {card.x + 88.0F, card.y + 77.0F, 84.0F, 24.0F}; }
        case console_control::effect_balance_left: { const auto card = effects_card(4); return {card.x + 42.0F, card.y + 48.0F, 40.0F, 28.0F}; }
        case console_control::effect_balance_right: { const auto card = effects_card(4); return {card.right() - 82.0F, card.y + 48.0F, 40.0F, 28.0F}; }
        case console_control::effect_reset: { const auto card = effects_card(4); return {card.x + 75.0F, card.y + 83.0F, 110.0F, 24.0F}; }
        case console_control::apps_refresh: { const auto list = apps_list_frame(); return {list.x + 8.0F, list.bottom() + 14.0F, 94.0F, 28.0F}; }
        case console_control::apps_route_selected: { const auto list = apps_list_frame(); return {list.x + 110.0F, list.bottom() + 14.0F, 122.0F, 28.0F}; }
        case console_control::apps_return_selected: { const auto list = apps_list_frame(); return {list.x + 240.0F, list.bottom() + 14.0F, 130.0F, 28.0F}; }
        case console_control::apps_open_mixer: { const auto list = apps_list_frame(); return {list.right() - 128.0F, list.bottom() + 14.0F, 120.0F, 28.0F}; }
        default: return {};
    }
}

console_hit console_layout::hit_test(console_point point, std::size_t message_count, float scroll_offset, console_tab active_tab) noexcept {
    if (minimize_button().contains(point)) return {console_control::minimize};
    if (close_button().contains(point)) return {console_control::close};
    constexpr std::array tabs{console_control::tab_graphic_eq, console_control::tab_effects_rack, console_control::tab_apps, console_control::tab_monitor};
    for (const auto tab : tabs) if (control_rect(tab).contains(point)) return {tab};
    if (status_scroll_thumb(message_count, scroll_offset).contains(point)) return {console_control::scroll_thumb};

    if (active_tab == console_tab::graphic_eq) {
        for (std::size_t index = 0; index < console_fader_count; ++index) {
            if (fader_up(index).contains(point)) return {console_control::fader_up, static_cast<int>(index)};
            if (fader_track(index).contains(point)) return {console_control::fader_track, static_cast<int>(index)};
            if (fader_down(index).contains(point)) return {console_control::fader_down, static_cast<int>(index)};
            if (fader_value(index).contains(point)) return {console_control::fader_value, static_cast<int>(index)};
        }
    } else if (active_tab == console_tab::effects_rack) {
        constexpr std::array effects{console_control::effect_bass_toggle, console_control::effect_bass_down, console_control::effect_bass_up,
            console_control::effect_loudness_toggle, console_control::effect_loudness_down, console_control::effect_loudness_up,
            console_control::effect_clarity_toggle, console_control::effect_clarity_down, console_control::effect_clarity_up,
            console_control::effect_stereo_toggle, console_control::effect_width_down, console_control::effect_width_up,
            console_control::effect_mono, console_control::effect_balance_left, console_control::effect_balance_right, console_control::effect_reset};
        for (const auto control : effects) if (control_rect(control).contains(point)) return {control};
    } else if (active_tab == console_tab::apps) {
        for (std::size_t index = 0; index < 9; ++index) if (apps_row(index).contains(point)) return {console_control::apps_row, static_cast<int>(index)};
        constexpr std::array apps{console_control::apps_refresh, console_control::apps_route_selected, console_control::apps_return_selected, console_control::apps_open_mixer};
        for (const auto control : apps) if (control_rect(control).contains(point)) return {control};
    }

    for (std::size_t index = 0; index < left_buttons.size(); ++index) {
        if (left_buttons[index].contains(point)) return {left_button_controls[index]};
    }

    constexpr std::array controls{
        console_control::equalizer_off, console_control::equalizer_on,
        console_control::volume_up, console_control::volume_down, console_control::profile_open, console_control::profile_save,
        console_control::preset_zero, console_control::preset_cycle, console_control::route_apps, console_control::grid,
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
    const auto scale = std::clamp(preferred_scale, std::min(0.1F, fit_scale), fit_scale);
    return {console_design_width * scale, console_design_height * scale};
}

}  // namespace termite
