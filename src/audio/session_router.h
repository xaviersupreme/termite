#pragma once

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
    std::size_t active_session_count{};
    bool routed_to_cable{};
};

class session_router {
public:
    [[nodiscard]] std::vector<audio_session_info> active_sessions() const;
    [[nodiscard]] std::vector<routing_candidate> eligible_sessions() const;
    [[nodiscard]] bool route_to_cable(const audio_session_info& session, std::wstring& failure_reason) const;
    static void open_manual_routing_settings();
};

}  // namespace termite
