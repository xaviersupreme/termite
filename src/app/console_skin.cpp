#include "app/console_skin.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>

namespace termite {
namespace {

const auto black = D2D1::ColorF(0.016F, 0.018F, 0.020F);
const auto near_black = D2D1::ColorF(0.050F, 0.053F, 0.057F);
const auto panel_dark = D2D1::ColorF(0.083F, 0.087F, 0.092F);
const auto panel_mid = D2D1::ColorF(0.145F, 0.150F, 0.155F);
const auto chrome = D2D1::ColorF(0.48F, 0.50F, 0.51F);
const auto silver = D2D1::ColorF(0.82F, 0.84F, 0.85F);

constexpr std::array<std::wstring_view, 6> background_asset_names{
    L"real-carbon-fibre.png",
    L"crossword.png",
    L"bright-squares.png",
    L"binding-light.png",
    L"billie-holiday.png",
    L"45-degree-fabric-light.png",
};

float clamp_gain(float gain) {
    return std::clamp(gain, -20.0F, 20.0F);
}

struct checker_pattern_cache {
    ID2D1RenderTarget* target{};
    Microsoft::WRL::ComPtr<ID2D1BitmapBrush> standard;
    Microsoft::WRL::ComPtr<ID2D1BitmapBrush> carbon;
};

checker_pattern_cache& checker_patterns() {
    static checker_pattern_cache cache;
    return cache;
}

void reset_checker_patterns(ID2D1RenderTarget* target) {
    auto& cache = checker_patterns();
    if (cache.target != target) {
        cache.target = target;
        cache.standard.Reset();
        cache.carbon.Reset();
    }
}

ID2D1BitmapBrush* checker_pattern(ID2D1RenderTarget* target, bool carbon) {
    if (target == nullptr) {
        return nullptr;
    }

    reset_checker_patterns(target);
    auto& cache = checker_patterns();
    auto& result = carbon ? cache.carbon : cache.standard;
    if (result != nullptr) {
        return result.Get();
    }

    const std::uint32_t base = carbon ? 0xFF121314U : 0xFF171819U;
    const std::uint32_t shadow = carbon ? 0xFF0D0E0FU : 0xFF141516U;
    const std::uint32_t thread = carbon ? 0xFF1C1D1EU : 0xFF222324U;
    std::array<std::uint32_t, 64> pixels{};
    for (std::size_t y = 0; y < 8; ++y) {
        for (std::size_t x = 0; x < 8; ++x) {
            const auto forward = (x + 8U - y) % 8U;
            const auto backward = (x + y) % 8U;
            pixels[y * 8U + x] = (forward == 0U || backward == 0U) ? thread :
                                (forward == 4U || backward == 4U) ? shadow : base;
        }
    }

    Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
    const auto properties = D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    if (FAILED(target->CreateBitmap(D2D1::SizeU(8, 8), pixels.data(), 8U * sizeof(std::uint32_t), properties, &bitmap))) {
        return nullptr;
    }
    const auto brush_properties = D2D1::BitmapBrushProperties(D2D1_EXTEND_MODE_WRAP, D2D1_EXTEND_MODE_WRAP, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
    if (FAILED(target->CreateBitmapBrush(bitmap.Get(), brush_properties, &result))) {
        return nullptr;
    }
    return result.Get();
}

std::filesystem::path asset_path(std::wstring_view filename) {
    std::array<wchar_t, MAX_PATH> executable_path{};
    const auto length = GetModuleFileNameW(nullptr, executable_path.data(), static_cast<DWORD>(executable_path.size()));
    if (length == 0 || length >= executable_path.size()) {
        return {};
    }
    return std::filesystem::path(executable_path.data()).parent_path() / L"assets" / std::filesystem::path{std::wstring{filename}};
}

}  // namespace

console_skin::console_skin(IDWriteFactory* write_factory, ID2D1Factory* d2d_factory)
    : write_factory_(write_factory), d2d_factory_(d2d_factory) {
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(wic_factory_.GetAddressOf()));
    if (write_factory_ == nullptr) {
        return;
    }

    const auto make_format = [this](const wchar_t* face,
                                    float size,
                                    DWRITE_FONT_WEIGHT weight,
                                    Microsoft::WRL::ComPtr<IDWriteTextFormat>& output) {
        write_factory_->CreateTextFormat(face,
                                         nullptr,
                                         weight,
                                         DWRITE_FONT_STYLE_NORMAL,
                                         DWRITE_FONT_STRETCH_NORMAL,
                                         size,
                                         L"en-us",
                                         output.GetAddressOf());
    };

