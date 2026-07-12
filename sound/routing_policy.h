#pragma once

#include <string>

namespace termite {

struct routing_process_metadata {
    unsigned long process_id{};
    bool active_audio{};
    bool system_sounds{};
    bool current_user_session{};
    bool has_visible_window{};
    std::wstring executable_path;
};

struct open_app_metadata {
    unsigned long process_id{};
    bool current_user_session{};
    bool has_visible_top_level_window{};
    std::wstring executable_path;
};

[[nodiscard]] bool is_known_user_facing_executable(const std::wstring& executable_path);
[[nodiscard]] bool is_eligible_routing_process(const routing_process_metadata& process);
[[nodiscard]] bool is_eligible_open_app(const open_app_metadata& app);
[[nodiscard]] std::wstring normalized_executable_key(const std::wstring& executable_path);
[[nodiscard]] std::wstring executable_display_name(const std::wstring& executable_path);

}  // namespace termite
