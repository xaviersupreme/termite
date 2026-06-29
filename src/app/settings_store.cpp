#include "app/settings_store.h"

#include "app/console_state.h"
#include "dsp/eq_profile.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <ShlObj.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <string_view>
#include <system_error>
#include <utility>

namespace termite {
namespace {

enum class json_kind : std::uint8_t { null_value, boolean, number, string, array, object };

struct json_value {
    json_kind kind{json_kind::null_value};
    bool boolean{};
    double number{};
    std::string string;
    std::vector<json_value> array;
    std::map<std::string, json_value, std::less<>> object;
};

class json_reader {
public:
    explicit json_reader(std::string_view source) : source_(source) {}

    [[nodiscard]] bool parse(json_value& value) {
        skip_space();
        if (!parse_value(value, 0)) return false;
        skip_space();
        return position_ == source_.size();
    }

private:
    void skip_space() noexcept {
        while (position_ < source_.size() && (source_[position_] == ' ' || source_[position_] == '\n' || source_[position_] == '\r' || source_[position_] == '\t')) {
            ++position_;
        }
    }

    [[nodiscard]] bool parse_value(json_value& value, int depth) {
        if (depth > 32) return false;
        skip_space();
        if (position_ == source_.size()) return false;
        switch (source_[position_]) {
            case '{': return parse_object(value, depth + 1);
            case '[': return parse_array(value, depth + 1);
            case '"':
                value.kind = json_kind::string;
                return parse_string(value.string);
            case 't':
                if (source_.substr(position_, 4) != "true") return false;
                position_ += 4;
                value.kind = json_kind::boolean;
                value.boolean = true;
                return true;
            case 'f':
                if (source_.substr(position_, 5) != "false") return false;
                position_ += 5;
                value.kind = json_kind::boolean;
                value.boolean = false;
                return true;
            case 'n':
                if (source_.substr(position_, 4) != "null") return false;
                position_ += 4;
                value.kind = json_kind::null_value;
                return true;
            default: return parse_number(value);
        }
    }

    [[nodiscard]] bool parse_object(json_value& value, int depth) {
        ++position_;
        value.kind = json_kind::object;
        value.object.clear();
        skip_space();
        if (position_ < source_.size() && source_[position_] == '}') {
            ++position_;
            return true;
        }
        while (position_ < source_.size()) {
            std::string key;
            if (source_[position_] != '"' || !parse_string(key)) return false;
            skip_space();
            if (position_ == source_.size() || source_[position_++] != ':') return false;
            json_value child;
            if (!parse_value(child, depth)) return false;
            value.object.emplace(std::move(key), std::move(child));
            skip_space();
            if (position_ == source_.size()) return false;
            const char separator = source_[position_++];
            if (separator == '}') return true;
            if (separator != ',') return false;
            skip_space();
        }
        return false;
    }

    [[nodiscard]] bool parse_array(json_value& value, int depth) {
        ++position_;
        value.kind = json_kind::array;
        value.array.clear();
        skip_space();
        if (position_ < source_.size() && source_[position_] == ']') {
            ++position_;
            return true;
        }
        while (position_ < source_.size()) {
            json_value child;
            if (!parse_value(child, depth)) return false;
            value.array.push_back(std::move(child));
            skip_space();
            if (position_ == source_.size()) return false;
            const char separator = source_[position_++];
            if (separator == ']') return true;
            if (separator != ',') return false;
            skip_space();
        }
        return false;
    }

    [[nodiscard]] bool parse_string(std::string& value) {
        if (position_ == source_.size() || source_[position_++] != '"') return false;
        value.clear();
        while (position_ < source_.size()) {
            const char character = source_[position_++];
            if (character == '"') return true;
            if (static_cast<unsigned char>(character) < 0x20U) return false;
            if (character != '\\') {
                value.push_back(character);
                continue;
            }
            if (position_ == source_.size()) return false;
            switch (source_[position_++]) {
                case '"': value.push_back('"'); break;
                case '\\': value.push_back('\\'); break;
                case '/': value.push_back('/'); break;
                case 'b': value.push_back('\b'); break;
                case 'f': value.push_back('\f'); break;
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                default: return false;
            }
        }
        return false;
    }

