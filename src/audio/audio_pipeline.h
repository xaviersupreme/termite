#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace termite {

inline constexpr std::size_t audio_max_channels = 8;

enum class sample_encoding : std::uint8_t {
    float32,
    pcm_s16,
    pcm_s24,
    pcm_s32,
    unsupported,
};

struct audio_format {
    sample_encoding encoding{sample_encoding::unsupported};
    std::uint32_t sample_rate{};
    std::uint16_t channels{};
    std::uint32_t channel_mask{};

    [[nodiscard]] std::size_t bytes_per_sample() const noexcept;
    [[nodiscard]] bool is_supported() const noexcept;
};

[[nodiscard]] bool decode_interleaved(std::span<const std::byte> source,
                                      const audio_format& format,
                                      std::size_t frame_count,
                                      float* destination) noexcept;
[[nodiscard]] bool encode_interleaved(const float* source,
                                      const audio_format& format,
                                      std::size_t frame_count,
                                      std::span<std::byte> destination) noexcept;

class spsc_frame_ring {
public:
    [[nodiscard]] bool configure(std::size_t capacity_frames, std::size_t channels);
    void clear() noexcept;
    [[nodiscard]] std::size_t push(const float* source, std::size_t frame_count) noexcept;
    [[nodiscard]] std::size_t read(float* destination, std::size_t frame_count) noexcept;
    [[nodiscard]] std::size_t discard(std::size_t frame_count) noexcept;
    [[nodiscard]] float sample_at(std::size_t frame_offset, std::size_t channel) const noexcept;
    [[nodiscard]] std::size_t available_read() const noexcept;
    [[nodiscard]] std::size_t available_write() const noexcept;
    [[nodiscard]] std::size_t channels() const noexcept;
    [[nodiscard]] std::size_t capacity_frames() const noexcept;

private:
    [[nodiscard]] std::size_t storage_offset(std::uint64_t frame, std::size_t channel) const noexcept;

    std::vector<float> storage_;
    std::size_t capacity_frames_{};
    std::size_t channels_{};
    alignas(64) std::atomic<std::uint64_t> read_frame_{};
    alignas(64) std::atomic<std::uint64_t> write_frame_{};
};

class channel_mapper {
public:
    [[nodiscard]] bool configure(std::size_t input_channels, std::size_t output_channels) noexcept;
    void map_frame(const float* source, float* destination) const noexcept;

private:
    std::size_t input_channels_{};
    std::size_t output_channels_{};
};

class streaming_resampler {
public:
    static constexpr std::size_t tap_count = 32;
    static constexpr std::size_t phase_count = 1024;

    [[nodiscard]] bool configure(std::uint32_t input_rate, std::uint32_t output_rate, std::size_t channels) noexcept;
    void reset() noexcept;
    [[nodiscard]] std::size_t render(spsc_frame_ring& input,
                                     float* destination,
                                     std::size_t destination_frames,
                                     float input_frames_per_output) noexcept;
    [[nodiscard]] float nominal_input_frames_per_output() const noexcept;

private:
    void build_coefficients() noexcept;

    std::array<float, tap_count * phase_count> coefficients_{};
    std::uint32_t input_rate_{};
    std::uint32_t output_rate_{};
    std::size_t channels_{};
    float nominal_ratio_{1.0F};
    float read_position_{static_cast<float>(tap_count / 2U - 1U)};
};

class drift_controller {
public:
    void configure(std::uint32_t input_rate, std::uint32_t target_fill_ms = 120) noexcept;
    void reset() noexcept;
    [[nodiscard]] float update(std::size_t buffered_frames) noexcept;
    [[nodiscard]] std::size_t target_frames() const noexcept;

private:
    std::size_t target_frames_{1};
    float correction_{};
};

enum class endpoint_negotiation_error : std::uint8_t {
    none,
    unsupported_capture_format,
    unsupported_render_format,
    unsafe_render_endpoint,
};

struct endpoint_negotiation {
    audio_format capture_format;
    audio_format render_format;
    endpoint_negotiation_error error{endpoint_negotiation_error::none};

    [[nodiscard]] bool valid() const noexcept;
};

[[nodiscard]] endpoint_negotiation negotiate_endpoints(audio_format capture_format,
                                                        audio_format render_format,
                                                        bool render_is_cable) noexcept;

}  // namespace termite