    make_format(L"Tahoma", 9.0F, DWRITE_FONT_WEIGHT_NORMAL, label_format_);
    make_format(L"Tahoma", 10.0F, DWRITE_FONT_WEIGHT_NORMAL, button_format_);
    make_format(L"Consolas", 12.0F, DWRITE_FONT_WEIGHT_NORMAL, numeric_format_);
    make_format(L"Tahoma", 12.0F, DWRITE_FONT_WEIGHT_BOLD, title_format_);
    make_format(L"Tahoma", 12.0F, DWRITE_FONT_WEIGHT_NORMAL, menu_format_);
}

void console_skin::set_target(ID2D1RenderTarget* target) noexcept {
    if (target_ != target) {
        brush_cache_.clear();
        reset_checker_patterns(target);
        for (auto& background : background_brushes_) {
            background.Reset();
        }
    }
    target_ = target;
}

void console_skin::draw_background(console_rect bounds, std::size_t background_index) const {
    if (target_ == nullptr) {
        return;
    }

    const auto frame = rect(bounds);
    target_->FillRectangle(frame, brush(D2D1::ColorF(0.022F, 0.024F, 0.027F)));
    (void)background_index;
    if (auto* background = checker_pattern(target_, true); background != nullptr) {
        background->SetTransform(D2D1::Matrix3x2F::Identity());
        target_->FillRectangle(frame, background);
    } else {
        draw_checker(bounds, true);
    }
}

void console_skin::draw_checker(console_rect bounds, bool carbon) const {
    if (target_ == nullptr) {
        return;
    }

    if (auto* pattern = checker_pattern(target_, carbon); pattern != nullptr) {
        target_->FillRectangle(rect(bounds), pattern);
    } else {
        target_->FillRectangle(rect(bounds), brush(carbon ? D2D1::ColorF(0.065F, 0.068F, 0.072F) : panel_dark));
    }
}

void console_skin::draw_title_bar(console_rect bounds) const {
    if (target_ == nullptr) {
        return;
    }

    const auto frame = rect(bounds);
    target_->FillRectangle(frame, brush(D2D1::ColorF(0.030F, 0.033F, 0.037F)));
    const auto inner = D2D1::RectF(frame.left + 1.0F, frame.top + 1.0F, frame.right - 1.0F, frame.bottom - 1.0F);
    const D2D1_GRADIENT_STOP stops[]{
        {0.0F, D2D1::ColorF(0.25F, 0.26F, 0.27F, 0.52F)},
        {0.18F, D2D1::ColorF(0.16F, 0.17F, 0.18F, 0.52F)},
        {0.78F, D2D1::ColorF(0.085F, 0.090F, 0.096F, 0.56F)},
        {1.0F, D2D1::ColorF(0.025F, 0.028F, 0.032F, 0.60F)},
    };
    Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> collection;
    Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> gradient;
    if (SUCCEEDED(target_->CreateGradientStopCollection(stops, std::size(stops), &collection)) &&
        SUCCEEDED(target_->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(D2D1::Point2F(inner.left, inner.top), D2D1::Point2F(inner.left, inner.bottom)), collection.Get(), &gradient))) {
        target_->FillRectangle(inner, gradient.Get());
    }
    target_->DrawLine(D2D1::Point2F(frame.left, frame.top), D2D1::Point2F(frame.right, frame.top), brush(D2D1::ColorF(0.52F, 0.54F, 0.55F, 0.74F)));
    target_->DrawLine(D2D1::Point2F(frame.left, frame.top), D2D1::Point2F(frame.left, frame.bottom), brush(D2D1::ColorF(0.31F, 0.33F, 0.34F, 0.64F)));
    target_->DrawLine(D2D1::Point2F(frame.left, frame.bottom - 1.0F), D2D1::Point2F(frame.right, frame.bottom - 1.0F), brush(black));
    target_->DrawLine(D2D1::Point2F(frame.right - 1.0F, frame.top), D2D1::Point2F(frame.right - 1.0F, frame.bottom), brush(black));
}