    [[nodiscard]] bool parse_number(json_value& value) {
        const auto begin = position_;
        if (source_[position_] == '-') ++position_;
        const auto digits_begin = position_;
        while (position_ < source_.size() && source_[position_] >= '0' && source_[position_] <= '9') ++position_;
        if (digits_begin == position_) return false;
        if (position_ < source_.size() && source_[position_] == '.') {
            ++position_;
            const auto fractional_begin = position_;
            while (position_ < source_.size() && source_[position_] >= '0' && source_[position_] <= '9') ++position_;
            if (fractional_begin == position_) return false;
        }
        if (position_ < source_.size() && (source_[position_] == 'e' || source_[position_] == 'E')) {
            ++position_;
            if (position_ < source_.size() && (source_[position_] == '+' || source_[position_] == '-')) ++position_;
            const auto exponent_begin = position_;
            while (position_ < source_.size() && source_[position_] >= '0' && source_[position_] <= '9') ++position_;
            if (exponent_begin == position_) return false;
        }
        const auto text = source_.substr(begin, position_ - begin);
        double parsed{};
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), parsed, std::chars_format::general);
        if (error != std::errc{} || end != text.data() + text.size() || !std::isfinite(parsed)) return false;
        value.kind = json_kind::number;
        value.number = parsed;
        return true;
    }

    std::string_view source_;
    std::size_t position_{};
};

[[nodiscard]] const json_value* member(const json_value& object, std::string_view name) {
    if (object.kind != json_kind::object) return nullptr;
    const auto found = object.object.find(name);
    return found == object.object.end() ? nullptr : &found->second;
}

[[nodiscard]] bool read_boolean(const json_value* value, bool& result) {
    if (value == nullptr || value->kind != json_kind::boolean) return false;
    result = value->boolean;
    return true;
}

[[nodiscard]] bool read_number(const json_value* value, double& result) {
    if (value == nullptr || value->kind != json_kind::number || !std::isfinite(value->number)) return false;
    result = value->number;
    return true;
}

[[nodiscard]] bool read_int(const json_value* value, int& result) {
    double number{};
    if (!read_number(value, number) || number < static_cast<double>(std::numeric_limits<int>::min()) || number > static_cast<double>(std::numeric_limits<int>::max())) return false;
    result = static_cast<int>(std::lround(number));
    return true;
}

[[nodiscard]] std::string narrow(std::wstring_view value) {
    if (value.empty()) return {};
    const auto count = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(std::max(0, count)), '\0');
    if (count > 0) WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), count, nullptr, nullptr);
    return result;
}

[[nodiscard]] std::wstring widen(std::string_view value) {
    if (value.empty()) return {};
    const auto count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(std::max(0, count)), L'\0');
    if (count > 0) MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), count);
    return result;
}

void write_string(std::ostringstream& output, std::string_view value) {
    output.put('"');
    for (const auto character : value) {
        switch (character) {
            case '"': output << "\\\""; break;
            case '\\': output << "\\\\"; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default:
                if (static_cast<unsigned char>(character) < 0x20U) output << "?";
                else output.put(character);
                break;
        }
    }
    output.put('"');
}

