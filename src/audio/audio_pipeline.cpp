#include "audio/audio_pipeline.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>

namespace termite {
namespace {

constexpr float pi = 3.14159265358979323846F;

float clamp_sample(float value) noexcept {
    return std::isfinite(value) ? std::clamp(value, -1.0F, 1.0F) : 0.0F;
}

float decode_s16(const std::byte* bytes) noexcept {
    const auto value = static_cast<std::uint16_t>(std::to_integer<unsigned char>(bytes[0])) |
                       (static_cast<std::uint16_t>(std::to_integer<unsigned char>(bytes[1])) << 8U);
    return static_cast<float>(static_cast<std::int16_t>(value)) / 32768.0F;
}

float decode_s24(const std::byte* bytes) noexcept {
    std::int32_t value = static_cast<std::int32_t>(std::to_integer<unsigned char>(bytes[0])) |
                         (static_cast<std::int32_t>(std::to_integer<unsigned char>(bytes[1])) << 8U) |
                         (static_cast<std::int32_t>(std::to_integer<unsigned char>(bytes[2])) << 16U);
    if ((value & 0x00800000) != 0) value |= static_cast<std::int32_t>(0xFF000000);
    return static_cast<float>(value) / 8388608.0F;
}

float decode_s32(const std::byte* bytes) noexcept {
    const auto value = static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[0])) |
                       (static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[1])) << 8U) |
                       (static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[2])) << 16U) |
                       (static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[3])) << 24U);
    return static_cast<float>(static_cast<std::int32_t>(value)) / 2147483648.0F;
}

void encode_s16(std::byte* bytes, float sample) noexcept {
    const auto value = static_cast<std::int16_t>(std::lrint(clamp_sample(sample) * 32767.0F));
    const auto bits = static_cast<std::uint16_t>(value);
    bytes[0] = static_cast<std::byte>(bits & 0xFFU);
    bytes[1] = static_cast<std::byte>((bits >> 8U) & 0xFFU);
}

void encode_s24(std::byte* bytes, float sample) noexcept {
    const auto value = static_cast<std::int32_t>(std::lrint(clamp_sample(sample) * 8388607.0F));
    bytes[0] = static_cast<std::byte>(value & 0xFF);
    bytes[1] = static_cast<std::byte>((value >> 8U) & 0xFF);
    bytes[2] = static_cast<std::byte>((value >> 16U) & 0xFF);
}

void encode_s32(std::byte* bytes, float sample) noexcept {
    const auto value = static_cast<std::int32_t>(std::llround(static_cast<double>(clamp_sample(sample)) * 2147483647.0));
    const auto bits = static_cast<std::uint32_t>(value);
    bytes[0] = static_cast<std::byte>(bits & 0xFFU);
    bytes[1] = static_cast<std::byte>((bits >> 8U) & 0xFFU);
    bytes[2] = static_cast<std::byte>((bits >> 16U) & 0xFFU);
    bytes[3] = static_cast<std::byte>((bits >> 24U) & 0xFFU);
}

float sinc(float value) noexcept {
    return std::abs(value) < 0.00001F ? 1.0F : std::sin(pi * value) / (pi * value);
}

}  // namespace

std::size_t audio_format::bytes_per_sample() const noexcept {
    switch (encoding) {
        case sample_encoding::float32:
        case sample_encoding::pcm_s32: return 4;
        case sample_encoding::pcm_s24: return 3;
        case sample_encoding::pcm_s16: return 2;
        case sample_encoding::unsupported: return 0;
    }
    return 0;
}

bool audio_format::is_supported() const noexcept {
    return encoding != sample_encoding::unsupported && sample_rate >= 8000 && channels >= 1 && channels <= audio_max_channels;
}