void console_skin::draw_panel(console_rect bounds, bool raised) const {
    if (target_ == nullptr) {
        return;
    }

    const auto frame = rect(bounds);
    if (bounds.width <= 2.0F || bounds.height <= 2.0F) return;
    const auto inner = D2D1::RectF(frame.left + 1.0F, frame.top + 1.0F, frame.right - 1.0F, frame.bottom - 1.0F);
    target_->FillRectangle(frame, brush(raised ? D2D1::ColorF(0.14F, 0.15F, 0.16F, 0.90F) : D2D1::ColorF(0.004F, 0.005F, 0.006F, 0.96F)));
    target_->FillRectangle(inner, brush(raised ? D2D1::ColorF(0.10F, 0.11F, 0.12F, 0.82F) : D2D1::ColorF(0.020F, 0.022F, 0.024F, 0.88F)));

    const auto dark_edge = brush(D2D1::ColorF(0.005F, 0.006F, 0.007F, 0.96F));
    const auto light_edge = brush(raised ? D2D1::ColorF(0.32F, 0.34F, 0.35F, 0.72F) : D2D1::ColorF(0.25F, 0.27F, 0.28F, 0.56F));
    if (raised) {
        target_->DrawLine(D2D1::Point2F(frame.left, frame.top), D2D1::Point2F(frame.right - 1.0F, frame.top), light_edge);
        target_->DrawLine(D2D1::Point2F(frame.left, frame.top), D2D1::Point2F(frame.left, frame.bottom - 1.0F), light_edge);
        target_->DrawLine(D2D1::Point2F(frame.left, frame.bottom - 1.0F), D2D1::Point2F(frame.right, frame.bottom - 1.0F), dark_edge);
        target_->DrawLine(D2D1::Point2F(frame.right - 1.0F, frame.top), D2D1::Point2F(frame.right - 1.0F, frame.bottom), dark_edge);
    } else {
        target_->DrawLine(D2D1::Point2F(frame.left, frame.top), D2D1::Point2F(frame.right - 1.0F, frame.top), dark_edge);
        target_->DrawLine(D2D1::Point2F(frame.left, frame.top), D2D1::Point2F(frame.left, frame.bottom - 1.0F), dark_edge);
        target_->DrawLine(D2D1::Point2F(frame.left, frame.bottom - 1.0F), D2D1::Point2F(frame.right, frame.bottom - 1.0F), light_edge);
        target_->DrawLine(D2D1::Point2F(frame.right - 1.0F, frame.top), D2D1::Point2F(frame.right - 1.0F, frame.bottom), light_edge);
    }
}

void console_skin::draw_graph_surface(console_rect bounds) const {
    if (target_ == nullptr) {
        return;
    }
    const auto frame = rect(bounds);
    const D2D1_GRADIENT_STOP stops[]{
        {0.0F, D2D1::ColorF(0.010F, 0.012F, 0.014F, 0.74F)},
        {0.58F, D2D1::ColorF(0.022F, 0.025F, 0.027F, 0.72F)},
        {1.0F, D2D1::ColorF(0.045F, 0.048F, 0.050F, 0.70F)},
    };
    Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> collection;
    Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> gradient;
    if (SUCCEEDED(target_->CreateGradientStopCollection(stops, std::size(stops), &collection)) &&
        SUCCEEDED(target_->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(D2D1::Point2F(frame.left, frame.top), D2D1::Point2F(frame.left, frame.bottom)), collection.Get(), &gradient))) {
        target_->FillRectangle(frame, gradient.Get());
    }
    target_->DrawRectangle(frame, brush(D2D1::ColorF(0.20F, 0.22F, 0.23F)));
}

void console_skin::draw_group(console_rect bounds, console_rect label_gap) const {
    if (target_ == nullptr || bounds.width <= 2.0F || bounds.height <= 2.0F) {
        return;
    }

    const auto frame = rect(bounds);
    const auto outer = D2D1::RoundedRect(frame, 4.0F, 4.0F);
    target_->FillRoundedRectangle(outer, brush(D2D1::ColorF(0.0F, 0.0F, 0.0F, 0.10F)));
    target_->DrawRoundedRectangle(outer, brush(D2D1::ColorF(0.005F, 0.006F, 0.007F, 0.94F)));

    const auto inner = D2D1::RectF(frame.left + 1.0F, frame.top + 1.0F, frame.right - 1.0F, frame.bottom - 1.0F);
    const auto inner_round = D2D1::RoundedRect(inner, 3.0F, 3.0F);
    target_->DrawRoundedRectangle(inner_round, brush(D2D1::ColorF(0.28F, 0.30F, 0.31F, 0.48F)));

    if (label_gap.width > 0.0F) {
        const console_rect seam_gap{
            std::max(bounds.x + 5.0F, label_gap.x - 3.0F),
            bounds.y - 2.0F,
            std::min(label_gap.width + 6.0F, bounds.right() - label_gap.x - 2.0F),
            5.0F,
        };
        draw_checker(seam_gap, true);
    }
}