[[nodiscard]] bool decode_profile_object(const json_value& value, eq_profile& result) {
    const auto* bands = member(value, "bands");
    if (value.kind != json_kind::object || bands == nullptr || bands->kind != json_kind::array || bands->array.size() != graphic_band_count) return false;

    eq_profile profile = eq_profile::flat();
    if (!read_boolean(member(value, "enabled"), profile.enabled)) return false;
    double number{};
    if (!read_number(member(value, "preamp_db"), number)) return false;
    profile.preamp_db = std::clamp(static_cast<float>(number), -24.0F, 12.0F);
    if (!read_number(member(value, "limiter_ceiling_db"), number)) return false;
    profile.limiter_ceiling_db = std::clamp(static_cast<float>(number), -12.0F, 0.0F);
    for (std::size_t index = 0; index < graphic_band_count; ++index) {
        const auto& band_value = bands->array[index];
        if (band_value.kind != json_kind::object) return false;
        auto& band = profile.bands[index];
        int shape{};
        if (!read_int(member(band_value, "shape"), shape) || !read_number(member(band_value, "gain_db"), number)) return false;
        band.shape = shape >= 0 && shape <= static_cast<int>(filter_shape::notch) ? static_cast<filter_shape>(shape) : filter_shape::peaking;
        band.gain_db = std::clamp(static_cast<float>(number), -20.0F, 20.0F);
        if (!read_number(member(band_value, "q"), number) || !read_boolean(member(band_value, "enabled"), band.enabled)) return false;
        band.q = std::clamp(static_cast<float>(number), 0.15F, 12.0F);
        band.frequency_hz = graphic_band_frequencies[index];
    }
    // v1 profiles intentionally did not have effects.  Flat effects are the
    // compatibility default, while v2 requires a complete effects object.
    if (const auto* effects = member(value, "effects"); effects != nullptr) {
        if (effects->kind != json_kind::object ||
            !read_boolean(member(*effects, "bass_enabled"), profile.effects.bass_enabled) ||
            !read_number(member(*effects, "bass_db"), number) ||
            !read_boolean(member(*effects, "loudness_enabled"), profile.effects.loudness_enabled)) return false;
        profile.effects.bass_db = std::clamp(static_cast<float>(number), -12.0F, 12.0F);
        if (!read_number(member(*effects, "loudness_amount"), number)) return false;
        profile.effects.loudness_amount = std::clamp(static_cast<float>(number), 0.0F, 1.0F);
        if (!read_boolean(member(*effects, "clarity_enabled"), profile.effects.clarity_enabled) || !read_number(member(*effects, "clarity_db"), number)) return false;
        profile.effects.clarity_db = std::clamp(static_cast<float>(number), -8.0F, 8.0F);
        if (!read_boolean(member(*effects, "stereo_enabled"), profile.effects.stereo_enabled) || !read_number(member(*effects, "stereo_width"), number)) return false;
        profile.effects.stereo_width = std::clamp(static_cast<float>(number), 0.5F, 1.5F);
        if (!read_boolean(member(*effects, "mono"), profile.effects.mono) || !read_number(member(*effects, "balance"), number)) return false;
        profile.effects.balance = std::clamp(static_cast<float>(number), -1.0F, 1.0F);
    }
    result = profile;
    return true;
}

void encode_profile_object(std::ostringstream& output, const eq_profile& profile, std::string_view indent) {
    output << indent << "\"enabled\": " << (profile.enabled ? "true" : "false")
           << ",\n" << indent << "\"preamp_db\": " << profile.preamp_db
           << ",\n" << indent << "\"limiter_ceiling_db\": " << profile.limiter_ceiling_db
           << ",\n" << indent << "\"effects\": {\"bass_enabled\": " << (profile.effects.bass_enabled ? "true" : "false")
           << ", \"bass_db\": " << profile.effects.bass_db << ", \"loudness_enabled\": " << (profile.effects.loudness_enabled ? "true" : "false")
           << ", \"loudness_amount\": " << profile.effects.loudness_amount << ", \"clarity_enabled\": " << (profile.effects.clarity_enabled ? "true" : "false")
           << ", \"clarity_db\": " << profile.effects.clarity_db << ", \"stereo_enabled\": " << (profile.effects.stereo_enabled ? "true" : "false")
           << ", \"stereo_width\": " << profile.effects.stereo_width << ", \"mono\": " << (profile.effects.mono ? "true" : "false")
           << ", \"balance\": " << profile.effects.balance << "}"
           << ",\n" << indent << "\"bands\": [\n";
    for (std::size_t index = 0; index < graphic_band_count; ++index) {
        const auto& band = profile.bands[index];
        output << indent << "  {\"shape\": " << static_cast<int>(band.shape) << ", \"gain_db\": " << band.gain_db
               << ", \"q\": " << band.q << ", \"enabled\": " << (band.enabled ? "true" : "false") << "}";
        output << (index + 1 == graphic_band_count ? "\n" : ",\n");
    }
    output << indent << "]";
}

