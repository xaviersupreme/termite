#pragma once

#include "audio/app_audio_policy.h"

#include <string>
#include <vector>

namespace termite {

struct audio_session_info {
    unsigned long process_id{};
    std::wstring executable_path;
    std::wstring display_name;
    bool routed_to_cable{};
};

struct routing_candidate {
    std::wstring executable_path;
    std::wstring display_name;
    std::size_t open_window_count{};
    bool routed_to_cable{};
};

class session_router {
public:
    [[nodiscard]] std::vector<audio_session_info> active_sessions() const;
    [[nodiscard]] std::vector<routing_candidate> open_apps() const;
    // Retained for existing callers. Candidates are now discovered from open app windows,
    // rather than active audio sessions.
    [[nodiscard]] std::vector<routing_candidate> eligible_sessions() const;
    [[nodiscard]] bool route_to_cable(const audio_session_info& session, std::wstring& failure_reason) const;
    [[nodiscard]] bool route_to_cable(const routing_candidate& candidate,
                                      app_audio_route_snapshot& previous_route,
                                      std::wstring& diagnostic) const;
    [[nodiscard]] bool restore_route(const app_audio_route_snapshot& previous_route,
                                     std::wstring& diagnostic) const;
    static void open_manual_routing_settings();
};

}  // namespace termite
