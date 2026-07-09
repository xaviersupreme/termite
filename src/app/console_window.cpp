#include "app/console_window.h"
#include "app/termite_resource.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cwchar>
#include <dwmapi.h>
#include <ShObjIdl.h>
#include <filesystem>
#include <format>
#include <numbers>
#include <optional>
#include <shellapi.h>
#include <string>
#include <string_view>
#include <windowsx.h>

namespace termite {
namespace {

constexpr wchar_t window_class_name[] = L"TermiteConsoleWindow";
constexpr UINT_PTR status_timer_id = 1;
constexpr UINT_PTR settings_save_timer_id = 2;
constexpr UINT tray_callback_message = WM_APP + 1;
constexpr UINT tray_icon_id = 1;
constexpr UINT tray_show_command = 2001;
constexpr UINT tray_quit_command = 2002;
constexpr int fader_edit_control_id = 1001;
constexpr DWORD dark_mode_attribute = 20;
constexpr DWORD window_corner_preference_attribute = 33;
constexpr int round_window_corners = 2;
constexpr float filter_menu_header_height = 22.0F;
constexpr float filter_menu_row_height = 19.0F;
constexpr int filter_menu_shape_rows = 6;
constexpr int filter_menu_toggle_row = 6;
constexpr int routing_picker_refresh = -2;
constexpr int routing_picker_route = -3;
constexpr int routing_picker_mixer = -4;
constexpr int routing_picker_close = -5;
constexpr float routing_picker_header_height = 24.0F;
constexpr float routing_picker_instruction_height = 28.0F;
constexpr float routing_picker_guidance_height = 25.0F;
constexpr float routing_picker_footer_height = 35.0F;

float clamp(float value, float low, float high) {
    return std::clamp(value, low, high);
}

std::wstring band_label(float frequency_hz) {
    return std::format(L"{:.0f}", frequency_hz);
}

std::wstring gain_label(float gain_db) {
    if (std::abs(gain_db) < 0.05F) {
        return L"0";
    }
    if (std::abs(gain_db - std::round(gain_db)) < 0.05F) {
        return std::format(L"{:.0f}", gain_db);
    }
    return std::format(L"{:.1f}", gain_db);
}

std::string narrow(std::wstring_view value) {
    if (value.empty()) return {};
    const auto length = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (length <= 0) return {};
    std::string result(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), length, nullptr, nullptr);
    return result;
}

std::wstring widen(std::string_view value) {
    if (value.empty()) return {};
    const auto length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (length <= 0) return {};
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), length);
    return result;
}

std::optional<std::filesystem::path> choose_profile_file(HWND owner, bool saving, std::wstring& failure_reason) {
    Microsoft::WRL::ComPtr<IFileDialog> dialog;
    const auto result = CoCreateInstance(saving ? CLSID_FileSaveDialog : CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                         IID_PPV_ARGS(dialog.GetAddressOf()));
    if (FAILED(result)) {
        failure_reason = L"Windows could not open the profile file dialog.";
        return std::nullopt;
    }
    constexpr COMDLG_FILTERSPEC filters[]{{L"Termite EQ profile (*.termiteeq)", L"*.termiteeq"}, {L"All files (*.*)", L"*.*"}};
    dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);
    dialog->SetFileTypeIndex(1);
    dialog->SetTitle(saving ? L"Save Termite EQ profile" : L"Open Termite EQ profile");
    if (saving) {
        dialog->SetDefaultExtension(termite_profile_extension + 1);
        dialog->SetFileName(L"Custom profile.termiteeq");
    }
    const auto show_result = dialog->Show(owner);
    if (show_result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) return std::nullopt;
    if (FAILED(show_result)) {
        failure_reason = L"Windows could not show the profile file dialog.";
        return std::nullopt;
    }
    Microsoft::WRL::ComPtr<IShellItem> item;
    if (FAILED(dialog->GetResult(item.GetAddressOf()))) {
        failure_reason = L"Windows did not return a selected profile file.";
        return std::nullopt;
    }
    PWSTR raw_path{};
    if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &raw_path)) || raw_path == nullptr) {
        failure_reason = L"Windows could not read the selected profile path.";
        return std::nullopt;
    }
    const std::filesystem::path path{raw_path};
    CoTaskMemFree(raw_path);
    return path;
}

console_visual_state visual_state_for(console_hit control, console_hit hot, console_hit pressed, bool selected = false) {
    if (pressed == control) {
        return console_visual_state::pressed;
    }
    if (selected) {
        return console_visual_state::selected;
    }
    if (hot == control) {
        return console_visual_state::hot;
    }
    return console_visual_state::normal;
}

}  // namespace

console_window::console_window(HINSTANCE instance) : instance_(instance), com_initialization_(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2d_factory_.GetAddressOf());
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(write_factory_.GetAddressOf()));
    skin_ = std::make_unique<console_skin>(write_factory_.Get(), d2d_factory_.Get());
    title_icon_ = LoadIconW(instance_, MAKEINTRESOURCEW(IDI_TERMITE));
    skin_->set_title_icon(title_icon_);
}

console_window::~console_window() {
    audio_engine_.stop();
    if (SUCCEEDED(com_initialization_)) {
        CoUninitialize();
    }
}

int console_window::run() {
    load_settings();
    if (!create_window()) {
        return 1;
    }

    create_tray_icon();
    ShowWindow(window_, SW_SHOW);
    UpdateWindow(window_);
    sync_profile();
    if (!audio_engine_.start()) {
        state_.append_engine_status(audio_engine_.status_text());
    } else {
        state_.append_engine_status("Audio engine started.");
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

bool console_window::create_window() {
    WNDCLASSEXW definition{};
    definition.cbSize = sizeof(definition);
    definition.style = CS_HREDRAW | CS_VREDRAW;
    definition.lpfnWndProc = &console_window::window_proc;
    definition.hInstance = instance_;
    definition.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    definition.hIcon = title_icon_;
    definition.hIconSm = title_icon_;
    definition.lpszClassName = window_class_name;
    if (RegisterClassExW(&definition) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    dpi_ = GetDpiForSystem();
    RECT work_area{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);
    constexpr LONG outer_margin = 16;
    const auto initial = console_layout::fit_canvas_to_bounds({
        static_cast<float>(std::max(1L, work_area.right - work_area.left - outer_margin * 2)),
        static_cast<float>(std::max(1L, work_area.bottom - work_area.top - outer_margin * 2)),
    }, 1.20F);
    auto width = static_cast<int>(std::lround(initial.width));
    auto height = static_cast<int>(std::lround(initial.height));
    auto left = work_area.left + ((work_area.right - work_area.left) - width) / 2;
    auto top = work_area.top + ((work_area.bottom - work_area.top) - height) / 2;
    if (restored_window_bounds_.valid) {
        const auto restored = console_layout::constrain_aspect_ratio({static_cast<float>(restored_window_bounds_.width), static_cast<float>(restored_window_bounds_.height)});
        const auto work_width = static_cast<int>(work_area.right - work_area.left);
        const auto work_height = static_cast<int>(work_area.bottom - work_area.top);
        const auto work_left = static_cast<int>(work_area.left);
        const auto work_top = static_cast<int>(work_area.top);
        width = std::min(static_cast<int>(std::lround(restored.width)), work_width);
        height = std::min(static_cast<int>(std::lround(restored.height)), work_height);
        left = std::clamp(restored_window_bounds_.x, work_left, work_left + work_width - width);
        top = std::clamp(restored_window_bounds_.y, work_top, work_top + work_height - height);
    }
    window_ = CreateWindowExW(WS_EX_APPWINDOW,
                              window_class_name,
                              L"Termite",
                              WS_POPUP | WS_MINIMIZEBOX,
                              left,
                              top,
                              width,
                              height,
                              nullptr,
                              nullptr,
                              instance_,
                              this);
    if (window_ == nullptr) {
        return false;
    }

    const BOOL dark_mode = TRUE;
    DwmSetWindowAttribute(window_, dark_mode_attribute, &dark_mode, sizeof(dark_mode));
    DwmSetWindowAttribute(window_, window_corner_preference_attribute, &round_window_corners, sizeof(round_window_corners));
    update_window_region();
    SetTimer(window_, status_timer_id, 700, nullptr);
    return true;
}

LRESULT CALLBACK console_window::window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* self = reinterpret_cast<console_window*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        self = static_cast<console_window*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->window_ = window;
    }
    return self == nullptr ? DefWindowProcW(window, message, wparam, lparam) : self->handle_message(message, wparam, lparam);
}

LRESULT CALLBACK console_window::fader_edit_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    const auto* self = reinterpret_cast<console_window*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (self == nullptr || self->fader_edit_original_proc_ == nullptr) {
        return DefWindowProcW(window, message, wparam, lparam);
    }

    if (message == WM_KEYDOWN) {
        if (wparam == VK_RETURN) {
            const_cast<console_window*>(self)->finish_fader_edit(true);
            return 0;
        }
        if (wparam == VK_ESCAPE) {
            const_cast<console_window*>(self)->finish_fader_edit(false);
            return 0;
        }
    }
    if (message == WM_CHAR) {
        const auto character = static_cast<wchar_t>(wparam);
        if (!(character == VK_BACK || (character >= L'0' && character <= L'9') || character == L'-' || character == L'.')) {
            return 0;
        }
    }
    return CallWindowProcW(self->fader_edit_original_proc_, window, message, wparam, lparam);
}

