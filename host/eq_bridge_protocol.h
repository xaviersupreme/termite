#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace termite {

// This is deliberately a C-compatible wire format.  It is mirrored by the
// VCL client; never put STL objects, pointers, or C++ enums on this boundary.
inline constexpr wchar_t eq_bridge_pipe_name[] = L"\\\\.\\pipe\\termite.eq.v1";
inline constexpr std::uint32_t eq_bridge_magic = 0x5145544DU;  // "MTEQ"
inline constexpr std::uint16_t eq_bridge_version = 1;
inline constexpr std::size_t eq_bridge_max_markers = 32;

enum class eq_bridge_mode : std::uint8_t { graphic = 0, arbitrary = 1 };
enum class eq_bridge_interpolation : std::uint8_t { spline = 0, linear = 1, step = 2 };
enum class eq_bridge_x_axis : std::uint8_t { logarithmic = 0, linear = 1 };
enum class eq_bridge_status : std::uint32_t { accepted = 0, rejected = 1 };

#pragma pack(push, 1)
struct eq_bridge_marker_v1 {
    float frequency_hz{};
    float gain_db{};
};

struct eq_bridge_snapshot_v1 {
    std::uint32_t magic{eq_bridge_magic};
    std::uint16_t version{eq_bridge_version};
    std::uint16_t bytes{sizeof(eq_bridge_snapshot_v1)};
    std::uint32_t sequence{};
    std::uint8_t mode{static_cast<std::uint8_t>(eq_bridge_mode::graphic)};
    std::uint8_t equalizer_enabled{1};
    std::uint8_t interpolation{static_cast<std::uint8_t>(eq_bridge_interpolation::spline)};
    std::uint8_t x_axis{static_cast<std::uint8_t>(eq_bridge_x_axis::logarithmic)};
    float preamp_db{};
    float smoothing{};
    float tension{};
    std::uint32_t marker_count{};
    std::array<float, 20> graphic_gains{};
    std::array<eq_bridge_marker_v1, eq_bridge_max_markers> markers{};
};

struct eq_bridge_ack_v1 {
    std::uint32_t magic{eq_bridge_magic};
    std::uint16_t version{eq_bridge_version};
    std::uint16_t bytes{sizeof(eq_bridge_ack_v1)};
    std::uint32_t sequence{};
    std::uint32_t status{static_cast<std::uint32_t>(eq_bridge_status::accepted)};
};
#pragma pack(pop)

static_assert(std::is_trivially_copyable_v<eq_bridge_snapshot_v1>);
static_assert(sizeof(eq_bridge_snapshot_v1) == 368);
static_assert(sizeof(eq_bridge_ack_v1) == 16);

}  // namespace termite