bool decode_interleaved(std::span<const std::byte> source,
                        const audio_format& format,
                        std::size_t frame_count,
                        float* destination) noexcept {
    if (!format.is_supported() || destination == nullptr) return false;
    const auto sample_count = frame_count * format.channels;
    const auto bytes_per_sample = format.bytes_per_sample();
    if (bytes_per_sample == 0 || source.size() < sample_count * bytes_per_sample) return false;

    for (std::size_t sample = 0; sample < sample_count; ++sample) {
        const auto* bytes = source.data() + sample * bytes_per_sample;
        switch (format.encoding) {
            case sample_encoding::float32: {
                float value{};
                std::memcpy(&value, bytes, sizeof(value));
                destination[sample] = clamp_sample(value);
                break;
            }
            case sample_encoding::pcm_s16: destination[sample] = decode_s16(bytes); break;
            case sample_encoding::pcm_s24: destination[sample] = decode_s24(bytes); break;
            case sample_encoding::pcm_s32: destination[sample] = decode_s32(bytes); break;
            case sample_encoding::unsupported: return false;
        }
    }
    return true;
}

bool encode_interleaved(const float* source,
                        const audio_format& format,
                        std::size_t frame_count,
                        std::span<std::byte> destination) noexcept {
    if (!format.is_supported() || source == nullptr) return false;
    const auto sample_count = frame_count * format.channels;
    const auto bytes_per_sample = format.bytes_per_sample();
    if (bytes_per_sample == 0 || destination.size() < sample_count * bytes_per_sample) return false;

    for (std::size_t sample = 0; sample < sample_count; ++sample) {
        auto* bytes = destination.data() + sample * bytes_per_sample;
        switch (format.encoding) {
            case sample_encoding::float32: {
                const auto value = clamp_sample(source[sample]);
                std::memcpy(bytes, &value, sizeof(value));
                break;
            }
            case sample_encoding::pcm_s16: encode_s16(bytes, source[sample]); break;
            case sample_encoding::pcm_s24: encode_s24(bytes, source[sample]); break;
            case sample_encoding::pcm_s32: encode_s32(bytes, source[sample]); break;
            case sample_encoding::unsupported: return false;
        }
    }
    return true;
}

bool spsc_frame_ring::configure(std::size_t capacity_frames, std::size_t channels) {
    if (capacity_frames < 2 || channels == 0 || channels > audio_max_channels) return false;
    storage_.assign(capacity_frames * channels, 0.0F);
    capacity_frames_ = capacity_frames;
    channels_ = channels;
    clear();
    return true;
}

void spsc_frame_ring::clear() noexcept {
    read_frame_.store(0, std::memory_order_release);
    write_frame_.store(0, std::memory_order_release);
}

std::size_t spsc_frame_ring::storage_offset(std::uint64_t frame, std::size_t channel) const noexcept {
    return (static_cast<std::size_t>(frame % capacity_frames_) * channels_) + channel;
}

std::size_t spsc_frame_ring::push(const float* source, std::size_t frame_count) noexcept {
    if (source == nullptr || capacity_frames_ == 0 || channels_ == 0) return 0;
    const auto write = write_frame_.load(std::memory_order_relaxed);
    const auto read = read_frame_.load(std::memory_order_acquire);
    const auto used = static_cast<std::size_t>(write - read);
    const auto count = std::min(frame_count, capacity_frames_ - std::min(used, capacity_frames_));
    for (std::size_t frame = 0; frame < count; ++frame) {
        for (std::size_t channel = 0; channel < channels_; ++channel) {
            storage_[storage_offset(write + frame, channel)] = clamp_sample(source[frame * channels_ + channel]);
        }
    }
    write_frame_.store(write + count, std::memory_order_release);
    return count;
}

std::size_t spsc_frame_ring::read(float* destination, std::size_t frame_count) noexcept {
    if (destination == nullptr) return 0;
    const auto count = std::min(frame_count, available_read());
    const auto read = read_frame_.load(std::memory_order_relaxed);
    for (std::size_t frame = 0; frame < count; ++frame) {
        for (std::size_t channel = 0; channel < channels_; ++channel) {
            destination[frame * channels_ + channel] = storage_[storage_offset(read + frame, channel)];
        }
    }
    read_frame_.store(read + count, std::memory_order_release);
    return count;
}