LRESULT console_window::handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_NCHITTEST: {
            POINT screen{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            return hit_test_screen(screen);
        }
        case WM_GETMINMAXINFO: {
            auto* constraints = reinterpret_cast<MINMAXINFO*>(lparam);
            const auto minimum = console_layout::constrain_aspect_ratio({0.0F, 0.0F});
            constraints->ptMinTrackSize.x = static_cast<LONG>(std::lround(minimum.width));
            constraints->ptMinTrackSize.y = static_cast<LONG>(std::lround(minimum.height));
            return 0;
        }
        case WM_SIZING: {
            auto* bounds = reinterpret_cast<RECT*>(lparam);
            const float requested_width = static_cast<float>(bounds->right - bounds->left);
            const float requested_height = static_cast<float>(bounds->bottom - bounds->top);
            const auto constrained = console_layout::constrain_aspect_ratio({requested_width, requested_height});
            const auto width = static_cast<LONG>(std::lround(constrained.width));
            const auto height = static_cast<LONG>(std::lround(constrained.height));
            switch (wparam) {
                case WMSZ_LEFT:
                case WMSZ_TOPLEFT:
                case WMSZ_BOTTOMLEFT:
                    bounds->left = bounds->right - width;
                    break;
                default:
                    bounds->right = bounds->left + width;
                    break;
            }
            switch (wparam) {
                case WMSZ_TOP:
                case WMSZ_TOPLEFT:
                case WMSZ_TOPRIGHT:
                    bounds->top = bounds->bottom - height;
                    break;
                default:
                    bounds->bottom = bounds->top + height;
                    break;
            }
            return TRUE;
        }
        case WM_DPICHANGED: {
            dpi_ = HIWORD(wparam);
            const auto* suggested = reinterpret_cast<const RECT*>(lparam);
            RECT current{};
            GetWindowRect(window_, &current);
            SetWindowPos(window_, nullptr, suggested->left, suggested->top,
                         current.right - current.left, current.bottom - current.top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            discard_render_target();
            return 0;
        }
        case WM_SIZE:
            update_window_region();
            if (render_target_ != nullptr) {
                render_target_->Resize(D2D1::SizeU(LOWORD(lparam), HIWORD(lparam)));
            }
            position_fader_edit();
            schedule_settings_save();
            InvalidateRect(window_, nullptr, FALSE);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            BeginPaint(window_, &paint);
            render();
            EndPaint(window_, &paint);
            return 0;
        }
        case WM_TIMER:
            if (wparam == status_timer_id) {
                append_audio_status();
                if (diagnostics_popup_open_ || state_.active_tab() == console_tab::monitor) InvalidateRect(window_, nullptr, FALSE);
            } else if (wparam == settings_save_timer_id) {
                KillTimer(window_, settings_save_timer_id);
                save_settings();
            }
            return 0;
        case tray_callback_message:
            // NOTIFYICON_VERSION_4 packs the event in the low word. Comparing
            // the complete LPARAM made the icon inert when it included its id.
            switch (const auto event = LOWORD(lparam)) {
            case NIN_SELECT:
            case NIN_KEYSELECT:
            case WM_LBUTTONUP:
            case WM_LBUTTONDBLCLK:
                show_from_tray();
                return 0;
            case WM_RBUTTONUP:
            case WM_CONTEXTMENU: {
                POINT point{};
                GetCursorPos(&point);
                const auto menu = CreatePopupMenu();
                if (menu != nullptr) {
                    AppendMenuW(menu, MF_STRING, tray_show_command, L"Show Termite");
                    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
                    AppendMenuW(menu, MF_STRING, tray_quit_command, L"Quit");
                    SetForegroundWindow(window_);
                    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, point.x, point.y, 0, window_, nullptr);
                    DestroyMenu(menu);
                    PostMessageW(window_, WM_NULL, 0, 0);
                }
                return 0;
            }
            default:
                return 0;
            }
        case WM_MOUSEMOVE: {
            POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            update_pointer(point);
            const auto design = to_design(point);
            if (active_fader_ >= 0) {
                update_fader_from_point(active_fader_, design);
            }
            if (dragging_scroll_) {
                update_scroll_from_point(design);
            }
            return 0;
        }
        case WM_MOUSELEAVE:
            tracking_mouse_ = false;
            hot_hit_ = {};
            filter_menu_hot_row_ = -1;
            preset_dropdown_hot_row_ = -1;
            routing_picker_hot_row_ = -1;
            InvalidateRect(window_, nullptr, FALSE);
            return 0;
        case WM_LBUTTONDOWN: {
            POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            const auto design = to_design(point);
            if (diagnostics_popup_open_) {
                diagnostics_popup_open_ = false;
                InvalidateRect(window_, nullptr, FALSE);
                return 0;
            }
            if (preset_dropdown_open_) {
                const auto row = preset_dropdown_row(design);
                if (row >= 0) {
                    preset_dropdown_pressed_row_ = row;
                    SetCapture(window_);
                    InvalidateRect(window_, nullptr, FALSE);
                    return 0;
                }
                preset_dropdown_open_ = false;
                preset_dropdown_pressed_row_ = -1;
                if (console_layout::control_rect(console_control::preset_cycle).contains(design)) {
                    InvalidateRect(window_, nullptr, FALSE);
                    return 0;
                }
            }
            if (routing_picker_open_) {
                const auto row = routing_picker_row(design);
                if (row >= 0 || row == routing_picker_refresh || row == routing_picker_route || row == routing_picker_mixer || row == routing_picker_close) {
                    routing_picker_pressed_row_ = row;
                    SetCapture(window_);
                } else if (!console_layout::routing_picker_frame(routing_candidates_.size()).contains(design)) {
                    routing_picker_open_ = false;
                }
                InvalidateRect(window_, nullptr, FALSE);
                return 0;
            }
            if (filter_menu_band_ >= 0) {
                const auto row = fader_filter_menu_row(design);
                if (row >= 0) {
                    filter_menu_pressed_row_ = row;
                    SetCapture(window_);
                } else {
                    filter_menu_band_ = -1;
                    filter_menu_hot_row_ = -1;
                }
                InvalidateRect(window_, nullptr, FALSE);
                return 0;
            }
            const auto hit = console_layout::hit_test(design, state_.notices().size(), state_.scroll_offset(), state_.active_tab());
            if (editing_fader_ >= 0 && (hit.control != console_control::fader_value || hit.index != editing_fader_)) {
                finish_fader_edit(true);
            }
            if (hit.control == console_control::fader_value) {
                begin_fader_edit(hit.index);
                return 0;
            }
            if (hit.control == console_control::preset_cycle) {
                preset_dropdown_open_ = true;
                preset_dropdown_hot_row_ = -1;
                preset_dropdown_pressed_row_ = -1;
                InvalidateRect(window_, nullptr, FALSE);
                return 0;
            }
            pressed_hit_ = hit;
            if (hit.control == console_control::fader_track) {
                active_fader_ = hit.index;
                update_fader_from_point(hit.index, design);
            } else if (hit.control == console_control::scroll_thumb) {
                dragging_scroll_ = true;
                scroll_drag_offset_ = design.y - console_layout::status_scroll_thumb(state_.notices().size(), state_.scroll_offset()).y;
            }
            if (hit.control != console_control::none) {
                SetCapture(window_);
                InvalidateRect(window_, nullptr, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            if (preset_dropdown_open_ && preset_dropdown_pressed_row_ >= 0) {
                const auto row = preset_dropdown_row(to_design(point));
                if (GetCapture() == window_) ReleaseCapture();
                if (row == preset_dropdown_pressed_row_ && state_.apply_preset(static_cast<std::size_t>(row))) {
                    sync_profile();
                }
                preset_dropdown_open_ = false;
                preset_dropdown_hot_row_ = -1;
                preset_dropdown_pressed_row_ = -1;
                InvalidateRect(window_, nullptr, FALSE);
                return 0;
            }
            if (routing_picker_open_) {
                const auto row = routing_picker_row(to_design(point));
                if (GetCapture() == window_) ReleaseCapture();
                if (row == routing_picker_pressed_row_) {
                    if (row >= 0 && static_cast<std::size_t>(row) < routing_selected_.size()) {
                        const auto index = static_cast<std::size_t>(row);
                        const auto& candidate = routing_candidates_[index];
                        if (routing_selected_[index]) {
                            const auto route = std::find_if(automatic_routes_.begin(), automatic_routes_.end(), [&candidate](const app_audio_route_snapshot& existing) {
                                return _wcsicmp(existing.executable_path.c_str(), candidate.executable_path.c_str()) == 0;
                            });
                            if (route == automatic_routes_.end()) {
                                routing_selected_[index] = false;
                                set_routing_reminder(candidate.executable_path, false);
                                routing_candidates_[index].routed_to_cable = false;
                                state_.append_engine_status(std::format("{} was removed from Termite's routing list.", narrow(candidate.display_name)));
                            } else {
                                std::wstring diagnostic;
                                if (session_router_.restore_route(*route, diagnostic)) {
                                    automatic_routes_.erase(route);
                                    routing_selected_[index] = false;
                                    set_routing_reminder(candidate.executable_path, false);
                                    routing_candidates_[index].routed_to_cable = false;
                                    state_.append_engine_status(std::format("{}: {} Restart playback to move its current stream off CABLE Input.",
                                                                            narrow(candidate.display_name), narrow(diagnostic)));
                                } else {
                                    state_.append_engine_status(std::format("Could not unroute {}: {}", narrow(candidate.display_name), narrow(diagnostic)));
                                }
                            }
                        } else {
                            routing_selected_[index] = true;
                            set_routing_reminder(candidate.executable_path, true);
                        }
                    } else if (row == routing_picker_refresh) {
                        refresh_routing_picker();
                    } else if (row == routing_picker_route) {
                        route_selected_apps();
                    } else if (row == routing_picker_mixer) {
                        session_router::open_manual_routing_settings();
                    } else if (row == routing_picker_close) {
                        routing_picker_open_ = false;
                    }
                }
                routing_picker_pressed_row_ = -1;
                update_pointer(point);
                InvalidateRect(window_, nullptr, FALSE);
                return 0;
            }
            if (filter_menu_band_ >= 0) {
                const auto row = fader_filter_menu_row(to_design(point));
                if (GetCapture() == window_) ReleaseCapture();
                if (row >= 0 && row == filter_menu_pressed_row_) execute_fader_filter_menu_row(row);
                filter_menu_pressed_row_ = -1;
                filter_menu_hot_row_ = -1;
                filter_menu_band_ = -1;
                InvalidateRect(window_, nullptr, FALSE);
                return 0;
            }
            const auto released = console_layout::hit_test(to_design(point), state_.notices().size(), state_.scroll_offset(), state_.active_tab());
            if (GetCapture() == window_) {
                ReleaseCapture();
            }
            if (active_fader_ < 0 && !dragging_scroll_ && pressed_hit_ == released) {
                execute_control(released);
            }
            active_fader_ = -1;
            dragging_scroll_ = false;
            pressed_hit_ = {};
            update_pointer(point);
            return 0;
        }
        case WM_MOUSEWHEEL: {
            POINT screen{};
            GetCursorPos(&screen);
            const auto design = client_to_design_screen(screen);
            if (routing_picker_open_ && console_layout::routing_picker_frame(routing_candidates_.size()).contains(design)) {
                const int wheel_steps = GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA;
                const auto visible = console_layout::routing_picker_visible_rows(routing_candidates_.size());
                const auto maximum = routing_candidates_.size() > visible ? routing_candidates_.size() - visible : 0U;
                const auto requested = static_cast<long long>(routing_picker_first_item_) - wheel_steps;
                routing_picker_first_item_ = static_cast<std::size_t>(std::clamp(requested, 0LL, static_cast<long long>(maximum)));
                InvalidateRect(window_, nullptr, FALSE);
                return 0;
            }
            const auto hit = console_layout::hit_test(design, state_.notices().size(), state_.scroll_offset(), state_.active_tab());
            const int wheel_steps = GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA;
            if (hit.control == console_control::fader_track && hit.index >= 0) {
                if (state_.adjust_fader_q(static_cast<std::size_t>(hit.index), static_cast<float>(wheel_steps) * 0.10F)) {
                    sync_profile();
                }
            } else if (console_layout::status_viewport().contains(design)) {
                state_.set_scroll_offset(state_.scroll_offset() - static_cast<float>(wheel_steps) * 24.0F);
            }
            InvalidateRect(window_, nullptr, FALSE);
            return 0;
        }
        case WM_RBUTTONUP: {
            POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            if (routing_picker_open_) return 0;
            const auto hit = console_layout::hit_test(to_design(point), state_.notices().size(), state_.scroll_offset(), state_.active_tab());
            if ((hit.control == console_control::fader_up || hit.control == console_control::fader_track || hit.control == console_control::fader_down) && hit.index >= 0) {
                show_fader_filter_menu(hit.index, to_design(point));
                return 0;
            }
            return DefWindowProcW(window_, message, wparam, lparam);
        }
        case WM_KEYDOWN:
            if (wparam == VK_ESCAPE) {
                if (diagnostics_popup_open_) {
                    diagnostics_popup_open_ = false;
                    InvalidateRect(window_, nullptr, FALSE);
                } else if (preset_dropdown_open_) {
                    preset_dropdown_open_ = false;
                    preset_dropdown_hot_row_ = -1;
                    preset_dropdown_pressed_row_ = -1;
                    InvalidateRect(window_, nullptr, FALSE);
                } else if (routing_picker_open_) {
                    routing_picker_open_ = false;
                    InvalidateRect(window_, nullptr, FALSE);
                } else {
                    hide_to_tray();
                }
            } else if (wparam == VK_F5) {
                execute_control({console_control::detect});
            }
            return 0;
        case WM_COMMAND:
            if (LOWORD(wparam) == tray_show_command) {
                show_from_tray();
                return 0;
            }
            if (LOWORD(wparam) == tray_quit_command) {
                quitting_ = true;
                save_settings();
                DestroyWindow(window_);
                return 0;
            }
            if (reinterpret_cast<HWND>(lparam) == fader_edit_ && HIWORD(wparam) == EN_KILLFOCUS) {
                finish_fader_edit(true);
                return 0;
            }
            return DefWindowProcW(window_, message, wparam, lparam);
        case WM_CTLCOLOREDIT:
            if (reinterpret_cast<HWND>(lparam) == fader_edit_) {
                const auto device_context = reinterpret_cast<HDC>(wparam);
                SetTextColor(device_context, RGB(242, 239, 201));
                SetBkColor(device_context, RGB(4, 5, 6));
                return reinterpret_cast<LRESULT>(GetStockObject(BLACK_BRUSH));
            }
            return DefWindowProcW(window_, message, wparam, lparam);
        case WM_CLOSE:
            quitting_ = true;
            save_settings();
            DestroyWindow(window_);
            return 0;
        case WM_DESTROY:
            KillTimer(window_, status_timer_id);
            KillTimer(window_, settings_save_timer_id);
            save_settings();
            restore_automatic_routes();
            remove_tray_icon();
            audio_engine_.stop();
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(window_, message, wparam, lparam);
    }
}