void console_skin::draw_combo_box(console_rect bounds, std::wstring_view label, bool open, console_visual_state state) const {
    if (target_ == nullptr) return;
    draw_panel(bounds);
    constexpr float arrow_width = 18.0F;
    const console_rect arrow{bounds.right() - arrow_width, bounds.y + 1.0F, arrow_width - 1.0F, bounds.height - 2.0F};
    draw_button(arrow, L"", state);
    draw_text(label, {bounds.x + 3.0F, bounds.y + 1.0F, bounds.width - arrow_width - 5.0F, bounds.height - 2.0F},
              console_text_style::button, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    const float center_x = arrow.x + arrow.width * 0.5F;
    const float center_y = arrow.y + arrow.height * 0.5F;
    const float direction = open ? -1.0F : 1.0F;
    const auto glyph = brush(D2D1::ColorF(0.78F, 0.80F, 0.81F));
    target_->DrawLine(D2D1::Point2F(center_x - 3.0F, center_y - direction * 1.5F), D2D1::Point2F(center_x, center_y + direction * 1.5F), glyph, 1.1F);
    target_->DrawLine(D2D1::Point2F(center_x, center_y + direction * 1.5F), D2D1::Point2F(center_x + 3.0F, center_y - direction * 1.5F), glyph, 1.1F);
}

void console_skin::draw_button(console_rect bounds, std::wstring_view label, console_visual_state state) const {
    if (target_ == nullptr) {
        return;
    }

    const auto frame = rect(bounds);
    const bool is_pressed = state == console_visual_state::pressed;
    const bool is_selected = state == console_visual_state::selected;
    const bool is_hot = state == console_visual_state::hot;
    const auto top = is_selected ? D2D1::ColorF(0.28F, 0.45F, 0.64F) : (is_pressed ? D2D1::ColorF(0.15F, 0.16F, 0.17F) : is_hot ? D2D1::ColorF(0.34F, 0.36F, 0.38F) : D2D1::ColorF(0.27F, 0.28F, 0.30F));
    const auto shoulder = is_selected ? D2D1::ColorF(0.18F, 0.35F, 0.56F) : (is_pressed ? D2D1::ColorF(0.075F, 0.080F, 0.085F) : is_hot ? D2D1::ColorF(0.24F, 0.25F, 0.27F) : D2D1::ColorF(0.20F, 0.21F, 0.22F));
    const auto face = is_selected ? D2D1::ColorF(0.070F, 0.20F, 0.36F) : (is_pressed ? D2D1::ColorF(0.040F, 0.044F, 0.048F) : D2D1::ColorF(0.105F, 0.112F, 0.118F));
    const auto bottom = is_selected ? D2D1::ColorF(0.025F, 0.075F, 0.145F) : D2D1::ColorF(0.038F, 0.042F, 0.046F);

    const auto outer = D2D1::RoundedRect(frame, 2.25F, 2.25F);
    target_->FillRoundedRectangle(outer, brush(black));
    const float inset = is_pressed ? 2.0F : 1.0F;
    const auto inner = D2D1::RectF(frame.left + inset, frame.top + inset, frame.right - 1.0F, frame.bottom - 1.0F);
    const auto inner_round = D2D1::RoundedRect(inner, 1.5F, 1.5F);
    const D2D1_GRADIENT_STOP stops[]{
        {0.0F, top},
        {0.12F, shoulder},
        {0.48F, face},
        {1.0F, bottom},
    };
    Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> collection;
    Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> gradient;
    if (SUCCEEDED(target_->CreateGradientStopCollection(stops, std::size(stops), &collection)) &&
        SUCCEEDED(target_->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(D2D1::Point2F(inner.left, inner.top), D2D1::Point2F(inner.left, inner.bottom)), collection.Get(), &gradient))) {
        target_->FillRoundedRectangle(inner_round, gradient.Get());
    } else {
        target_->FillRoundedRectangle(inner_round, brush(face));
    }
    target_->DrawRoundedRectangle(inner_round, brush(D2D1::ColorF(0.010F, 0.012F, 0.014F, 0.80F)));
    target_->DrawLine(D2D1::Point2F(inner.left + 2.0F, inner.bottom - 1.0F), D2D1::Point2F(inner.right - 2.0F, inner.bottom - 1.0F),
                      brush(D2D1::ColorF(0.0F, 0.0F, 0.0F, 0.55F)));
    const auto text_bounds = console_rect{bounds.x + 1.0F, bounds.y + 1.0F, std::max(1.0F, bounds.width - 2.0F), std::max(1.0F, bounds.height - 2.0F)};
    const auto shadow_bounds = console_rect{text_bounds.x + (is_pressed ? 0.0F : 1.0F), text_bounds.y + 1.0F,
                                             std::max(1.0F, text_bounds.width - 1.0F), std::max(1.0F, text_bounds.height - 1.0F)};
    draw_text(label, shadow_bounds, console_text_style::button, DWRITE_TEXT_ALIGNMENT_CENTER,
              DWRITE_PARAGRAPH_ALIGNMENT_CENTER, D2D1::ColorF(0.0F, 0.0F, 0.0F, 0.8F));
    draw_text(label, text_bounds, console_text_style::button, DWRITE_TEXT_ALIGNMENT_CENTER,
              DWRITE_PARAGRAPH_ALIGNMENT_CENTER, D2D1::ColorF(0.95F, 0.96F, 0.97F));
}

