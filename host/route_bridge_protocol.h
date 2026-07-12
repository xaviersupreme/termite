#pragma once

#include <cstdint>
#include <type_traits>

#include <windows.h>

namespace termite {

// A second, deliberately small local pipe for the parts of the frontend that
// need an answer from the host.  EQ updates are fire-and-forget; app routing
// must always return the host's current, authoritative Windows route state.
inline constexpr wchar_t route_bridge_pipe_name[] = L"\\\\.\\pipe\\termite.route.v1";
inline constexpr std::uint32_t route_bridge_magic = 0x5452544DU;  // "MTRT"
inline constexpr std::uint16_t route_bridge_version = 1;
inline constexpr std::size_t route_bridge_max_apps = 48;
inline constexpr std::size_t route_bridge_path_chars = 520;
inline constexpr std::size_t route_bridge_display_chars = 128;
inline constexpr std::size_t route_bridge_message_chars = 256;

enum class route_bridge_command : std::uint8_t { list_apps = 0, set_route = 1 };
enum class route_bridge_status : std::uint32_t { accepted = 0, rejected = 1, failed = 2 };

#pragma pack(push, 1)
struct route_bridge_request_v1 {
    std::uint32_t magic{route_bridge_magic};
    std::uint16_t version{route_bridge_version};
    std::uint16_t bytes{sizeof(route_bridge_request_v1)};
    std::uint32_t sequence{};
    std::uint8_t command{static_cast<std::uint8_t>(route_bridge_command::list_apps)};
    std::uint8_t route_to_cable{};
    std::uint16_t reserved{};
    wchar_t executable_path[route_bridge_path_chars]{};
};

struct route_bridge_app_v1 {
    wchar_t executable_path[route_bridge_path_chars]{};
    wchar_t display_name[route_bridge_display_chars]{};
    std::uint32_t open_window_count{};
    std::uint32_t active_session_count{};
    std::uint8_t routed_to_cable{};
    std::uint8_t reserved[3]{};
};

struct route_bridge_response_v1 {
    std::uint32_t magic{route_bridge_magic};
    std::uint16_t version{route_bridge_version};
    std::uint16_t bytes{sizeof(route_bridge_response_v1)};
    std::uint32_t sequence{};
    std::uint32_t status{static_cast<std::uint32_t>(route_bridge_status::accepted)};
    std::uint32_t app_count{};
    wchar_t message[route_bridge_message_chars]{};
    route_bridge_app_v1 apps[route_bridge_max_apps]{};
};

struct route_bridge_transaction_v1 {
    route_bridge_request_v1 request{};
    route_bridge_response_v1 response{};
};
#pragma pack(pop)

inline constexpr UINT route_bridge_request_message = WM_APP + 43;

inline bool valid_route_bridge_request(const route_bridge_request_v1& request) noexcept {
    return request.magic == route_bridge_magic && request.version == route_bridge_version &&
           request.bytes == sizeof(route_bridge_request_v1) &&
           request.command <= static_cast<std::uint8_t>(route_bridge_command::set_route);
}

static_assert(std::is_trivially_copyable_v<route_bridge_request_v1>);
static_assert(std::is_trivially_copyable_v<route_bridge_response_v1>);

}  // namespace termite