void console_window::update_window_region() {
    if (window_ == nullptr) return;
    RECT area{};
    GetClientRect(window_, &area);
    const auto width = area.right - area.left;
    const auto height = area.bottom - area.top;
    if (width <= 0 || height <= 0) return;

    const auto corner_diameter = std::max(6, static_cast<int>(std::lround(scale() * 14.0F)));
    const auto region = CreateRoundRectRgn(0, 0, width + 1, height + 1, corner_diameter, corner_diameter);
    if (region != nullptr) SetWindowRgn(window_, region, TRUE);
}

LRESULT console_window::hit_test_screen(POINT screen_point) const {
    POINT client = screen_point;
    ScreenToClient(window_, &client);
    RECT bounds{};
    GetClientRect(window_, &bounds);
    constexpr LONG resize_margin = 7;
    const bool left = client.x < resize_margin;
    const bool right = client.x >= bounds.right - resize_margin;
    const bool top = client.y < resize_margin;
    const bool bottom = client.y >= bounds.bottom - resize_margin;
    if (top && left) return HTTOPLEFT;
    if (top && right) return HTTOPRIGHT;
    if (bottom && left) return HTBOTTOMLEFT;
    if (bottom && right) return HTBOTTOMRIGHT;
    if (left) return HTLEFT;
    if (right) return HTRIGHT;
    if (top) return HTTOP;
    if (bottom) return HTBOTTOM;

    const auto hit = console_layout::hit_test(to_design(client), state_.notices().size(), state_.scroll_offset(), state_.active_tab());
    return hit.control == console_control::title_drag ? HTCAPTION : HTCLIENT;
}

void console_window::ensure_render_target() {
    if (render_target_ != nullptr || d2d_factory_ == nullptr) {
        return;
    }
    RECT area{};
    GetClientRect(window_, &area);
    const auto size = D2D1::SizeU(std::max(1L, area.right - area.left), std::max(1L, area.bottom - area.top));
    d2d_factory_->CreateHwndRenderTarget(D2D1::RenderTargetProperties(),
                                         D2D1::HwndRenderTargetProperties(window_, size, D2D1_PRESENT_OPTIONS_IMMEDIATELY),
                                         &render_target_);
    if (render_target_ != nullptr) {
        // Layout coordinates are physical canvas pixels. A per-monitor target otherwise
        // applies its own DPI transform on top of the canvas transform.
        render_target_->SetDpi(96.0F, 96.0F);
        skin_->set_target(render_target_.Get());
    }
}

void console_window::discard_render_target() {
    skin_->set_target(nullptr);
    render_target_.Reset();
}

void console_window::render() {
    ensure_render_target();
    if (render_target_ == nullptr) {
        return;
    }
    render_target_->BeginDraw();
    render_target_->SetTransform(D2D1::Matrix3x2F::Identity());
    render_target_->Clear(D2D1::ColorF(0.012F, 0.013F, 0.014F));
    RECT area{};
    GetClientRect(window_, &area);
    const float width = static_cast<float>(std::max(1L, area.right - area.left));
    const float height = static_cast<float>(std::max(1L, area.bottom - area.top));
    render_target_->SetTransform(D2D1::Matrix3x2F::Scale(width / console_design_width, height / console_design_height));
    draw_console();
    render_target_->SetTransform(D2D1::Matrix3x2F::Identity());
    if (render_target_->EndDraw() == D2DERR_RECREATE_TARGET) {
        discard_render_target();
    }
}

void console_window::draw_console() {
    skin_->draw_background({0.0F, 0.0F, console_design_width, console_design_height}, state_.background_index());
    draw_title_and_menu();
    draw_left_bay();
    draw_graph();
    draw_faders();
    draw_bottom_controls();
    draw_preset_dropdown();
    draw_fader_filter_menu();
    draw_routing_picker();
    draw_diagnostics_popup();
}

void console_window::draw_title_and_menu() {
    skin_->draw_title_bar(console_layout::title_bar());
    skin_->draw_panel(console_layout::title_icon());
    skin_->draw_title_icon(console_layout::title_icon());
    skin_->draw_text(L"Termite", console_layout::title_label(), console_text_style::title);

    skin_->draw_caption_button(console_layout::minimize_button(), L"-", visual_state_for({console_control::minimize}, hot_hit_, pressed_hit_));
    skin_->draw_caption_button(console_layout::close_button(), L"X", visual_state_for({console_control::close}, hot_hit_, pressed_hit_));
}

void console_window::draw_left_bay() {
    skin_->draw_group(console_layout::group_rect(console_group::left_bay));
    skin_->draw_text(L"Notification control:", console_layout::notification_label(), console_text_style::label);
    skin_->draw_panel(console_layout::status_viewport());
    const auto& notices = state_.notices();
    const auto scroll = state_.scroll_offset();
    const auto viewport = console_layout::status_viewport();
    render_target_->PushAxisAlignedClip(D2D1::RectF(viewport.x + 4.0F, viewport.y + 4.0F, viewport.right() - 5.0F, viewport.bottom() - 4.0F), D2D1_ANTIALIAS_MODE_ALIASED);
    for (std::size_t index = 0; index < notices.size(); ++index) {
        const auto y = viewport.y + 5.0F + static_cast<float>(index) * 15.0F - scroll;
        skin_->draw_text(notices[index], {viewport.x + 5.0F, y, 228.0F, 14.0F}, console_text_style::label);
    }
    render_target_->PopAxisAlignedClip();
    const auto thumb = console_layout::status_scroll_thumb(notices.size(), scroll);
    skin_->draw_scrollbar(console_layout::status_scroll_track(), thumb, is_hot({console_control::scroll_thumb}), is_pressed({console_control::scroll_thumb}));

    constexpr std::array labels{L"Reconnect", L"Diagnostics", L"Clear log"};
    constexpr std::array controls{
        console_control::detect, console_control::status_sync, console_control::clear_info,
    };
    for (std::size_t index = 0; index < labels.size(); ++index) {
        skin_->draw_button(console_layout::control_rect(controls[index]), labels[index],
                           visual_state_for({controls[index]}, hot_hit_, pressed_hit_, state_.is_selected(controls[index])));
    }

    const auto equalizer = console_layout::group_rect(console_group::equalizer);
    skin_->draw_group(equalizer);
    skin_->draw_text(L"Equalizer", console_layout::equalizer_label(), console_text_style::label, DWRITE_TEXT_ALIGNMENT_CENTER);
    skin_->draw_equalizer_toggle(console_layout::equalizer_toggle(), state_.profile().enabled);

    skin_->draw_group(console_layout::group_rect(console_group::digital_volume));
    skin_->draw_text(L"Digital volume", console_layout::digital_volume_label(), console_text_style::label, DWRITE_TEXT_ALIGNMENT_CENTER);
    skin_->draw_panel(console_layout::digital_volume_display());
    skin_->draw_display_number(std::format(L"{:.0f}", state_.profile().preamp_db), console_layout::digital_volume_display());
    skin_->draw_button(console_layout::control_rect(console_control::volume_up), L"Up", visual_state_for({console_control::volume_up}, hot_hit_, pressed_hit_));
    skin_->draw_button(console_layout::control_rect(console_control::volume_down), L"Down", visual_state_for({console_control::volume_down}, hot_hit_, pressed_hit_));
}