void console_skin::draw_caption_button(console_rect bounds, std::wstring_view label, console_visual_state state) const {
    if (target_ == nullptr) {
        return;
    }

    const auto frame = rect(bounds);
    const float press_offset = state == console_visual_state::pressed ? 0.7F : 0.0F;
    const auto center = D2D1::Point2F((frame.left + frame.right) * 0.5F, (frame.top + frame.bottom) * 0.5F + press_offset);
    const float radius = std::min(bounds.width, bounds.height) * 0.5F - 0.5F;
    const auto outer = D2D1::Ellipse(center, radius, radius);
    const auto inner = D2D1::Ellipse(center, radius - 1.2F, radius - 1.2F);
    target_->FillEllipse(outer, brush(D2D1::ColorF(0.010F, 0.012F, 0.014F)));
    target_->DrawEllipse(outer, brush(D2D1::ColorF(0.47F, 0.49F, 0.50F, 0.70F)));

    const D2D1_GRADIENT_STOP stops[]{
        {0.0F, state == console_visual_state::hot ? D2D1::ColorF(0.72F, 0.79F, 0.85F) : D2D1::ColorF(0.72F, 0.74F, 0.75F)},
        {0.45F, state == console_visual_state::hot ? D2D1::ColorF(0.34F, 0.44F, 0.53F) : D2D1::ColorF(0.37F, 0.40F, 0.42F)},
        {1.0F, D2D1::ColorF(0.12F, 0.14F, 0.16F)},
    };
    Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> collection;
    Microsoft::WRL::ComPtr<ID2D1RadialGradientBrush> gradient;
    if (SUCCEEDED(target_->CreateGradientStopCollection(stops, std::size(stops), &collection)) &&
        SUCCEEDED(target_->CreateRadialGradientBrush(D2D1::RadialGradientBrushProperties(center, D2D1::Point2F(-1.5F, -2.0F), radius - 1.2F, radius - 1.2F), collection.Get(), &gradient))) {
        target_->FillEllipse(inner, gradient.Get());
    } else {
        target_->FillEllipse(inner, brush(D2D1::ColorF(0.37F, 0.40F, 0.42F)));
    }
    target_->DrawEllipse(inner, brush(D2D1::ColorF(0.08F, 0.09F, 0.10F)));

    const auto glyph = brush(D2D1::ColorF(0.06F, 0.07F, 0.08F));
    if (label == L"-") {
        target_->DrawLine(D2D1::Point2F(center.x - 2.7F, center.y + 0.4F), D2D1::Point2F(center.x + 2.7F, center.y + 0.4F), glyph, 1.4F);
    } else if (label == L"X") {
        target_->DrawLine(D2D1::Point2F(center.x - 2.3F, center.y - 2.3F), D2D1::Point2F(center.x + 2.3F, center.y + 2.3F), glyph, 1.2F);
        target_->DrawLine(D2D1::Point2F(center.x + 2.3F, center.y - 2.3F), D2D1::Point2F(center.x - 2.3F, center.y + 2.3F), glyph, 1.2F);
    } else {
        draw_text(label, bounds, console_text_style::label, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, glyph->GetColor());
    }
}

void console_skin::draw_radio(console_point center, bool selected, std::wstring_view label) const {
    if (target_ == nullptr) {
        return;
    }

    const auto ellipse = D2D1::Ellipse(D2D1::Point2F(center.x, center.y), 5.0F, 5.0F);
    target_->FillEllipse(ellipse, brush(black));
    target_->DrawEllipse(ellipse, brush(chrome));
    if (selected) {
        target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x, center.y), 2.6F, 2.6F), brush(D2D1::ColorF(0.77F, 0.80F, 0.82F)));
    }
    draw_text(label, {center.x + 9.0F, center.y - 7.0F, 38.0F, 14.0F}, console_text_style::label);
}

void console_skin::draw_equalizer_toggle(console_rect bounds, bool enabled) const {
    if (target_ == nullptr) return;
    draw_panel(bounds);
    const auto frame = rect(bounds);
    const auto inner = D2D1::RectF(frame.left + 2.0F, frame.top + 2.0F, frame.right - 2.0F, frame.bottom - 2.0F);
    const float split = (inner.left + inner.right) * 0.5F;
    const auto bypass_color = enabled ? D2D1::ColorF(0.075F, 0.080F, 0.085F) : D2D1::ColorF(0.34F, 0.30F, 0.17F);
    const auto on_color = enabled ? D2D1::ColorF(0.10F, 0.34F, 0.54F) : D2D1::ColorF(0.075F, 0.080F, 0.085F);
    target_->FillRectangle(D2D1::RectF(inner.left, inner.top, split - 1.0F, inner.bottom), brush(bypass_color));
    target_->FillRectangle(D2D1::RectF(split + 1.0F, inner.top, inner.right, inner.bottom), brush(on_color));
    target_->DrawLine(D2D1::Point2F(split, inner.top), D2D1::Point2F(split, inner.bottom), brush(D2D1::ColorF(0.005F, 0.006F, 0.007F)));
    const auto active_left = !enabled;
    draw_text(L"BYPASS", {inner.left, inner.top, split - inner.left - 1.0F, inner.bottom - inner.top}, console_text_style::label,
              DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, active_left ? D2D1::ColorF(1.0F, 0.94F, 0.72F) : D2D1::ColorF(0.66F, 0.68F, 0.68F));
    draw_text(L"ON", {split + 1.0F, inner.top, inner.right - split - 1.0F, inner.bottom - inner.top}, console_text_style::label,
              DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, enabled ? D2D1::ColorF(0.87F, 0.96F, 1.0F) : D2D1::ColorF(0.66F, 0.68F, 0.68F));
}