std::size_t spsc_frame_ring::discard(std::size_t frame_count) noexcept {
    const auto count = std::min(frame_count, available_read());
    read_frame_.fetch_add(count, std::memory_order_release);
    return count;
}

float spsc_frame_ring::sample_at(std::size_t frame_offset, std::size_t channel) const noexcept {
    if (channel >= channels_ || frame_offset >= available_read()) return 0.0F;
    const auto read = read_frame_.load(std::memory_order_relaxed);
    return storage_[storage_offset(read + frame_offset, channel)];
}

std::size_t spsc_frame_ring::available_read() const noexcept {
    if (capacity_frames_ == 0) return 0;
    const auto write = write_frame_.load(std::memory_order_acquire);
    const auto read = read_frame_.load(std::memory_order_acquire);
    return std::min<std::size_t>(static_cast<std::size_t>(write - read), capacity_frames_);
}

std::size_t spsc_frame_ring::available_write() const noexcept {
    return capacity_frames_ - available_read();
}

std::size_t spsc_frame_ring::channels() const noexcept { return channels_; }
std::size_t spsc_frame_ring::capacity_frames() const noexcept { return capacity_frames_; }

bool channel_mapper::configure(std::size_t input_channels, std::size_t output_channels) noexcept {
    if (input_channels == 0 || output_channels == 0 || input_channels > audio_max_channels || output_channels > audio_max_channels) return false;
    input_channels_ = input_channels;
    output_channels_ = output_channels;
    return true;
}

void channel_mapper::map_frame(const float* source, float* destination) const noexcept {
    if (source == nullptr || destination == nullptr) return;
    std::fill_n(destination, output_channels_, 0.0F);
    if (input_channels_ == output_channels_) {
        for (std::size_t channel = 0; channel < output_channels_; ++channel) destination[channel] = clamp_sample(source[channel]);
        return;
    }
    if (output_channels_ == 1) {
        if (input_channels_ == 1) {
            destination[0] = clamp_sample(source[0]);
        } else if (input_channels_ == 2) {
            destination[0] = clamp_sample((source[0] + source[1]) * 0.5F);
        } else {
            float value = source[0] + source[1] + source[2] * 0.707F;
            if (input_channels_ > 3) value += source[3] * 0.25F;
            for (std::size_t channel = 4; channel < input_channels_; ++channel) value += source[channel] * 0.5F;
            destination[0] = clamp_sample(value * 0.5F);
        }
        return;
    }
    if (input_channels_ == 1) {
        destination[0] = destination[1] = clamp_sample(source[0]);
        return;
    }
    if (input_channels_ == 2) {
        destination[0] = clamp_sample(source[0]);
        destination[1] = clamp_sample(source[1]);
        return;
    }
    float left = source[0] + source[2] * 0.707F;
    float right = source[1] + source[2] * 0.707F;
    if (input_channels_ > 3) {
        left += source[3] * 0.25F;
        right += source[3] * 0.25F;
    }
    for (std::size_t channel = 4; channel < input_channels_; ++channel) {
        if ((channel & 1U) == 0U) left += source[channel] * 0.5F;
        else right += source[channel] * 0.5F;
    }
    destination[0] = clamp_sample(left);
    destination[1] = clamp_sample(right);
}

bool streaming_resampler::configure(std::uint32_t input_rate, std::uint32_t output_rate, std::size_t channels) noexcept {
    if (input_rate < 8000 || output_rate < 8000 || channels == 0 || channels > audio_max_channels) return false;
    input_rate_ = input_rate;
    output_rate_ = output_rate;
    channels_ = channels;
    nominal_ratio_ = static_cast<float>(input_rate) / static_cast<float>(output_rate);
    build_coefficients();
    reset();
    return true;
}

void streaming_resampler::reset() noexcept {
    read_position_ = static_cast<float>(tap_count / 2U - 1U);
}