void console_window::draw_graph() {
    const auto graph = console_layout::graph_frame();
    const auto plot = console_layout::graph_plot();
    skin_->draw_panel(graph);
    skin_->draw_text(L"Graphic Equalizer", console_layout::graph_title(), console_text_style::title, DWRITE_TEXT_ALIGNMENT_CENTER);
    skin_->draw_graph_surface(plot);

    auto graph_profile = state_.profile();
    graph_profile.preamp_db = 0.0F;
    Microsoft::WRL::ComPtr<ID2D1PathGeometry> response;
    if (FAILED(d2d_factory_->CreatePathGeometry(&response))) {
        return;
    }
    Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(response->Open(&sink))) {
        return;
    }
    sink->BeginFigure(D2D1::Point2F(plot.x, plot.bottom()), D2D1_FIGURE_BEGIN_FILLED);
    constexpr int points = 260;
    for (int index = 0; index <= points; ++index) {
        const float fraction = static_cast<float>(index) / static_cast<float>(points);
        const float frequency = 30.0F * std::pow(18000.0F / 30.0F, fraction);
        const float gain = clamp(profile_response_db(graph_profile, 48000.0F, frequency), -20.0F, 20.0F);
        const float x = console_layout::graph_x_for_frequency(frequency);
        const float y = plot.y + (20.0F - gain) / 40.0F * plot.height;
        sink->AddLine(D2D1::Point2F(x, y));
    }
    sink->AddLine(D2D1::Point2F(plot.right(), plot.bottom()));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();
    render_target_->PushAxisAlignedClip(D2D1::RectF(plot.x, plot.y, plot.right(), plot.bottom()), D2D1_ANTIALIAS_MODE_ALIASED);
    render_target_->FillGeometry(response.Get(), skin_->brush(D2D1::ColorF(0.0F, 0.83F, 0.09F, 0.88F)));
    render_target_->DrawGeometry(response.Get(), skin_->brush(D2D1::ColorF(0.17F, 1.0F, 0.24F)), 1.0F);
    if (state_.grid_visible()) {
        const auto grid = skin_->brush(D2D1::ColorF(0.49F, 0.53F, 0.56F, 0.62F));
        for (int gain = -20; gain <= 20; gain += 5) {
            const float y = plot.y + (20.0F - static_cast<float>(gain)) / 40.0F * plot.height;
            render_target_->DrawLine(D2D1::Point2F(plot.x, y), D2D1::Point2F(plot.right(), y), grid, gain == 0 ? 1.0F : 0.7F);
        }
        for (std::size_t index = 0; index < graphic_band_count; ++index) {
            const auto track = console_layout::fader_track(index);
            const float x = track.x + track.width * 0.5F;
            render_target_->DrawLine(D2D1::Point2F(x, plot.y), D2D1::Point2F(x, plot.bottom()), grid, 0.7F);
        }
    }
    render_target_->PopAxisAlignedClip();

    const auto y_axis = console_layout::graph_y_axis();
    const auto x_axis = console_layout::graph_x_axis();
    render_target_->DrawLine(D2D1::Point2F(y_axis.x, y_axis.y), D2D1::Point2F(y_axis.x, y_axis.bottom()), skin_->brush(D2D1::ColorF(0.84F, 0.85F, 0.86F)), 0.75F);
    render_target_->DrawLine(D2D1::Point2F(x_axis.x, x_axis.y), D2D1::Point2F(x_axis.right(), x_axis.y), skin_->brush(D2D1::ColorF(0.84F, 0.85F, 0.86F)), 0.75F);
    const auto gain_label = console_layout::graph_gain_label();
    skin_->draw_rotated_text(L"dB Gain", gain_label, -90.0F, {gain_label.x + gain_label.width * 0.5F, gain_label.y + 18.0F}, console_text_style::label);
    skin_->draw_text(L"Frequency, Hz", console_layout::graph_frequency_label(), console_text_style::label, DWRITE_TEXT_ALIGNMENT_CENTER);
    skin_->draw_text(L"20 dB", console_layout::graph_db_label(0), console_text_style::label, DWRITE_TEXT_ALIGNMENT_TRAILING);
    skin_->draw_text(L"0 dB", console_layout::graph_db_label(1), console_text_style::label, DWRITE_TEXT_ALIGNMENT_TRAILING);
    skin_->draw_text(L"-20 dB", console_layout::graph_db_label(2), console_text_style::label, DWRITE_TEXT_ALIGNMENT_TRAILING);
}

void console_window::draw_faders() {
    for (std::size_t index = 0; index < graphic_band_count; ++index) {
        const auto& band = state_.profile().bands[index];
        const auto up = console_layout::fader_up(index);
        const auto down = console_layout::fader_down(index);
        const auto track = console_layout::fader_track(index);
        const auto value = console_layout::fader_value(index);
        const auto fader_hot = hot_hit_.index == static_cast<int>(index) &&
                               (hot_hit_.control == console_control::fader_up || hot_hit_.control == console_control::fader_track || hot_hit_.control == console_control::fader_down);
        const auto fader_pressed = pressed_hit_.index == static_cast<int>(index) && active_fader_ == static_cast<int>(index);
        skin_->draw_text(band_label(band.frequency_hz), console_layout::fader_frequency_label(index), console_text_style::label, DWRITE_TEXT_ALIGNMENT_CENTER);
        skin_->draw_fader(up, track, down, band.gain_db, fader_pressed ? console_visual_state::pressed : fader_hot ? console_visual_state::hot : console_visual_state::normal);
        if (editing_fader_ != static_cast<int>(index)) {
            skin_->draw_panel(value);
            skin_->draw_display_number(gain_label(band.gain_db), value);
        }
    }
}

void console_window::draw_effects_rack() {
    const auto& effects = state_.profile().effects;
    const auto draw_card = [this](std::size_t index, std::wstring_view title, std::wstring_view detail) {
        const auto card = console_layout::effects_card(index);
        skin_->draw_group(card);
        skin_->draw_text(title, {card.x + 10.0F, card.y - 8.0F, card.width - 20.0F, 16.0F}, console_text_style::label, DWRITE_TEXT_ALIGNMENT_CENTER);
        skin_->draw_text(detail, {card.x + 10.0F, card.y + 13.0F, card.width - 20.0F, 17.0F}, console_text_style::label, DWRITE_TEXT_ALIGNMENT_CENTER,
                         DWRITE_PARAGRAPH_ALIGNMENT_CENTER, D2D1::ColorF(0.66F, 0.69F, 0.70F));
    };
    draw_card(0, L"Bass", L"Low shelf  ·  95 Hz");
    draw_card(1, L"Loudness", L"Low + high contour");
    draw_card(2, L"Clarity", L"High shelf  ·  4.5 kHz");
    draw_card(3, L"Stereo", L"Mid / side only");
    draw_card(4, L"Balance", L"Equal-power L / R");

    const auto draw_value = [this](console_control down, console_control up, float value, std::wstring_view suffix) {
        const auto left = console_layout::control_rect(down);
        const auto right = console_layout::control_rect(up);
        const console_rect display{left.right() + 4.0F, left.y, right.x - left.right() - 8.0F, left.height};
        skin_->draw_button(left, L"-", visual_state_for({down}, hot_hit_, pressed_hit_));
        skin_->draw_panel(display);
        skin_->draw_text(std::format(L"{:.0f}{}", value, suffix), display, console_text_style::label, DWRITE_TEXT_ALIGNMENT_CENTER);
        skin_->draw_button(right, L"+", visual_state_for({up}, hot_hit_, pressed_hit_));
    };
    skin_->draw_button(console_layout::control_rect(console_control::effect_bass_toggle), effects.bass_enabled ? L"Enabled" : L"Bypassed",
                       visual_state_for({console_control::effect_bass_toggle}, hot_hit_, pressed_hit_, effects.bass_enabled));
    draw_value(console_control::effect_bass_down, console_control::effect_bass_up, effects.bass_db, L" dB");
    skin_->draw_button(console_layout::control_rect(console_control::effect_loudness_toggle), effects.loudness_enabled ? L"Enabled" : L"Bypassed",
                       visual_state_for({console_control::effect_loudness_toggle}, hot_hit_, pressed_hit_, effects.loudness_enabled));
    draw_value(console_control::effect_loudness_down, console_control::effect_loudness_up, effects.loudness_amount * 100.0F, L"%");
    skin_->draw_button(console_layout::control_rect(console_control::effect_clarity_toggle), effects.clarity_enabled ? L"Enabled" : L"Bypassed",
                       visual_state_for({console_control::effect_clarity_toggle}, hot_hit_, pressed_hit_, effects.clarity_enabled));
    draw_value(console_control::effect_clarity_down, console_control::effect_clarity_up, effects.clarity_db, L" dB");
    skin_->draw_button(console_layout::control_rect(console_control::effect_stereo_toggle), effects.stereo_enabled ? L"Enabled" : L"Bypassed",
                       visual_state_for({console_control::effect_stereo_toggle}, hot_hit_, pressed_hit_, effects.stereo_enabled));
    draw_value(console_control::effect_width_down, console_control::effect_width_up, effects.stereo_width * 100.0F, L"%");
    skin_->draw_checkbox(console_layout::control_rect(console_control::effect_mono), effects.mono, L"Mono");
    skin_->draw_button(console_layout::control_rect(console_control::effect_balance_left), L"L", visual_state_for({console_control::effect_balance_left}, hot_hit_, pressed_hit_));
    skin_->draw_panel({console_layout::effects_card(4).x + 88.0F, console_layout::effects_card(4).y + 48.0F, 84.0F, 28.0F});
    skin_->draw_text(std::format(L"{:+.0f}", effects.balance * 100.0F), {console_layout::effects_card(4).x + 88.0F, console_layout::effects_card(4).y + 48.0F, 84.0F, 28.0F},
                     console_text_style::label, DWRITE_TEXT_ALIGNMENT_CENTER);
    skin_->draw_button(console_layout::control_rect(console_control::effect_balance_right), L"R", visual_state_for({console_control::effect_balance_right}, hot_hit_, pressed_hit_));
    skin_->draw_button(console_layout::control_rect(console_control::effect_reset), L"Reset effects", visual_state_for({console_control::effect_reset}, hot_hit_, pressed_hit_));
    skin_->draw_text(L"Effects are shared across the CABLE Input mix. The master bypass remains dry.",
                     {console_layout::page_rect().x + 40.0F, console_layout::page_rect().y + 285.0F, console_layout::page_rect().width - 80.0F, 20.0F},
                     console_text_style::label, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, D2D1::ColorF(0.72F, 0.74F, 0.75F));
}