void console_skin::draw_checkbox(console_rect bounds, bool checked, std::wstring_view label) const {
    if (target_ == nullptr) {
        return;
    }

    const float box_size = std::min(14.0F, bounds.height - 6.0F);
    const auto box = D2D1::RectF(bounds.x + 3.0F, bounds.y + (bounds.height - box_size) * 0.5F, bounds.x + 3.0F + box_size,
                                 bounds.y + (bounds.height + box_size) * 0.5F);
    target_->FillRectangle(box, brush(D2D1::ColorF(0.025F, 0.028F, 0.031F)));
    target_->DrawRectangle(box, brush(D2D1::ColorF(0.32F, 0.34F, 0.35F)));
    if (checked) {
        const auto mark = brush(D2D1::ColorF(0.71F, 0.76F, 0.79F));
        target_->DrawLine(D2D1::Point2F(box.left + 3.0F, box.top + box_size * 0.56F), D2D1::Point2F(box.left + 5.6F, box.bottom - 3.0F), mark, 1.15F);
        target_->DrawLine(D2D1::Point2F(box.left + 5.6F, box.bottom - 3.0F), D2D1::Point2F(box.right - 2.4F, box.top + 2.8F), mark, 1.15F);
    }
    draw_text(label, {box.right + 4.0F, bounds.y, std::max(1.0F, bounds.right() - box.right - 4.0F), bounds.height}, console_text_style::label);
}

void console_skin::draw_scrollbar(console_rect track, console_rect thumb, bool hot, bool pressed) const {
    draw_panel(track);
    draw_button({track.x + 1.0F, track.y + 1.0F, track.width - 2.0F, 14.0F}, L"^", console_visual_state::normal);
    draw_button({track.x + 1.0F, track.bottom() - 15.0F, track.width - 2.0F, 14.0F}, L"v", console_visual_state::normal);
    const auto t = rect(thumb);
    target_->FillRectangle(t, brush(pressed ? D2D1::ColorF(0.32F, 0.35F, 0.37F) : hot ? D2D1::ColorF(0.47F, 0.49F, 0.50F) : chrome));
    target_->DrawLine(D2D1::Point2F(t.left, t.top), D2D1::Point2F(t.right, t.top), brush(silver));
    target_->DrawLine(D2D1::Point2F(t.left, t.bottom - 1.0F), D2D1::Point2F(t.right, t.bottom - 1.0F), brush(black));
    for (float y = t.top + 5.0F; y < t.bottom - 3.0F; y += 4.0F) {
        target_->DrawLine(D2D1::Point2F(t.left + 3.0F, y), D2D1::Point2F(t.right - 3.0F, y), brush(D2D1::ColorF(0.18F, 0.19F, 0.20F)));
    }
}

