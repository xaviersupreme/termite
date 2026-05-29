#pragma once

#include "app/console_state.h"

#include <filesystem>
#include <string>
#include <vector>

namespace termite {

struct persisted_window_bounds {
    int x{};
    int y{};
    int width{};
    int height{};
    bool valid{};
};

struct termite_settings {
    console_persistent_state console;
    persisted_window_bounds window;
    std::vector<std::wstring> routing_executables;
};

struct settings_load_result {
    termite_settings settings;
    bool loaded{};
    std::wstring notice;
};

struct profile_load_result {
    eq_profile profile{eq_profile::flat()};
    bool loaded{};
    std::wstring notice;
};

inline constexpr wchar_t termite_profile_extension[] = L".termiteeq";

class settings_store {
public:
    explicit settings_store(std::filesystem::path path = default_path());

    [[nodiscard]] static std::filesystem::path default_path();
    [[nodiscard]] settings_load_result load() const;
    [[nodiscard]] bool save(const termite_settings& settings, std::wstring& failure_reason) const;
    [[nodiscard]] static profile_load_result load_profile_file(const std::filesystem::path& path);
    [[nodiscard]] static bool save_profile_file(const std::filesystem::path& path, const eq_profile& profile, std::wstring& failure_reason);

private:
    std::filesystem::path path_;
};

}  // namespace termite