void console_window::draw_apps_page() {
    const auto frame = console_layout::apps_list_frame();
    skin_->draw_group(frame);
    skin_->draw_text(L"Open foreground apps", {frame.x + 8.0F, frame.y - 8.0F, frame.width - 16.0F, 16.0F}, console_text_style::label, DWRITE_TEXT_ALIGNMENT_CENTER);
    skin_->draw_text(L"APP                                      WINDOWS     AUDIO     ROUTE", {frame.x + 14.0F, frame.y + 8.0F, frame.width - 28.0F, 17.0F},
                     console_text_style::label, DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, D2D1::ColorF(0.66F, 0.69F, 0.70F));
    if (routing_candidates_.empty()) {
        skin_->draw_panel({frame.x + 8.0F, frame.y + 37.0F, frame.width - 16.0F, 36.0F});
        skin_->draw_text(L"No eligible foreground apps. Press Refresh after opening an app window.", {frame.x + 12.0F, frame.y + 38.0F, frame.width - 24.0F, 34.0F},
                         console_text_style::label, DWRITE_TEXT_ALIGNMENT_CENTER);
    }
    const auto count = std::min<std::size_t>(routing_candidates_.size(), 9);
    for (std::size_t index = 0; index < count; ++index) {
        const auto row = console_layout::apps_row(index);
        const auto& candidate = routing_candidates_[index];
        const bool selected = index < routing_selected_.size() && routing_selected_[index];
        skin_->draw_panel(row, hot_hit_.control == console_control::apps_row && hot_hit_.index == static_cast<int>(index));
        const auto route = candidate.routed_to_cable ? L"Routed" : candidate.active_session_count == 0 ? L"Awaiting audio" : L"Not routed";
        skin_->draw_checkbox({row.x + 3.0F, row.y, row.width * 0.48F, row.height}, selected, candidate.display_name);
        skin_->draw_text(std::format(L"{}", candidate.open_window_count), {row.x + row.width * 0.58F, row.y, 48.0F, row.height}, console_text_style::label, DWRITE_TEXT_ALIGNMENT_CENTER);
        skin_->draw_text(std::format(L"{}", candidate.active_session_count), {row.x + row.width * 0.69F, row.y, 48.0F, row.height}, console_text_style::label, DWRITE_TEXT_ALIGNMENT_CENTER);
        skin_->draw_text(route, {row.x + row.width * 0.78F, row.y, row.width * 0.20F, row.height}, console_text_style::label, DWRITE_TEXT_ALIGNMENT_CENTER);
    }
    skin_->draw_button(console_layout::control_rect(console_control::apps_refresh), L"Refresh", visual_state_for({console_control::apps_refresh}, hot_hit_, pressed_hit_));
    skin_->draw_button(console_layout::control_rect(console_control::apps_route_selected), L"Route selected", visual_state_for({console_control::apps_route_selected}, hot_hit_, pressed_hit_));
    skin_->draw_button(console_layout::control_rect(console_control::apps_return_selected), L"Return selected", visual_state_for({console_control::apps_return_selected}, hot_hit_, pressed_hit_));
    skin_->draw_button(console_layout::control_rect(console_control::apps_open_mixer), L"Volume Mixer", visual_state_for({console_control::apps_open_mixer}, hot_hit_, pressed_hit_));
    skin_->draw_text(L"Routing is a Windows endpoint preference. Termite processes the shared CABLE Input mix once.",
                     {frame.x + 10.0F, frame.bottom() + 53.0F, frame.width - 20.0F, 18.0F}, console_text_style::label, DWRITE_TEXT_ALIGNMENT_CENTER,
                     DWRITE_PARAGRAPH_ALIGNMENT_CENTER, D2D1::ColorF(0.72F, 0.74F, 0.75F));
}

void console_window::draw_monitor_page() {
    const auto snapshot = audio_engine_.monitor();
    const auto draw_spectrum = [this, &snapshot](bool output) {
        const auto frame = console_layout::monitor_spectrum_frame(output);
        skin_->draw_group(frame);
        skin_->draw_text(output ? L"Post EQ spectrum" : L"CABLE input spectrum", {frame.x + 8.0F, frame.y - 8.0F, frame.width - 16.0F, 16.0F}, console_text_style::label, DWRITE_TEXT_ALIGNMENT_CENTER);
        const auto plot = console_rect{frame.x + 8.0F, frame.y + 20.0F, frame.width - 16.0F, frame.height - 30.0F};
        const auto grid = skin_->brush(D2D1::ColorF(0.36F, 0.40F, 0.42F, 0.55F));
        for (int db = -96; db <= 0; db += 24) {
            const auto y = plot.bottom() - (static_cast<float>(db + 96) / 96.0F) * plot.height;
            render_target_->DrawLine(D2D1::Point2F(plot.x, y), D2D1::Point2F(plot.right(), y), grid, 0.6F);
        }
        const auto& spectrum = output ? snapshot.output_spectrum_db : snapshot.input_spectrum_db;
        const auto paint = skin_->brush(output ? D2D1::ColorF(0.12F, 0.83F, 0.24F) : D2D1::ColorF(0.18F, 0.52F, 0.92F));
        for (std::size_t band = 0; band < audio_monitor_band_count; ++band) {
            const float x = plot.x + static_cast<float>(band) / static_cast<float>(audio_monitor_band_count) * plot.width;
            const float width = std::max(1.0F, plot.width / static_cast<float>(audio_monitor_band_count) - 1.0F);
            const float y = plot.bottom() - std::clamp((spectrum[band] + 96.0F) / 96.0F, 0.0F, 1.0F) * plot.height;
            render_target_->FillRectangle(D2D1::RectF(x, y, x + width, plot.bottom()), paint);
        }
    };
    draw_spectrum(false);
    draw_spectrum(true);
    const auto meter = console_layout::monitor_meter_frame();
    skin_->draw_group(meter);
    skin_->draw_text(L"Transport and meters", {meter.x + 10.0F, meter.y - 8.0F, meter.width - 20.0F, 16.0F}, console_text_style::label, DWRITE_TEXT_ALIGNMENT_CENTER);
    const auto diagnostics = audio_engine_.diagnostics();
    const std::array<std::wstring, 5> rows{
        std::format(L"Input  peak L/R {:.2f} / {:.2f}    RMS {:.2f} / {:.2f}", snapshot.input_peak_left, snapshot.input_peak_right, snapshot.input_rms_left, snapshot.input_rms_right),
        std::format(L"Output peak L/R {:.2f} / {:.2f}    RMS {:.2f} / {:.2f}    limiter clamps {}", snapshot.output_peak_left, snapshot.output_peak_right, snapshot.output_rms_left, snapshot.output_rms_right, snapshot.limiter_clamp_count),
        std::format(L"Ring {} / {} frames    xruns capture {} render {}", diagnostics.ring_fill_frames, diagnostics.target_fill_frames, diagnostics.capture_overflows, diagnostics.render_underflows),
        std::format(L"Capture {} Hz / {} ch    Render {} Hz / {} ch", diagnostics.capture_sample_rate, diagnostics.capture_channels, diagnostics.render_sample_rate, diagnostics.render_channels),
        L"State: " + widen(audio_engine_.status_text()) + L"    Recovery: " + widen(diagnostics.recovery_reason.empty() ? "None" : diagnostics.recovery_reason),
    };
    for (std::size_t row = 0; row < rows.size(); ++row) skin_->draw_text(rows[row], {meter.x + 12.0F, meter.y + 15.0F + static_cast<float>(row) * 23.0F, meter.width - 24.0F, 19.0F}, console_text_style::label);
}

void console_window::draw_bottom_controls() {
    const auto draw_labeled_group = [this](console_rect box, std::wstring_view label) {
        const float label_width = std::min(box.width - 18.0F, 13.0F + static_cast<float>(label.size()) * 5.4F);
        const console_rect label_box{box.x + 10.0F, box.y - 9.0F, label_width, 16.0F};
        skin_->draw_group(box);
        skin_->draw_text(label, label_box, console_text_style::label, DWRITE_TEXT_ALIGNMENT_CENTER);
    };
    draw_labeled_group(console_layout::group_rect(console_group::profiles), L"Profiles");
    draw_labeled_group(console_layout::group_rect(console_group::presets), L"Pre-sets");
    draw_labeled_group(console_layout::group_rect(console_group::tone), L"Tone shaping");
    draw_labeled_group(console_layout::group_rect(console_group::stereo), L"Stereo / balance");
    draw_labeled_group(console_layout::group_rect(console_group::termite_control), L"Termite control");

    skin_->draw_button(console_layout::control_rect(console_control::profile_open), L"Open profile", visual_state_for({console_control::profile_open}, hot_hit_, pressed_hit_));
    skin_->draw_button(console_layout::control_rect(console_control::profile_save), L"Save profile", visual_state_for({console_control::profile_save}, hot_hit_, pressed_hit_));
    skin_->draw_button(console_layout::control_rect(console_control::preset_zero), L"All zero", visual_state_for({console_control::preset_zero}, hot_hit_, pressed_hit_));
    skin_->draw_combo_box(console_layout::control_rect(console_control::preset_cycle), state_.preset_label(), preset_dropdown_open_,
                          preset_dropdown_open_ ? console_visual_state::selected
                                                : visual_state_for({console_control::preset_cycle}, hot_hit_, pressed_hit_));
    const auto& effects = state_.profile().effects;
    skin_->draw_button(console_layout::control_rect(console_control::effect_bass_toggle), std::format(L"Bass {:+.0f}", effects.bass_db),
                       visual_state_for({console_control::effect_bass_toggle}, hot_hit_, pressed_hit_, effects.bass_enabled));
    skin_->draw_button(console_layout::control_rect(console_control::effect_loudness_toggle), effects.loudness_enabled ? L"Loudness" : L"Loudness",
                       visual_state_for({console_control::effect_loudness_toggle}, hot_hit_, pressed_hit_, effects.loudness_enabled));
    skin_->draw_button(console_layout::control_rect(console_control::effect_clarity_toggle), std::format(L"Clarity {:+.0f}", effects.clarity_db),
                       visual_state_for({console_control::effect_clarity_toggle}, hot_hit_, pressed_hit_, effects.clarity_enabled));
    skin_->draw_button(console_layout::control_rect(console_control::effect_reset), L"Reset FX", visual_state_for({console_control::effect_reset}, hot_hit_, pressed_hit_));
    skin_->draw_button(console_layout::control_rect(console_control::effect_stereo_toggle), std::format(L"Wide stereo {:.0f}%", effects.stereo_width * 100.0F),
                       visual_state_for({console_control::effect_stereo_toggle}, hot_hit_, pressed_hit_, effects.stereo_enabled));
    skin_->draw_button(console_layout::control_rect(console_control::effect_mono), effects.mono ? L"Mono" : L"Mono",
                       visual_state_for({console_control::effect_mono}, hot_hit_, pressed_hit_, effects.mono));
    skin_->draw_button(console_layout::control_rect(console_control::effect_balance_left), L"L", visual_state_for({console_control::effect_balance_left}, hot_hit_, pressed_hit_));
    skin_->draw_button(console_layout::control_rect(console_control::effect_balance_right), L"R", visual_state_for({console_control::effect_balance_right}, hot_hit_, pressed_hit_));
    skin_->draw_button(console_layout::control_rect(console_control::route_apps), L"Route apps", visual_state_for({console_control::route_apps}, hot_hit_, pressed_hit_));
    skin_->draw_checkbox(console_layout::control_rect(console_control::grid), state_.grid_visible(), L"Grid");
}

void console_window::draw_preset_dropdown() {
    if (!preset_dropdown_open_) return;
    const auto frame = console_layout::preset_dropdown_frame();
    skin_->draw_popup(frame);
    for (std::size_t index = 0; index < console_state::preset_count(); ++index) {
        const auto visual = preset_dropdown_pressed_row_ == static_cast<int>(index) ? console_visual_state::pressed
                          : state_.selected_preset_index() == static_cast<int>(index) ? console_visual_state::selected
                          : preset_dropdown_hot_row_ == static_cast<int>(index) ? console_visual_state::hot
                          : console_visual_state::normal;
        skin_->draw_button(console_layout::preset_dropdown_item(index), console_state::preset_name(index), visual);
    }
}

