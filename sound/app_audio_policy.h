#pragma once

#include <string>
#include <vector>

namespace termite {

// A snapshot of the two render roles that Windows' Volume Mixer changes for a
// single process. Empty values mean the process used the system default.
struct app_audio_process_route_snapshot {
    unsigned long process_id{};
    std::wstring console_endpoint_id;
    std::wstring multimedia_endpoint_id;
    bool had_console_endpoint{};
    bool had_multimedia_endpoint{};
};

// An application can have several audio-rendering processes. Preserve every
// process's original route rather than accidentally restoring all of them to
// the first process's route.
struct app_audio_route_snapshot {
    std::wstring executable_path;
    std::vector<app_audio_process_route_snapshot> processes;
};

class app_audio_policy {
public:
    // Applies the same per-app render-endpoint preference that the Windows
    // Volume Mixer stores.  This is an internal Windows policy interface, so
    // every call returns a useful diagnostic and leaves the manual mixer
    // fallback available if a future Windows release removes it.
    [[nodiscard]] bool route_executable_to_cable(const std::wstring& executable_path,
                                                 app_audio_route_snapshot& previous_route,
                                                 std::wstring& diagnostic) const;
    [[nodiscard]] bool restore_executable_route(const app_audio_route_snapshot& previous_route,
                                                std::wstring& diagnostic) const;
    [[nodiscard]] bool is_executable_routed_to_cable(const std::wstring& executable_path) const;
};

}  // namespace termite
