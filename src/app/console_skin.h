#pragma once

#include "app/console_layout.h"

#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <string_view>

namespace termite {

enum class console_visual_state {
    normal,
    hot,
    pressed,
    selected,
};

enum class console_text_style {
    label,
    button,
    numeric,
    title,
    menu,
};

class console_skin {
public:
    console_skin(IDWriteFactory* write_factory, ID2D1Factory* d2d_factory);

    void set_target(ID2D1RenderTarget* target) noexcept;
    void draw_background(console_rect rect, std::size_t background_index) const;
    void draw_checker(console_rect rect, bool carbon = false) const;
    void draw_title_bar(console_rect rect) const;
    void draw_panel(console_rect rect, bool raised = false) const;
    void draw_graph_surface(console_rect rect) const;
    void draw_group(console_rect rect) const;
    void draw_button(console_rect rect, std::wstring_view label, console_visual_state state) const;
    void draw_caption_button(console_rect rect, std::wstring_view label, console_visual_state state) const;
    void draw_radio(console_point center, bool selected, std::wstring_view label) const;
    void draw_equalizer_toggle(console_rect rect, bool enabled) const;
    void draw_checkbox(console_rect rect, bool checked, std::wstring_view label) const;
    void draw_scrollbar(console_rect track, console_rect thumb, bool hot, bool pressed) const;
    void draw_fader(console_rect up_button,
                    console_rect track,
                    console_rect down_button,
                    float gain_db,
                    console_visual_state state) const;
    void draw_text(std::wstring_view text,
                   console_rect rect,
                   console_text_style style,
                   DWRITE_TEXT_ALIGNMENT alignment = DWRITE_TEXT_ALIGNMENT_LEADING,
                   DWRITE_PARAGRAPH_ALIGNMENT vertical = DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
                   D2D1_COLOR_F color = D2D1::ColorF(0.9F, 0.9F, 0.9F)) const;
    void draw_rotated_text(std::wstring_view text, console_rect rect, float degrees, console_point pivot, console_text_style style) const;
    void draw_display_number(std::wstring_view text, console_rect rect, D2D1_COLOR_F color = D2D1::ColorF(0.95F, 0.94F, 0.78F)) const;
    [[nodiscard]] ID2D1SolidColorBrush* brush(D2D1_COLOR_F color) const;

private:
    [[nodiscard]] D2D1_RECT_F rect(console_rect value) const noexcept;
    [[nodiscard]] IDWriteTextFormat* text_format(console_text_style style) const noexcept;
    [[nodiscard]] ID2D1BitmapBrush* background_brush(std::size_t background_index) const;

    ID2D1RenderTarget* target_{};
    IDWriteFactory* write_factory_{};
    ID2D1Factory* d2d_factory_{};
    Microsoft::WRL::ComPtr<IWICImagingFactory> wic_factory_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> label_format_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> button_format_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> numeric_format_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> title_format_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> menu_format_;
    mutable std::array<Microsoft::WRL::ComPtr<ID2D1BitmapBrush>, 6> background_brushes_;
    mutable std::unordered_map<std::uint32_t, Microsoft::WRL::ComPtr<ID2D1SolidColorBrush>> brush_cache_;
};

}  // namespace termite