void console_window::draw_fader_filter_menu() {
    if (filter_menu_band_ < 0 || filter_menu_band_ >= static_cast<int>(graphic_band_count)) return;
    const auto& band = state_.profile().bands[static_cast<std::size_t>(filter_menu_band_)];
    skin_->draw_popup(filter_menu_rect_);
    skin_->draw_panel({filter_menu_rect_.x + 4.0F, filter_menu_rect_.y + 4.0F, filter_menu_rect_.width - 8.0F, filter_menu_header_height}, true);
    skin_->draw_text(std::format(L"BAND {:02}  {:.0f} Hz  Q {:.1f}", filter_menu_band_ + 1, band.frequency_hz, band.q),
                     {filter_menu_rect_.x + 6.0F, filter_menu_rect_.y + 5.0F, filter_menu_rect_.width - 12.0F, filter_menu_header_height - 2.0F},
                     console_text_style::label, DWRITE_TEXT_ALIGNMENT_CENTER);
    constexpr std::array labels{L"PEAK", L"LOW SHELF", L"HIGH SHELF", L"LOW PASS", L"HIGH PASS", L"NOTCH"};
    constexpr std::array shapes{
        filter_shape::peaking, filter_shape::low_shelf, filter_shape::high_shelf,
        filter_shape::low_pass, filter_shape::high_pass, filter_shape::notch,
    };
    for (int row = 0; row < filter_menu_shape_rows; ++row) {
        const auto state = filter_menu_pressed_row_ == row ? console_visual_state::pressed
                           : band.shape == shapes[static_cast<std::size_t>(row)] ? console_visual_state::selected
                           : filter_menu_hot_row_ == row ? console_visual_state::hot
                           : console_visual_state::normal;
        const auto y = filter_menu_rect_.y + filter_menu_header_height + 7.0F + static_cast<float>(row) * filter_menu_row_height;
        skin_->draw_button({filter_menu_rect_.x + 5.0F, y, filter_menu_rect_.width - 10.0F, filter_menu_row_height - 2.0F}, labels[static_cast<std::size_t>(row)], state);
    }
    const auto toggle_y = filter_menu_rect_.y + filter_menu_header_height + 7.0F + static_cast<float>(filter_menu_toggle_row) * filter_menu_row_height;
    const auto toggle_state = filter_menu_pressed_row_ == filter_menu_toggle_row ? console_visual_state::pressed
                            : !band.enabled ? console_visual_state::selected
                            : filter_menu_hot_row_ == filter_menu_toggle_row ? console_visual_state::hot
                            : console_visual_state::normal;
    skin_->draw_button({filter_menu_rect_.x + 5.0F, toggle_y, filter_menu_rect_.width - 10.0F, filter_menu_row_height - 2.0F},
                       band.enabled ? L"BYPASS BAND" : L"ENABLE BAND", toggle_state);
    skin_->draw_text(L"wheel over fader: Q", {filter_menu_rect_.x + 5.0F, filter_menu_rect_.bottom() - 17.0F, filter_menu_rect_.width - 10.0F, 12.0F},
                     console_text_style::label, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, D2D1::ColorF(0.66F, 0.68F, 0.68F));
}

void console_window::draw_routing_picker() {
    if (!routing_picker_open_) return;

    const auto frame = console_layout::routing_picker_frame(routing_candidates_.size());
    skin_->draw_popup(frame);
    skin_->draw_panel({frame.x + 4.0F, frame.y + 4.0F, frame.width - 8.0F, routing_picker_header_height}, true);
    const auto visible = console_layout::routing_picker_visible_rows(routing_candidates_.size());
    const auto last = std::min(routing_candidates_.size(), routing_picker_first_item_ + visible);
    const auto count_text = routing_candidates_.empty()
                                ? L"No eligible open apps"
                                : std::format(L"Route apps  ({}-{} of {})", routing_picker_first_item_ + 1, last, routing_candidates_.size());
    skin_->draw_text(count_text, {frame.x + 8.0F, frame.y + 5.0F, frame.width - 16.0F, routing_picker_header_height - 2.0F},
                     console_text_style::title, DWRITE_TEXT_ALIGNMENT_CENTER);
    skin_->draw_text(L"Select open apps, then apply their CABLE Input route.",
                     {frame.x + 9.0F, frame.y + routing_picker_header_height, frame.width - 18.0F, routing_picker_instruction_height},
                     console_text_style::label, DWRITE_TEXT_ALIGNMENT_CENTER);

    if (routing_candidates_.empty()) {
        const auto row = console_layout::routing_picker_row(0, 0);
        skin_->draw_panel(row);
        skin_->draw_text(L"Open an app with a visible window, then press Refresh.", row, console_text_style::label, DWRITE_TEXT_ALIGNMENT_CENTER);
    } else {
        for (std::size_t visible_index = 0; visible_index < visible && routing_picker_first_item_ + visible_index < routing_candidates_.size(); ++visible_index) {
            const auto index = routing_picker_first_item_ + visible_index;
            const auto row = console_layout::routing_picker_row(routing_candidates_.size(), visible_index);
            if (routing_picker_hot_row_ == static_cast<int>(index)) skin_->draw_panel(row, true);
            const auto& candidate = routing_candidates_[index];
            const auto suffix = std::format(L"  ({} open window{})", candidate.open_window_count,
                                            candidate.open_window_count == 1 ? L"" : L"s");
            skin_->draw_checkbox(row, routing_selected_[index], candidate.display_name + suffix);
        }
    }

    const auto guidance_y = frame.bottom() - routing_picker_footer_height - routing_picker_guidance_height;
    skin_->draw_text(L"Unroute restores the app preference. Restart playback to move an active stream.",
                     {frame.x + 9.0F, guidance_y, frame.width - 18.0F, routing_picker_guidance_height},
                     console_text_style::label, DWRITE_TEXT_ALIGNMENT_CENTER);
    skin_->draw_button(console_layout::routing_picker_refresh_button(routing_candidates_.size()), L"Refresh",
                       routing_picker_pressed_row_ == routing_picker_refresh ? console_visual_state::pressed : console_visual_state::normal);
    skin_->draw_button(console_layout::routing_picker_open_button(routing_candidates_.size()), L"Route selected",
                       routing_picker_pressed_row_ == routing_picker_route ? console_visual_state::pressed : console_visual_state::normal);
    skin_->draw_button(console_layout::routing_picker_mixer_button(routing_candidates_.size()), L"Volume Mixer",
                       routing_picker_pressed_row_ == routing_picker_mixer ? console_visual_state::pressed : console_visual_state::normal);
    skin_->draw_button(console_layout::routing_picker_close_button(routing_candidates_.size()), L"Close",
                       routing_picker_pressed_row_ == routing_picker_close ? console_visual_state::pressed : console_visual_state::normal);
}

void console_window::draw_diagnostics_popup() {
    if (!diagnostics_popup_open_) return;
    const auto frame = console_layout::hardware_diagnostics_frame();
    const auto diagnostics = audio_engine_.diagnostics();
    skin_->draw_popup(frame);
    skin_->draw_panel({frame.x + 4.0F, frame.y + 4.0F, frame.width - 8.0F, 22.0F}, true);
    skin_->draw_text(L"Audio diagnostics  —  click anywhere to close", {frame.x + 8.0F, frame.y + 5.0F, frame.width - 16.0F, 19.0F},
                     console_text_style::title, DWRITE_TEXT_ALIGNMENT_CENTER);
    const auto capture = diagnostics.capture_endpoint_name.empty() ? std::wstring{L"Not connected"} : widen(diagnostics.capture_endpoint_name);
    const auto render = diagnostics.render_endpoint_name.empty() ? std::wstring{L"Not connected"} : widen(diagnostics.render_endpoint_name);
    const auto reason = diagnostics.recovery_reason.empty() ? std::wstring{L"None"} : widen(diagnostics.recovery_reason);
    const std::array<std::wstring, 5> rows{
        L"State: " + widen(audio_engine_.status_text()),
        std::format(L"Capture: {}  ({} Hz, {} ch)", capture, diagnostics.capture_sample_rate, diagnostics.capture_channels),
        std::format(L"Render: {}  ({} Hz, {} ch)", render, diagnostics.render_sample_rate, diagnostics.render_channels),
        std::format(L"Ring: {} / {} frames    xruns: capture {}  render {}    restarts: {}", diagnostics.ring_fill_frames, diagnostics.target_fill_frames,
                    diagnostics.capture_overflows, diagnostics.render_underflows, diagnostics.restart_count),
        L"Recovery: " + reason,
    };
    for (std::size_t index = 0; index < rows.size(); ++index) {
        skin_->draw_text(rows[index], {frame.x + 11.0F, frame.y + 32.0F + static_cast<float>(index) * 25.0F, frame.width - 22.0F, 20.0F},
                         console_text_style::label);
    }
}

void console_window::update_pointer(POINT client_point) {
    const auto design = to_design(client_point);
    if (preset_dropdown_open_) {
        const auto row = preset_dropdown_row(design);
        if (row != preset_dropdown_hot_row_) {
            preset_dropdown_hot_row_ = row;
            InvalidateRect(window_, nullptr, FALSE);
        }
        if (!tracking_mouse_) {
            TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, window_, 0};
            TrackMouseEvent(&tracking);
            tracking_mouse_ = true;
        }
        return;
    }
    if (routing_picker_open_) {
        const auto row = routing_picker_row(design);
        if (row != routing_picker_hot_row_) {
            routing_picker_hot_row_ = row;
            InvalidateRect(window_, nullptr, FALSE);
        }
        if (!tracking_mouse_) {
            TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, window_, 0};
            TrackMouseEvent(&tracking);
            tracking_mouse_ = true;
        }
        return;
    }
    if (filter_menu_band_ >= 0) {
        const auto row = fader_filter_menu_row(design);
        if (row != filter_menu_hot_row_) {
            filter_menu_hot_row_ = row;
            InvalidateRect(window_, nullptr, FALSE);
        }
        if (!tracking_mouse_) {
            TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, window_, 0};
            TrackMouseEvent(&tracking);
            tracking_mouse_ = true;
        }
        return;
    }
    const auto next_hit = console_layout::hit_test(design, state_.notices().size(), state_.scroll_offset(), state_.active_tab());
    const bool hot_changed = next_hit != hot_hit_;
    hot_hit_ = next_hit;
    if (!tracking_mouse_) {
        TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, window_, 0};
        TrackMouseEvent(&tracking);
        tracking_mouse_ = true;
    }
    if (hot_changed) {
        InvalidateRect(window_, nullptr, FALSE);
    }
}