[[nodiscard]] bool decode_settings(const json_value& root, termite_settings& result) {
    int version{};
    if (!read_int(member(root, "version"), version) || (version != 1 && version != 2)) return false;
    console_persistent_state state;
    const auto* profile = member(root, "profile");
    if (profile == nullptr || !decode_profile_object(*profile, state.profile)) return false;

    const auto* ui = member(root, "ui");
    int background{};
    if (ui == nullptr || ui->kind != json_kind::object || !read_int(member(*ui, "preset"), state.preset_index) ||
        !read_boolean(member(*ui, "paused"), state.paused) || !read_boolean(member(*ui, "sleeping"), state.sleeping) ||
        !read_boolean(member(*ui, "default_start"), state.default_start_saved) || !read_boolean(member(*ui, "grid"), state.grid_visible) ||
        !read_int(member(*ui, "background"), background)) return false;
    int active_tab{};
    if (version >= 2 && !read_int(member(*ui, "active_tab"), active_tab)) return false;
    state.preset_index = std::clamp(state.preset_index, -1, static_cast<int>(console_state::preset_count()) - 1);
    state.background_index = static_cast<std::size_t>(std::max(0, background)) % 6U;
    state.active_tab = active_tab >= 0 && active_tab < static_cast<int>(console_tab_count) ? static_cast<console_tab>(active_tab) : console_tab::graphic_eq;

    const auto* window = member(root, "window");
    if (window == nullptr || window->kind != json_kind::object || !read_int(member(*window, "x"), result.window.x) ||
        !read_int(member(*window, "y"), result.window.y) || !read_int(member(*window, "width"), result.window.width) ||
        !read_int(member(*window, "height"), result.window.height) || !read_boolean(member(*window, "valid"), result.window.valid)) return false;
    result.window.valid = result.window.valid && result.window.width >= 934 && result.window.height >= 525 && result.window.width <= 7680 && result.window.height <= 4320;

    const auto* routing = member(root, "routing");
    if (routing == nullptr || routing->kind != json_kind::array) return false;
    for (const auto& entry : routing->array) {
        if (entry.kind != json_kind::string) return false;
        const auto path = widen(entry.string);
        if (!path.empty() && result.routing_executables.size() < 128) result.routing_executables.push_back(path);
    }
    result.console = state;
    return true;
}

[[nodiscard]] std::string encode_settings(const termite_settings& settings) {
    const auto& state = settings.console;
    std::ostringstream output;
    output.precision(9);
    output << "{\n  \"version\": 2,\n  \"profile\": {\n";
    encode_profile_object(output, state.profile, "    ");
    output << "\n  },\n  \"ui\": {\"preset\": " << state.preset_index << ", \"paused\": " << (state.paused ? "true" : "false")
           << ", \"sleeping\": " << (state.sleeping ? "true" : "false") << ", \"default_start\": " << (state.default_start_saved ? "true" : "false")
           << ", \"grid\": " << (state.grid_visible ? "true" : "false") << ", \"background\": " << state.background_index
           << ", \"active_tab\": " << static_cast<std::size_t>(state.active_tab) << "},\n"
           << "  \"window\": {\"x\": " << settings.window.x << ", \"y\": " << settings.window.y << ", \"width\": " << settings.window.width
           << ", \"height\": " << settings.window.height << ", \"valid\": " << (settings.window.valid ? "true" : "false") << "},\n  \"routing\": [";
    for (std::size_t index = 0; index < settings.routing_executables.size(); ++index) {
        if (index != 0) output << ", ";
        write_string(output, narrow(settings.routing_executables[index]));
    }
    output << "]\n}\n";
    return output.str();
}