void streaming_resampler::build_coefficients() noexcept {
    const auto cutoff = std::min(1.0F, static_cast<float>(output_rate_) / static_cast<float>(input_rate_));
    constexpr float half = static_cast<float>(tap_count / 2U);
    for (std::size_t phase = 0; phase < phase_count; ++phase) {
        const float fraction = static_cast<float>(phase) / static_cast<float>(phase_count);
        float total{};
        for (std::size_t tap = 0; tap < tap_count; ++tap) {
            const float position = static_cast<float>(tap) - (half - 1.0F) - fraction;
            const float window_position = position / half;
            const float window = std::abs(window_position) < 1.0F ? sinc(window_position) : 0.0F;
            const float coefficient = cutoff * sinc(position * cutoff) * window;
            coefficients_[phase * tap_count + tap] = coefficient;
            total += coefficient;
        }
        if (std::abs(total) > 0.000001F) {
            for (std::size_t tap = 0; tap < tap_count; ++tap) coefficients_[phase * tap_count + tap] /= total;
        }
    }
}

std::size_t streaming_resampler::render(spsc_frame_ring& input,
                                        float* destination,
                                        std::size_t destination_frames,
                                        float input_frames_per_output) noexcept {
    if (destination == nullptr || channels_ == 0 || input.channels() != channels_) return 0;
    const auto ratio = std::clamp(input_frames_per_output, nominal_ratio_ * 0.9975F, nominal_ratio_ * 1.0025F);
    std::size_t produced{};
    constexpr std::size_t history = tap_count / 2U - 1U;
    constexpr std::size_t future = tap_count - history - 1U;
    while (produced < destination_frames) {
        const auto center = static_cast<std::size_t>(std::floor(read_position_));
        if (center < history || center + future >= input.available_read()) break;
        const auto fraction = read_position_ - static_cast<float>(center);
        const auto phase = std::min<std::size_t>(static_cast<std::size_t>(fraction * static_cast<float>(phase_count)), phase_count - 1U);
        const auto* coefficients = coefficients_.data() + phase * tap_count;
        for (std::size_t channel = 0; channel < channels_; ++channel) {
            float sample{};
            for (std::size_t tap = 0; tap < tap_count; ++tap) {
                sample += input.sample_at(center + tap - history, channel) * coefficients[tap];
            }
            destination[produced * channels_ + channel] = clamp_sample(sample);
        }
        ++produced;
        read_position_ += ratio;
        const auto next_center = static_cast<std::size_t>(std::floor(read_position_));
        if (next_center > history) {
            const auto discarded = input.discard(next_center - history);
            read_position_ -= static_cast<float>(discarded);
        }
    }
    return produced;
}

float streaming_resampler::nominal_input_frames_per_output() const noexcept { return nominal_ratio_; }

void drift_controller::configure(std::uint32_t input_rate, std::uint32_t target_fill_ms) noexcept {
    target_frames_ = std::max<std::size_t>(1, static_cast<std::size_t>(input_rate) * target_fill_ms / 1000U);
    reset();
}

void drift_controller::reset() noexcept { correction_ = 0.0F; }

float drift_controller::update(std::size_t buffered_frames) noexcept {
    const auto error = (static_cast<float>(buffered_frames) - static_cast<float>(target_frames_)) / static_cast<float>(target_frames_);
    const auto desired = std::clamp(error * 0.0025F, -0.0025F, 0.0025F);
    correction_ += std::clamp(desired - correction_, -0.00002F, 0.00002F);
    return correction_;
}

std::size_t drift_controller::target_frames() const noexcept { return target_frames_; }

bool endpoint_negotiation::valid() const noexcept { return error == endpoint_negotiation_error::none; }

endpoint_negotiation negotiate_endpoints(audio_format capture_format,
                                        audio_format render_format,
                                        bool render_is_cable) noexcept {
    endpoint_negotiation result{capture_format, render_format};
    if (!capture_format.is_supported()) result.error = endpoint_negotiation_error::unsupported_capture_format;
    else if (!render_format.is_supported()) result.error = endpoint_negotiation_error::unsupported_render_format;
    else if (render_is_cable) result.error = endpoint_negotiation_error::unsafe_render_endpoint;
    return result;
}

}  // namespace termite