void console_window::execute_control(console_hit hit) {
    if (hit.control == console_control::none || hit.control == console_control::title_drag || hit.control == console_control::scroll_thumb) {
        return;
    }
    if (hit.control == console_control::fader_up || hit.control == console_control::fader_down) {
        const float delta = hit.control == console_control::fader_up ? 0.1F : -0.1F;
        if (state_.adjust_fader_gain(static_cast<std::size_t>(hit.index), delta)) {
            sync_profile();
        }
        InvalidateRect(window_, nullptr, FALSE);
        return;
    }
    if (hit.control == console_control::profile_open) {
        open_profile_file();
        return;
    }
    if (hit.control == console_control::profile_save) {
        save_profile_file();
        return;
    }
    if (hit.control == console_control::apps_refresh) {
        refresh_routing_picker();
        InvalidateRect(window_, nullptr, FALSE);
        return;
    }
    if (hit.control == console_control::apps_route_selected) {
        route_selected_apps();
        schedule_settings_save();
        return;
    }
    if (hit.control == console_control::apps_return_selected) {
        return_selected_apps();
        schedule_settings_save();
        return;
    }
    if (hit.control == console_control::apps_open_mixer) {
        session_router::open_manual_routing_settings();
        return;
    }
    if (hit.control == console_control::apps_row && hit.index >= 0 && static_cast<std::size_t>(hit.index) < routing_selected_.size()) {
        const auto index = static_cast<std::size_t>(hit.index);
        routing_selected_[index] = !routing_selected_[index];
        set_routing_reminder(routing_candidates_[index].executable_path, routing_selected_[index]);
        schedule_settings_save();
        InvalidateRect(window_, nullptr, FALSE);
        return;
    }

    const auto result = state_.activate(hit.control);
    if (hit.control == console_control::tab_apps) refresh_routing_picker();
    if (result.profile_changed) {
        sync_profile();
    }
    if (result.restart_audio) {
        audio_engine_.stop();
        if (audio_engine_.start()) {
            state_.append_engine_status("Audio restart requested.");
        }
        append_audio_status();
    }
    if (result.request_engine_status) {
        append_audio_status();
    }
    if (result.open_routing) {
        show_routing_picker();
    }
    if (result.open_diagnostics) {
        show_diagnostics();
    }
    if (result.minimize) {
        ShowWindow(window_, SW_MINIMIZE);
    }
    if (result.close) {
        quitting_ = true;
        save_settings();
        DestroyWindow(window_);
        return;
    }
    schedule_settings_save();
    InvalidateRect(window_, nullptr, FALSE);
}

void console_window::update_fader_from_point(int index, console_point point) {
    const auto track = console_layout::fader_track(static_cast<std::size_t>(index));
    const float thumb_top = track.y + 8.0F;
    const float thumb_bottom = track.bottom() - 8.0F;
    const float ratio = clamp((point.y - thumb_top) / (thumb_bottom - thumb_top), 0.0F, 1.0F);
    const float gain = console_layout::snap_fader_gain(20.0F - ratio * 40.0F);
    if (state_.set_fader_gain(static_cast<std::size_t>(index), gain)) {
        sync_profile();
        InvalidateRect(window_, nullptr, FALSE);
    }
}

void console_window::begin_fader_edit(int index) {
    if (index < 0 || index >= static_cast<int>(graphic_band_count)) return;
    if (editing_fader_ == index && fader_edit_ != nullptr) {
        SetFocus(fader_edit_);
        SendMessageW(fader_edit_, EM_SETSEL, 0, -1);
        return;
    }
    finish_fader_edit(true);

    editing_fader_ = index;
    const auto label = gain_label(state_.profile().bands[static_cast<std::size_t>(index)].gain_db);
    fader_edit_ = CreateWindowExW(0,
                                  L"EDIT",
                                  label.c_str(),
                                  WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER | ES_AUTOHSCROLL,
                                  0,
                                  0,
                                  1,
                                  1,
                                  window_,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(fader_edit_control_id)),
                                  instance_,
                                  nullptr);
    if (fader_edit_ == nullptr) {
        editing_fader_ = -1;
        return;
    }

    SetWindowLongPtrW(fader_edit_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    fader_edit_original_proc_ = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(fader_edit_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&console_window::fader_edit_proc)));
    SendMessageW(fader_edit_, EM_SETLIMITTEXT, 5, 0);
    SendMessageW(fader_edit_, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    position_fader_edit();
    SetFocus(fader_edit_);
    SendMessageW(fader_edit_, EM_SETSEL, 0, -1);
    InvalidateRect(window_, nullptr, FALSE);
}

void console_window::finish_fader_edit(bool commit) {
    if (editing_fader_ < 0) return;

    const int index = editing_fader_;
    if (commit && fader_edit_ != nullptr) {
        std::array<wchar_t, 16> text{};
        GetWindowTextW(fader_edit_, text.data(), static_cast<int>(text.size()));
        wchar_t* end{};
        const float gain = std::wcstof(text.data(), &end);
        if (end != text.data() && *end == L'\0' && std::isfinite(gain) && state_.set_fader_gain(static_cast<std::size_t>(index), gain)) {
            sync_profile();
        }
    }

    const auto editor = fader_edit_;
    fader_edit_ = nullptr;
    fader_edit_original_proc_ = nullptr;
    editing_fader_ = -1;
    if (editor != nullptr) {
        DestroyWindow(editor);
    }
    InvalidateRect(window_, nullptr, FALSE);
}

void console_window::position_fader_edit() {
    if (fader_edit_ == nullptr || editing_fader_ < 0) return;
    const auto bounds = console_layout::fader_value(static_cast<std::size_t>(editing_fader_));
    RECT client{};
    GetClientRect(window_, &client);
    const float width = static_cast<float>(std::max(1L, client.right - client.left));
    const float height = static_cast<float>(std::max(1L, client.bottom - client.top));
    SetWindowPos(fader_edit_,
                 HWND_TOP,
                 static_cast<int>(std::lround(bounds.x * width / console_design_width)),
                 static_cast<int>(std::lround(bounds.y * height / console_design_height)),
                 std::max(1, static_cast<int>(std::lround(bounds.width * width / console_design_width))),
                 std::max(1, static_cast<int>(std::lround(bounds.height * height / console_design_height))),
                 SWP_NOACTIVATE);
}

void console_window::show_fader_filter_menu(int band, console_point anchor) {
    if (band < 0 || band >= static_cast<int>(graphic_band_count)) return;
    constexpr float width = 150.0F;
    constexpr float height = filter_menu_header_height + 7.0F + filter_menu_row_height * 7.0F + 21.0F;
    filter_menu_band_ = band;
    preset_dropdown_open_ = false;
    filter_menu_pressed_row_ = -1;
    filter_menu_hot_row_ = -1;
    filter_menu_rect_.x = clamp(anchor.x + 9.0F, 282.0F, console_design_width - width - 8.0F);
    filter_menu_rect_.y = clamp(anchor.y + 7.0F, 53.0F, console_design_height - height - 8.0F);
    filter_menu_rect_.width = width;
    filter_menu_rect_.height = height;
    InvalidateRect(window_, nullptr, FALSE);
}

void console_window::execute_fader_filter_menu_row(int row) {
    if (filter_menu_band_ < 0 || filter_menu_band_ >= static_cast<int>(graphic_band_count)) return;
    constexpr std::array shapes{
        filter_shape::peaking, filter_shape::low_shelf, filter_shape::high_shelf,
        filter_shape::low_pass, filter_shape::high_pass, filter_shape::notch,
    };
    bool changed = false;
    if (row >= 0 && row < filter_menu_shape_rows) {
        changed = state_.set_fader_shape(static_cast<std::size_t>(filter_menu_band_), shapes[static_cast<std::size_t>(row)]);
    } else if (row == filter_menu_toggle_row) {
        const auto enabled = state_.profile().bands[static_cast<std::size_t>(filter_menu_band_)].enabled;
        changed = state_.set_fader_enabled(static_cast<std::size_t>(filter_menu_band_), !enabled);
    }
    if (changed) sync_profile();
}

int console_window::fader_filter_menu_row(console_point point) const noexcept {
    if (filter_menu_band_ < 0 || !filter_menu_rect_.contains(point)) return -1;
    const float first_row = filter_menu_rect_.y + filter_menu_header_height + 7.0F;
    if (point.y < first_row) return -1;
    const auto row = static_cast<int>((point.y - first_row) / filter_menu_row_height);
    if (row < 0 || row > filter_menu_toggle_row) return -1;
    const float row_y = first_row + static_cast<float>(row) * filter_menu_row_height;
    return point.y < row_y + filter_menu_row_height - 2.0F ? row : -1;
}

int console_window::preset_dropdown_row(console_point point) const noexcept {
    if (!preset_dropdown_open_ || !console_layout::preset_dropdown_frame().contains(point)) return -1;
    for (std::size_t index = 0; index < console_state::preset_count(); ++index) {
        if (console_layout::preset_dropdown_item(index).contains(point)) return static_cast<int>(index);
    }
    return -1;
}

int console_window::routing_picker_row(console_point point) const noexcept {
    if (!routing_picker_open_) return -1;
    const auto frame = console_layout::routing_picker_frame(routing_candidates_.size());
    if (!frame.contains(point)) return -1;
    if (console_layout::routing_picker_refresh_button(routing_candidates_.size()).contains(point)) return routing_picker_refresh;
    if (console_layout::routing_picker_open_button(routing_candidates_.size()).contains(point)) return routing_picker_route;
    if (console_layout::routing_picker_mixer_button(routing_candidates_.size()).contains(point)) return routing_picker_mixer;
    if (console_layout::routing_picker_close_button(routing_candidates_.size()).contains(point)) return routing_picker_close;
    const auto visible = console_layout::routing_picker_visible_rows(routing_candidates_.size());
    for (std::size_t visible_index = 0; visible_index < visible; ++visible_index) {
        if (console_layout::routing_picker_row(routing_candidates_.size(), visible_index).contains(point)) {
            const auto index = routing_picker_first_item_ + visible_index;
            return index < routing_candidates_.size() ? static_cast<int>(index) : -1;
        }
    }
    return -1;
}

void console_window::update_scroll_from_point(console_point point) {
    const auto track = console_layout::status_scroll_track();
    const auto thumb = console_layout::status_scroll_thumb(state_.notices().size(), state_.scroll_offset());
    const auto content = static_cast<float>(state_.notices().size()) * 15.0F;
    const auto visible = console_layout::status_viewport().height - 8.0F;
    const auto max_scroll = std::max(0.0F, content - visible);
    constexpr float arrow_height = 16.0F;
    const auto channel_height = track.height - arrow_height * 2.0F;
    const auto extent = std::max(1.0F, channel_height - thumb.height);
    const auto position = clamp(point.y - scroll_drag_offset_ - (track.y + arrow_height), 0.0F, extent);
    state_.set_scroll_offset(max_scroll * position / extent);
    InvalidateRect(window_, nullptr, FALSE);
}

