#pragma once

#include <string>
#include <vector>

namespace termite {



struct app_audio_process_route_snapshot {
    unsigned long process_id{};
    std::wstring console_endpoint_id;
    std::wstring multimedia_endpoint_id;
    bool had_console_endpoint{};
    bool had_multimedia_endpoint{};
};




struct app_audio_route_snapshot {
    std::wstring executable_path;
    std::vector<app_audio_process_route_snapshot> processes;
};

class app_audio_policy {
public:




    [[nodiscard]] bool route_executable_to_cable(const std::wstring& executable_path,
                                                 app_audio_route_snapshot& previous_route,
                                                 std::wstring& diagnostic) const;
    [[nodiscard]] bool restore_executable_route(const app_audio_route_snapshot& previous_route,
                                                std::wstring& diagnostic) const;
    [[nodiscard]] bool is_executable_routed_to_cable(const std::wstring& executable_path) const;
};

}  // namespace termite