void console_skin::draw_fader(console_rect up_button,
                               console_rect track,
                               console_rect down_button,
                               float gain_db,
                               console_visual_state state) const {
    draw_button(up_button, L"", state == console_visual_state::pressed ? state : console_visual_state::normal);
    draw_button(down_button, L"", state == console_visual_state::pressed ? state : console_visual_state::normal);
    const auto draw_chevron = [this](console_rect button, bool up) {
        const float center_x = button.x + button.width * 0.5F;
        const float center_y = button.y + button.height * 0.5F;
        const float spread = 3.3F;
        const float rise = 2.0F;
        const auto color = brush(D2D1::ColorF(0.78F, 0.80F, 0.81F));
        if (up) {
            target_->DrawLine(D2D1::Point2F(center_x - spread, center_y + rise), D2D1::Point2F(center_x, center_y - rise), color, 1.1F);
            target_->DrawLine(D2D1::Point2F(center_x, center_y - rise), D2D1::Point2F(center_x + spread, center_y + rise), color, 1.1F);
        } else {
            target_->DrawLine(D2D1::Point2F(center_x - spread, center_y - rise), D2D1::Point2F(center_x, center_y + rise), color, 1.1F);
            target_->DrawLine(D2D1::Point2F(center_x, center_y + rise), D2D1::Point2F(center_x + spread, center_y - rise), color, 1.1F);
        }
    };
    draw_chevron(up_button, true);
    draw_chevron(down_button, false);
    draw_panel(track);
    const auto frame = rect(track);
    const float center = (frame.top + frame.bottom) * 0.5F;
    const auto rail = D2D1::RectF(frame.left + 7.0F, frame.top + 2.0F, frame.right - 7.0F, frame.bottom - 2.0F);
    target_->FillRectangle(rail, brush(D2D1::ColorF(0.012F, 0.014F, 0.016F, 0.90F)));
    target_->DrawLine(D2D1::Point2F(rail.left, center), D2D1::Point2F(rail.right, center), brush(D2D1::ColorF(0.24F, 0.25F, 0.26F, 0.45F)));
    const float ratio = (20.0F - clamp_gain(gain_db)) / 40.0F;
    constexpr float thumb_half_height = 4.5F;
    const float y = std::lerp(frame.top + thumb_half_height + 2.0F, frame.bottom - thumb_half_height - 2.0F, ratio);
    const auto thumb = D2D1::RectF(frame.left + 3.0F, y - thumb_half_height, frame.right - 3.0F, y + thumb_half_height);
    target_->FillRectangle(thumb, brush(D2D1::ColorF(0.10F, 0.105F, 0.11F)));
    const D2D1_GRADIENT_STOP thumb_stops[]{
        {0.0F, D2D1::ColorF(0.34F, 0.35F, 0.36F)},
        {0.30F, D2D1::ColorF(0.28F, 0.29F, 0.30F)},
        {1.0F, D2D1::ColorF(0.17F, 0.18F, 0.19F)},
    };
    Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> thumb_collection;
    Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> thumb_gradient;
    const auto thumb_inner = D2D1::RectF(thumb.left + 1.0F, thumb.top + 1.0F, thumb.right - 1.0F, thumb.bottom - 1.0F);
    if (SUCCEEDED(target_->CreateGradientStopCollection(thumb_stops, std::size(thumb_stops), &thumb_collection)) &&
        SUCCEEDED(target_->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(D2D1::Point2F(thumb_inner.left, thumb_inner.top), D2D1::Point2F(thumb_inner.left, thumb_inner.bottom)), thumb_collection.Get(), &thumb_gradient))) {
        target_->FillRectangle(thumb_inner, thumb_gradient.Get());
    } else {
        target_->FillRectangle(thumb_inner, brush(D2D1::ColorF(0.30F, 0.31F, 0.32F)));
    }
}

void console_skin::draw_text(std::wstring_view text,
                              console_rect bounds,
                              console_text_style style,
                              DWRITE_TEXT_ALIGNMENT alignment,
                              DWRITE_PARAGRAPH_ALIGNMENT vertical,
                              D2D1_COLOR_F color) const {
    if (target_ == nullptr || text.empty()) {
        return;
    }

    const auto format = text_format(style);
    if (format == nullptr) {
        return;
    }
    format->SetTextAlignment(alignment);
    format->SetParagraphAlignment(vertical);
    format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    target_->DrawTextW(text.data(), static_cast<UINT32>(text.size()), format, rect(bounds), brush(color), D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

void console_skin::draw_rotated_text(std::wstring_view text, console_rect bounds, float degrees, console_point pivot, console_text_style style) const {
    if (target_ == nullptr || text.empty()) {
        return;
    }
    D2D1_MATRIX_3X2_F original{};
    target_->GetTransform(&original);
    target_->SetTransform(D2D1::Matrix3x2F::Rotation(degrees, D2D1::Point2F(pivot.x, pivot.y)) * original);
    draw_text(text, bounds, style, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    target_->SetTransform(original);
}

void console_skin::draw_display_number(std::wstring_view text, console_rect bounds, D2D1_COLOR_F color) const {
    if (target_ == nullptr || text.empty()) {
        return;
    }

    const console_rect display_bounds{bounds.x + 2.0F, bounds.y + 2.0F, std::max(1.0F, bounds.width - 4.0F), std::max(1.0F, bounds.height - 4.0F)};
    const float pad = std::max(2.0F, display_bounds.height * 0.18F);
    const float digit_height = display_bounds.height - pad * 2.0F;
    const auto display = console_layout::measure_display_layout(text, display_bounds);
    const float digit_width = display.digit_width;
    constexpr float gap_units = 0.16F;
    const float segment = std::max(1.0F, std::min(digit_width, digit_height) * 0.105F);
    target_->PushAxisAlignedClip(rect(display_bounds), D2D1_ANTIALIAS_MODE_ALIASED);
    const auto draw_segment = [this, color](float x, float y, float width, float height, bool horizontal) {
        (void)horizontal;
        const float radius = std::min(width, height) * 0.22F;
        const auto bounds = D2D1::RoundedRect(D2D1::RectF(x, y, x + width, y + height), radius, radius);
        target_->FillRoundedRectangle(bounds, brush(color));
    };
    const auto mask_for = [](wchar_t character) -> unsigned char {
        switch (character) {
            case L'0': return 0x3F;
            case L'1': return 0x06;
            case L'2': return 0x5B;
            case L'3': return 0x4F;
            case L'4': return 0x66;
            case L'5': return 0x6D;
            case L'6': return 0x7D;
            case L'7': return 0x07;
            case L'8': return 0x7F;
            case L'9': return 0x6F;
            case L'-': return 0x40;
            default: return 0;
        }
    };

    float x = display.origin_x;
    for (std::size_t index = 0; index < text.size(); ++index) {
        const auto character = text[index];
        const float cell_width = console_layout::display_glyph_units(character) * digit_width;
        if (character == L'.') {
            const float radius = std::max(0.8F, segment * 0.72F);
            target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x + cell_width * 0.5F, display_bounds.bottom() - pad - radius), radius, radius), brush(color));
        } else if (character == L'-') {
            const float y = display_bounds.y + pad + (digit_height - segment) * 0.5F;
            draw_segment(x, y, cell_width, segment, true);
        } else {
            const auto mask = mask_for(character);
            const float left = x + segment;
            const float right = x + cell_width - segment;
            const float top = display_bounds.y + pad;
            const float middle = top + (digit_height - segment) * 0.5F;
            const float bottom = display_bounds.bottom() - pad - segment;
            const float vertical = std::max(1.0F, middle - top - segment * 0.5F);
            if ((mask & 0x01U) != 0) draw_segment(left, top, right - left, segment, true);
            if ((mask & 0x02U) != 0) draw_segment(right, top + segment * 0.45F, segment, vertical, false);
            if ((mask & 0x04U) != 0) draw_segment(right, middle + segment * 0.45F, segment, vertical, false);
            if ((mask & 0x08U) != 0) draw_segment(left, bottom, right - left, segment, true);
            if ((mask & 0x10U) != 0) draw_segment(x, middle + segment * 0.45F, segment, vertical, false);
            if ((mask & 0x20U) != 0) draw_segment(x, top + segment * 0.45F, segment, vertical, false);
            if ((mask & 0x40U) != 0) draw_segment(left, middle, right - left, segment, true);
        }
        x += cell_width;
        if (index + 1 < text.size()) {
            x += digit_width * gap_units;
        }
    }
    target_->PopAxisAlignedClip();
}

