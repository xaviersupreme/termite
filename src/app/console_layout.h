#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

namespace termite {

inline constexpr float console_design_width = 1245.0F;
inline constexpr float console_design_height = 700.0F;
inline constexpr std::size_t console_fader_count = 20;

struct console_point {
    float x{};
    float y{};
};

struct console_rect {
    float x{};
    float y{};
    float width{};
    float height{};

    [[nodiscard]] bool contains(console_point point) const noexcept;
    [[nodiscard]] float right() const noexcept;
    [[nodiscard]] float bottom() const noexcept;
};

enum class console_control {
    none,
    title_drag,
    minimize,
    close,
    file_menu,
    hardware_menu,
    themes_menu,
    help_menu,
    detect,
    reset,
    status_sync,
    pause,
    run,
    clear_info,
    sleep,
    default_start,
    equalizer_off,
    equalizer_on,
    blender_increase,
    blender_decrease,
    volume_up,
    volume_down,
    fader_up,
    fader_track,
    fader_down,
    fader_value,
    profile_open,
    profile_save,
    preset_zero,
    preset_cycle,
    smoothing_reset,
    smoothing_decrease,
    smoothing_increase,
    route_apps,
    export_response,
    grid,
    help_button,
    scroll_thumb,
};

enum class console_group {
    left_bay,
    equalizer,
    blender,
    digital_volume,
    profiles,
    presets,
    smoothing,
    termite_control,
};

struct console_hit {
    console_control control{console_control::none};
    int index{-1};

    [[nodiscard]] bool operator==(const console_hit& other) const noexcept = default;
};

struct console_size {
    float width{};
    float height{};
};

struct console_display_layout {
    float origin_x{};
    float content_width{};
    float digit_width{};
};

class console_layout {
public:
    [[nodiscard]] static console_rect title_bar() noexcept;
    [[nodiscard]] static console_rect title_icon() noexcept;
    [[nodiscard]] static console_rect title_label() noexcept;
    [[nodiscard]] static console_rect menu_bar() noexcept;
    [[nodiscard]] static console_rect minimize_button() noexcept;
    [[nodiscard]] static console_rect close_button() noexcept;
    [[nodiscard]] static console_rect status_viewport() noexcept;
    [[nodiscard]] static console_rect status_scroll_track() noexcept;
    [[nodiscard]] static console_rect status_scroll_thumb(std::size_t message_count, float scroll_offset) noexcept;
    [[nodiscard]] static console_rect notification_label() noexcept;
    [[nodiscard]] static console_rect graph_frame() noexcept;
    [[nodiscard]] static console_rect graph_plot() noexcept;
    [[nodiscard]] static console_rect graph_y_axis() noexcept;
    [[nodiscard]] static console_rect graph_x_axis() noexcept;
    [[nodiscard]] static console_rect graph_title() noexcept;
    [[nodiscard]] static console_rect graph_gain_label() noexcept;
    [[nodiscard]] static console_rect graph_frequency_label() noexcept;
    [[nodiscard]] static console_rect graph_db_label(std::size_t index) noexcept;
    [[nodiscard]] static console_rect group_rect(console_group group) noexcept;
    [[nodiscard]] static console_rect equalizer_label() noexcept;
    [[nodiscard]] static console_rect equalizer_toggle() noexcept;
    [[nodiscard]] static console_rect blender_label() noexcept;
    [[nodiscard]] static console_rect blender_dry_label() noexcept;
    [[nodiscard]] static console_rect blender_wet_label() noexcept;
    [[nodiscard]] static console_rect blender_dry_display() noexcept;
    [[nodiscard]] static console_rect blender_wet_display() noexcept;
    [[nodiscard]] static console_rect digital_volume_label() noexcept;
    [[nodiscard]] static console_rect digital_volume_display() noexcept;
    [[nodiscard]] static console_rect fader_up(std::size_t index) noexcept;
    [[nodiscard]] static console_rect fader_track(std::size_t index) noexcept;
    [[nodiscard]] static console_rect fader_down(std::size_t index) noexcept;
    [[nodiscard]] static console_rect fader_value(std::size_t index) noexcept;
    [[nodiscard]] static console_rect fader_bank() noexcept;
    [[nodiscard]] static console_rect fader_frequency_label(std::size_t index) noexcept;
    [[nodiscard]] static console_rect smoothing_value() noexcept;
    [[nodiscard]] static console_rect preset_dropdown_frame() noexcept;
    [[nodiscard]] static console_rect preset_dropdown_item(std::size_t index) noexcept;
    [[nodiscard]] static std::size_t routing_picker_visible_rows(std::size_t candidate_count) noexcept;
    [[nodiscard]] static console_rect routing_picker_frame(std::size_t candidate_count) noexcept;
    [[nodiscard]] static console_rect routing_picker_row(std::size_t candidate_count, std::size_t visible_index) noexcept;
    [[nodiscard]] static console_rect routing_picker_refresh_button(std::size_t candidate_count) noexcept;
    [[nodiscard]] static console_rect routing_picker_open_button(std::size_t candidate_count) noexcept;
    [[nodiscard]] static console_rect routing_picker_close_button(std::size_t candidate_count) noexcept;
    [[nodiscard]] static float snap_fader_gain(float gain_db) noexcept;
    [[nodiscard]] static float display_glyph_units(wchar_t character) noexcept;
    [[nodiscard]] static console_display_layout measure_display_layout(std::wstring_view text, console_rect rect) noexcept;
    [[nodiscard]] static console_rect control_rect(console_control control) noexcept;
    [[nodiscard]] static console_hit hit_test(console_point point, std::size_t message_count, float scroll_offset) noexcept;
    [[nodiscard]] static console_size constrain_aspect_ratio(console_size requested, float minimum_scale = 0.75F) noexcept;
    [[nodiscard]] static console_size fit_canvas_to_bounds(console_size available, float preferred_scale = 1.0F) noexcept;
};

}  // namespace termite
