#pragma once

#include "app/console_layout.h"
#include "app/console_skin.h"
#include "app/console_state.h"
#include "audio/session_router.h"
#include "audio/wasapi_audio_engine.h"

#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <memory>
#include <vector>

namespace termite {

class console_window {
public:
    explicit console_window(HINSTANCE instance);
    ~console_window();

    console_window(const console_window&) = delete;
    console_window& operator=(const console_window&) = delete;

    int run();

private:
    static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    static LRESULT CALLBACK fader_edit_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

    [[nodiscard]] bool create_window();
    [[nodiscard]] LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam);
    [[nodiscard]] LRESULT hit_test_screen(POINT screen_point) const;
    void ensure_render_target();
    void discard_render_target();
    void render();
    void draw_console();
    void draw_title_and_menu();
    void draw_left_bay();
    void draw_graph();
    void draw_faders();
    void draw_bottom_controls();
    void draw_preset_dropdown();
    void draw_fader_filter_menu();
    void draw_routing_picker();
    void update_pointer(POINT client_point);
    void execute_control(console_hit hit);
    void update_fader_from_point(int index, console_point point);
    void begin_fader_edit(int index);
    void finish_fader_edit(bool commit);
    void position_fader_edit();
    void update_scroll_from_point(console_point point);
    void show_fader_filter_menu(int band, console_point anchor);
    void execute_fader_filter_menu_row(int row);
    void sync_profile();
    void show_routing_picker();
    void refresh_routing_picker();
    void open_selected_routing_settings();
    void append_audio_status();

    [[nodiscard]] console_point to_design(POINT client_point) const noexcept;
    [[nodiscard]] console_point client_to_design_screen(POINT screen_point) const noexcept;
    [[nodiscard]] float scale() const noexcept;
    [[nodiscard]] POINT canvas_origin() const noexcept;
    [[nodiscard]] bool is_hot(console_hit hit) const noexcept;
    [[nodiscard]] bool is_pressed(console_hit hit) const noexcept;
    [[nodiscard]] int fader_filter_menu_row(console_point point) const noexcept;
    [[nodiscard]] int preset_dropdown_row(console_point point) const noexcept;
    [[nodiscard]] int routing_picker_row(console_point point) const noexcept;

    HINSTANCE instance_{};
    HWND window_{};
    UINT dpi_{96};
    HRESULT com_initialization_{E_FAIL};
    Microsoft::WRL::ComPtr<ID2D1Factory> d2d_factory_;
    Microsoft::WRL::ComPtr<IDWriteFactory> write_factory_;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> render_target_;
    std::unique_ptr<console_skin> skin_;
    console_state state_;
    wasapi_audio_engine audio_engine_;
    session_router session_router_;
    console_hit hot_hit_{};
    console_hit pressed_hit_{};
    int active_fader_{-1};
    HWND fader_edit_{};
    WNDPROC fader_edit_original_proc_{};
    int editing_fader_{-1};
    bool preset_dropdown_open_{};
    int preset_dropdown_hot_row_{-1};
    int preset_dropdown_pressed_row_{-1};
    int filter_menu_band_{-1};
    int filter_menu_hot_row_{-1};
    int filter_menu_pressed_row_{-1};
    console_rect filter_menu_rect_{};
    std::vector<routing_candidate> routing_candidates_;
    std::vector<bool> routing_selected_;
    std::size_t routing_picker_first_item_{};
    int routing_picker_hot_row_{-1};
    int routing_picker_pressed_row_{-1};
    bool routing_picker_open_{};
    bool dragging_scroll_{};
    float scroll_drag_offset_{};
    bool tracking_mouse_{};
    std::string last_audio_status_;
};

}  // namespace termite