void console_window::sync_profile() {
    audio_engine_.set_profile(state_.profile());
    schedule_settings_save();
}

void console_window::open_profile_file() {
    std::wstring failure_reason;
    const auto path = choose_profile_file(window_, false, failure_reason);
    if (!path.has_value()) {
        if (!failure_reason.empty()) state_.append_engine_status(narrow(failure_reason));
        return;
    }
    const auto loaded = settings_store::load_profile_file(*path);
    if (!loaded.loaded) {
        state_.append_engine_status(narrow(loaded.notice));
        InvalidateRect(window_, nullptr, FALSE);
        return;
    }
    state_.set_profile(loaded.profile);
    sync_profile();
    state_.append_engine_status(std::format("Loaded profile: {}", narrow(path->filename().wstring())));
    InvalidateRect(window_, nullptr, FALSE);
}

void console_window::save_profile_file() {
    std::wstring failure_reason;
    const auto path = choose_profile_file(window_, true, failure_reason);
    if (!path.has_value()) {
        if (!failure_reason.empty()) state_.append_engine_status(narrow(failure_reason));
        return;
    }
    if (!settings_store::save_profile_file(*path, state_.profile(), failure_reason)) {
        state_.append_engine_status(narrow(failure_reason));
    } else {
        state_.append_engine_status(std::format("Saved profile: {}", narrow(path->filename().wstring())));
    }
    InvalidateRect(window_, nullptr, FALSE);
}

void console_window::show_routing_picker() {
    filter_menu_band_ = -1;
    preset_dropdown_open_ = false;
    routing_picker_open_ = true;
    routing_picker_pressed_row_ = -1;
    routing_picker_hot_row_ = -1;
    refresh_routing_picker();
}

void console_window::refresh_routing_picker() {
    routing_candidates_ = session_router_.open_apps();
    routing_selected_.assign(routing_candidates_.size(), false);
    for (std::size_t index = 0; index < routing_candidates_.size(); ++index) {
        routing_selected_[index] = std::any_of(routing_reminders_.begin(), routing_reminders_.end(), [this, index](const std::wstring& selected) {
            return _wcsicmp(selected.c_str(), routing_candidates_[index].executable_path.c_str()) == 0;
        });
    }
    const auto visible = console_layout::routing_picker_visible_rows(routing_candidates_.size());
    const auto maximum = routing_candidates_.size() > visible ? routing_candidates_.size() - visible : 0U;
    routing_picker_first_item_ = std::min(routing_picker_first_item_, maximum);
    state_.append_engine_status(std::format("{} eligible open app(s) found.", routing_candidates_.size()));
}

void console_window::route_selected_apps() {
    std::size_t selected{};
    std::size_t routed{};
    for (std::size_t index = 0; index < routing_candidates_.size() && index < routing_selected_.size(); ++index) {
        if (!routing_selected_[index]) continue;
        ++selected;
        app_audio_route_snapshot previous_route;
        std::wstring diagnostic;
        if (session_router_.route_to_cable(routing_candidates_[index], previous_route, diagnostic)) {
            const auto already_tracked = std::any_of(automatic_routes_.begin(), automatic_routes_.end(), [&previous_route](const app_audio_route_snapshot& existing) {
                return _wcsicmp(existing.executable_path.c_str(), previous_route.executable_path.c_str()) == 0;
            });
            if (!already_tracked) automatic_routes_.push_back(std::move(previous_route));
            routing_candidates_[index].routed_to_cable = true;
            ++routed;
            state_.append_engine_status(std::format("{}: {}", narrow(routing_candidates_[index].display_name), narrow(diagnostic)));
        } else {
            state_.append_engine_status(std::format("{} could not be routed automatically: {}. Use Volume Mixer.",
                                                     narrow(routing_candidates_[index].display_name), narrow(diagnostic)));
        }
    }
    if (selected == 0) {
        state_.append_engine_status("Select one or more open apps before routing them.");
        return;
    }
    if (routed > 0) {
        state_.append_engine_status("CABLE Input mixes the routed apps. Restart playback if an app already had an active stream.");
    }
    InvalidateRect(window_, nullptr, FALSE);
}

void console_window::return_selected_apps() {
    std::size_t returned{};
    for (std::size_t index = 0; index < routing_candidates_.size() && index < routing_selected_.size(); ++index) {
        if (!routing_selected_[index]) continue;
        const auto& candidate = routing_candidates_[index];
        const auto route = std::find_if(automatic_routes_.begin(), automatic_routes_.end(), [&candidate](const app_audio_route_snapshot& existing) {
            return _wcsicmp(existing.executable_path.c_str(), candidate.executable_path.c_str()) == 0;
        });
        if (route == automatic_routes_.end()) {
            routing_selected_[index] = false;
            set_routing_reminder(candidate.executable_path, false);
            continue;
        }
        std::wstring diagnostic;
        if (session_router_.restore_route(*route, diagnostic)) {
            automatic_routes_.erase(route);
            routing_selected_[index] = false;
            set_routing_reminder(candidate.executable_path, false);
            state_.append_engine_status(std::format("{}: {}", narrow(candidate.display_name), narrow(diagnostic)));
            ++returned;
        } else {
            state_.append_engine_status(std::format("Could not return {}: {}", narrow(candidate.display_name), narrow(diagnostic)));
        }
    }
    refresh_routing_picker();
    state_.append_engine_status(returned == 0 ? "No selected app had a Termite route to return." : "Selected app routes returned to their previous Windows output.");
    InvalidateRect(window_, nullptr, FALSE);
}

void console_window::restore_automatic_routes() {
    for (const auto& route : automatic_routes_) {
        std::wstring diagnostic;
        if (!session_router_.restore_route(route, diagnostic)) {
            state_.append_engine_status(std::format("Could not restore {}: {}", narrow(route.executable_path), narrow(diagnostic)));
        }
    }
    automatic_routes_.clear();
}

void console_window::append_audio_status() {
    const auto status = audio_engine_.status_text();
    if (status != last_audio_status_) {
        last_audio_status_ = status;
        state_.append_engine_status(status);
        InvalidateRect(window_, nullptr, FALSE);
    }
}

void console_window::show_diagnostics() {
    filter_menu_band_ = -1;
    preset_dropdown_open_ = false;
    routing_picker_open_ = false;
    diagnostics_popup_open_ = true;
    InvalidateRect(window_, nullptr, FALSE);
}

void console_window::load_settings() {
    const auto loaded = settings_store_.load();
    if (loaded.loaded) {
        state_.restore_persistent_state(loaded.settings.console);
        routing_reminders_ = loaded.settings.routing_executables;
        restored_window_bounds_ = loaded.settings.window;
    } else if (!loaded.notice.empty()) {
        state_.append_engine_status(narrow(loaded.notice));
    }
}

void console_window::schedule_settings_save() {
    settings_dirty_ = true;
    if (window_ != nullptr && !quitting_) {
        SetTimer(window_, settings_save_timer_id, 700, nullptr);
    }
}

void console_window::save_settings() {
    if (!settings_dirty_) return;
    termite_settings settings;
    settings.console = state_.persistent_state();
    settings.routing_executables = routing_reminders_;
    if (window_ != nullptr) {
        RECT bounds{};
        if (GetWindowRect(window_, &bounds)) {
            settings.window = {bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top, true};
        }
    } else {
        settings.window = restored_window_bounds_;
    }
    std::wstring failure;
    if (!settings_store_.save(settings, failure)) {
        state_.append_engine_status(narrow(failure));
    }
    settings_dirty_ = false;
}

void console_window::create_tray_icon() {
    if (window_ == nullptr || title_icon_ == nullptr || tray_icon_visible_) return;
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = window_;
    data.uID = tray_icon_id;
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    data.uCallbackMessage = tray_callback_message;
    data.hIcon = title_icon_;
    wcsncpy_s(data.szTip, L"Termite — selected-app EQ", _TRUNCATE);
    tray_icon_visible_ = Shell_NotifyIconW(NIM_ADD, &data) != FALSE;
    if (tray_icon_visible_) {
        data.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &data);
    }
}

void console_window::remove_tray_icon() {
    if (!tray_icon_visible_ || window_ == nullptr) return;
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = window_;
    data.uID = tray_icon_id;
    Shell_NotifyIconW(NIM_DELETE, &data);
    tray_icon_visible_ = false;
}

void console_window::show_from_tray() {
    ShowWindow(window_, IsIconic(window_) ? SW_RESTORE : SW_SHOW);
    SetForegroundWindow(window_);
}

void console_window::hide_to_tray() {
    if (editing_fader_ >= 0) finish_fader_edit(true);
    ShowWindow(window_, SW_HIDE);
    schedule_settings_save();
}

void console_window::set_routing_reminder(const std::wstring& executable, bool selected) {
    const auto found = std::find_if(routing_reminders_.begin(), routing_reminders_.end(), [&executable](const std::wstring& existing) {
        return _wcsicmp(existing.c_str(), executable.c_str()) == 0;
    });
    if (selected && found == routing_reminders_.end()) {
        routing_reminders_.push_back(executable);
    } else if (!selected && found != routing_reminders_.end()) {
        routing_reminders_.erase(found);
    }
    schedule_settings_save();
}

console_point console_window::to_design(POINT client_point) const noexcept {
    RECT area{};
    GetClientRect(window_, &area);
    const float width = static_cast<float>(std::max(1L, area.right - area.left));
    const float height = static_cast<float>(std::max(1L, area.bottom - area.top));
    return {static_cast<float>(client_point.x) * console_design_width / width,
            static_cast<float>(client_point.y) * console_design_height / height};
}

console_point console_window::client_to_design_screen(POINT screen_point) const noexcept {
    ScreenToClient(window_, &screen_point);
    return to_design(screen_point);
}

float console_window::scale() const noexcept {
    RECT area{};
    GetClientRect(window_, &area);
    const auto width = static_cast<float>(std::max(1L, area.right - area.left));
    const auto height = static_cast<float>(std::max(1L, area.bottom - area.top));
    return std::min(width / console_design_width, height / console_design_height);
}

POINT console_window::canvas_origin() const noexcept {
    RECT area{};
    GetClientRect(window_, &area);
    const auto canvas_scale = scale();
    const auto width = static_cast<float>(area.right - area.left);
    const auto height = static_cast<float>(area.bottom - area.top);
    return {static_cast<LONG>(std::lround((width - console_design_width * canvas_scale) * 0.5F)),
            static_cast<LONG>(std::lround((height - console_design_height * canvas_scale) * 0.5F))};
}

bool console_window::is_hot(console_hit hit) const noexcept {
    return hit == hot_hit_;
}

bool console_window::is_pressed(console_hit hit) const noexcept {
    return hit == pressed_hit_;
}

}  // namespace termite
