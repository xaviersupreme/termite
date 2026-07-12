#pragma once

#include "host/eq_bridge_protocol.h"
#include "sound/eq/eq_profile.h"

namespace termite {

[[nodiscard]] bool valid_eq_bridge_snapshot(const eq_bridge_snapshot_v1& snapshot) noexcept;
[[nodiscard]] float bridge_arbitrary_response_db(const eq_bridge_snapshot_v1& snapshot, float frequency_hz) noexcept;
[[nodiscard]] eq_profile profile_from_bridge_snapshot(const eq_bridge_snapshot_v1& snapshot, const eq_profile& current) noexcept;

}  // namespace termite
