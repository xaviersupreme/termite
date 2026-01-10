#include "audio/routing_policy.h"

#include <algorithm>
#include <array>
#include <cwctype>
#include <filesystem>

namespace termite {
namespace {

std::wstring lower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t character) {
        return static_cast<wchar_t>(std::towlower(character));
    });
    return value;
}

bool has_system_path(const std::wstring& path) {
    const auto value = lower(path);
    return value.find(L"\\windows\\") != std::wstring::npos ||
           value.find(L"\\programdata\\") != std::wstring::npos;
}

bool is_background_window_host(const std::wstring& executable_path) {
    const auto name = lower(std::filesystem::path(executable_path).filename().wstring());
    constexpr std::array<std::wstring_view, 7> names{
        L"applicationframehost.exe", L"textinputhost.exe", L"searchhost.exe", L"shellexperiencehost.exe",
        L"startmenuexperiencehost.exe", L"widgetservice.exe", L"lockapp.exe",
    };
    return std::find(names.begin(), names.end(), name) != names.end();
}

}  // namespace

bool is_known_user_facing_executable(const std::wstring& executable_path) {
    const auto name = lower(std::filesystem::path(executable_path).filename().wstring());
    constexpr std::array<std::wstring_view, 13> names{
        L"chrome.exe", L"msedge.exe", L"firefox.exe", L"brave.exe", L"opera.exe", L"spotify.exe", L"discord.exe",
        L"vlc.exe", L"teams.exe", L"slack.exe", L"zoom.exe", L"winamp.exe", L"foobar2000.exe",
    };
    return std::find(names.begin(), names.end(), name) != names.end();
}

bool is_eligible_routing_process(const routing_process_metadata& process) {
    return process.process_id != 0 && process.active_audio && !process.system_sounds && process.current_user_session &&
           !process.executable_path.empty() && !has_system_path(process.executable_path) &&
           (process.has_visible_window || is_known_user_facing_executable(process.executable_path));
}

bool is_eligible_open_app(const open_app_metadata& app) {
    return app.process_id != 0 && app.current_user_session && app.has_visible_top_level_window &&
           !app.executable_path.empty() && !is_background_window_host(app.executable_path);
}

std::wstring normalized_executable_key(const std::wstring& executable_path) {
    return lower(executable_path);
}

std::wstring executable_display_name(const std::wstring& executable_path) {
    const auto stem = std::filesystem::path(executable_path).stem().wstring();
    return stem.empty() ? executable_path : stem;
}

}  // namespace termite
