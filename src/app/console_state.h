#pragma once

#include "app/console_layout.h"
#include "dsp/eq_profile.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace termite {

struct console_action_result {
    bool profile_changed{};
    bool restart_audio{};
    bool open_routing{};
    bool minimize{};
    bool close{};
    bool request_engine_status{};
    bool open_diagnostics{};
};

struct console_persistent_state {
    eq_profile profile{eq_profile::flat()};
    int smoothing_amount{50};
    int dry_mix{100};
    int wet_mix{};
    int preset_index{-1};
    bool paused{};
    bool sleeping{};
    bool default_start_saved{true};
    bool grid_visible{true};
    std::size_t background_index{};
};

class console_state {
public:
    console_state();

    [[nodiscard]] const eq_profile& profile() const noexcept;
    [[nodiscard]] const std::vector<std::wstring>& notices() const noexcept;
    [[nodiscard]] float scroll_offset() const noexcept;
    [[nodiscard]] int smoothing_amount() const noexcept;
    [[nodiscard]] int dry_mix() const noexcept;
    [[nodiscard]] int wet_mix() const noexcept;
    [[nodiscard]] bool grid_visible() const noexcept;
    [[nodiscard]] std::size_t background_index() const noexcept;
    [[nodiscard]] bool is_selected(console_control control) const noexcept;
    [[nodiscard]] std::wstring preset_label() const;
    [[nodiscard]] static std::size_t preset_count() noexcept;
    [[nodiscard]] static std::wstring_view preset_name(std::size_t index) noexcept;
    [[nodiscard]] int selected_preset_index() const noexcept;
    [[nodiscard]] console_persistent_state persistent_state() const;

    console_action_result activate(console_control control);
    bool set_fader_gain(std::size_t index, float gain_db);
    bool adjust_fader_gain(std::size_t index, float delta_db);
    bool adjust_fader_q(std::size_t index, float delta_q);
    bool set_fader_shape(std::size_t index, filter_shape shape);
    bool set_fader_enabled(std::size_t index, bool enabled);
    bool apply_preset(std::size_t index);
    void restore_persistent_state(const console_persistent_state& settings);
    void set_scroll_offset(float offset) noexcept;
    void append_engine_status(const std::string& status);

private:
    void append_notice(std::wstring message);
    void reset_profile();
    void apply_next_preset();

    eq_profile profile_{eq_profile::flat()};
    std::vector<std::wstring> notices_;
    float scroll_offset_{};
    int smoothing_amount_{50};
    int dry_mix_{100};
    int wet_mix_{};
    int preset_index_{-1};
    bool paused_{};
    bool sleeping_{};
    bool default_start_saved_{true};
    bool grid_visible_{true};
    std::size_t background_index_{};
};

}  // namespace termite