[[nodiscard]] bool decode_profile_file(const json_value& root, eq_profile& result) {
    int version{};
    const auto* format = member(root, "format");
    const auto* profile = member(root, "profile");
    return format != nullptr && format->kind == json_kind::string && format->string == "termite-eq-profile" &&
           read_int(member(root, "version"), version) && (version == 1 || version == 2) && profile != nullptr && decode_profile_object(*profile, result);
}

[[nodiscard]] std::string encode_profile_file(const eq_profile& profile) {
    std::ostringstream output;
    output.precision(9);
    output << "{\n  \"format\": \"termite-eq-profile\",\n  \"version\": 2,\n  \"profile\": {\n";
    encode_profile_object(output, profile, "    ");
    output << "\n  }\n}\n";
    return output.str();
}

}  // namespace

settings_store::settings_store(std::filesystem::path path) : path_(std::move(path)) {}

std::filesystem::path settings_store::default_path() {
    PWSTR raw{};
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &raw)) || raw == nullptr) {
        return L"Termite/settings.json";
    }
    const std::filesystem::path path = std::filesystem::path(raw) / L"Termite" / L"settings.json";
    CoTaskMemFree(raw);
    return path;
}

settings_load_result settings_store::load() const {
    settings_load_result result;
    std::ifstream input(path_, std::ios::binary);
    if (!input) return result;
    const std::string source((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    json_value root;
    if (source.empty() || !json_reader(source).parse(root) || !decode_settings(root, result.settings)) {
        result.notice = L"Saved settings were invalid; defaults were restored.";
        return result;
    }
    result.loaded = true;
    result.notice = L"Saved settings restored.";
    return result;
}

bool settings_store::save(const termite_settings& settings, std::wstring& failure_reason) const {
    std::error_code error;
    if (!path_.parent_path().empty()) std::filesystem::create_directories(path_.parent_path(), error);
    if (error) {
        failure_reason = L"Could not create the Termite settings folder.";
        return false;
    }
    const auto temporary = path_.wstring() + L".tmp";
    {
        std::ofstream output(std::filesystem::path(temporary), std::ios::binary | std::ios::trunc);
        if (!output) {
            failure_reason = L"Could not write Termite settings.";
            return false;
        }
        output << encode_settings(settings);
        output.flush();
        if (!output) {
            failure_reason = L"Could not finish writing Termite settings.";
            return false;
        }
    }
    if (!MoveFileExW(temporary.c_str(), path_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::filesystem::remove(std::filesystem::path(temporary), error);
        failure_reason = L"Could not replace Termite settings.";
        return false;
    }
    return true;
}

profile_load_result settings_store::load_profile_file(const std::filesystem::path& path) {
    profile_load_result result;
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        result.notice = L"Could not open the selected Termite EQ profile.";
        return result;
    }
    const std::string source((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    json_value root;
    if (source.empty() || !json_reader(source).parse(root) || !decode_profile_file(root, result.profile)) {
        result.notice = L"That file is not a valid Termite EQ profile.";
        return result;
    }
    result.loaded = true;
    return result;
}

bool settings_store::save_profile_file(const std::filesystem::path& path, const eq_profile& profile, std::wstring& failure_reason) {
    std::error_code error;
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        failure_reason = L"Could not create the selected profile folder.";
        return false;
    }
    const auto temporary = path.wstring() + L".tmp";
    {
        std::ofstream output(std::filesystem::path(temporary), std::ios::binary | std::ios::trunc);
        if (!output) {
            failure_reason = L"Could not write the Termite EQ profile.";
            return false;
        }
        output << encode_profile_file(profile);
        output.flush();
        if (!output) {
            failure_reason = L"Could not finish writing the Termite EQ profile.";
            return false;
        }
    }
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::filesystem::remove(std::filesystem::path(temporary), error);
        failure_reason = L"Could not replace the Termite EQ profile.";
        return false;
    }
    return true;
}

}  // namespace termite