ID2D1BitmapBrush* console_skin::background_brush(std::size_t background_index) const {
    if (target_ == nullptr || wic_factory_ == nullptr || background_index >= background_brushes_.size()) {
        return nullptr;
    }

    auto& result = background_brushes_[background_index];
    if (result != nullptr) {
        return result.Get();
    }

    const auto path = asset_path(background_asset_names[background_index]);
    if (path.empty()) {
        return nullptr;
    }

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(wic_factory_->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder))) {
        return nullptr;
    }
    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame))) {
        return nullptr;
    }
    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    if (FAILED(wic_factory_->CreateFormatConverter(&converter)) ||
        FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0F, WICBitmapPaletteTypeCustom))) {
        return nullptr;
    }
    Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
    if (FAILED(target_->CreateBitmapFromWicBitmap(converter.Get(), nullptr, &bitmap))) {
        return nullptr;
    }
    const auto properties = D2D1::BitmapBrushProperties(D2D1_EXTEND_MODE_WRAP, D2D1_EXTEND_MODE_WRAP, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
    if (FAILED(target_->CreateBitmapBrush(bitmap.Get(), properties, &result))) {
        return nullptr;
    }
    result->SetOpacity(0.60F);
    return result.Get();
}

ID2D1SolidColorBrush* console_skin::brush(D2D1_COLOR_F color) const {
    const auto to_byte = [](float component) {
        return static_cast<std::uint32_t>(std::clamp(component, 0.0F, 1.0F) * 255.0F + 0.5F);
    };
    const auto key = to_byte(color.r) | (to_byte(color.g) << 8U) | (to_byte(color.b) << 16U) | (to_byte(color.a) << 24U);
    if (const auto existing = brush_cache_.find(key); existing != brush_cache_.end()) {
        return existing->second.Get();
    }

    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> value;
    if (SUCCEEDED(target_->CreateSolidColorBrush(color, &value))) {
        return brush_cache_.emplace(key, std::move(value)).first->second.Get();
    }
    return nullptr;
}

D2D1_RECT_F console_skin::rect(console_rect value) const noexcept {
    return D2D1::RectF(value.x, value.y, value.right(), value.bottom());
}

IDWriteTextFormat* console_skin::text_format(console_text_style style) const noexcept {
    switch (style) {
        case console_text_style::label:
            return label_format_.Get();
        case console_text_style::button:
            return button_format_.Get();
        case console_text_style::numeric:
            return numeric_format_.Get();
        case console_text_style::title:
            return title_format_.Get();
        case console_text_style::menu:
            return menu_format_.Get();
    }
    return label_format_.Get();
}

}  // namespace termite
