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

class settings_store {
public:
    explicit settings_store(std::filesystem::path path = default_path());

    [[nodiscard]] static std::filesystem::path default_path();
    [[nodiscard]] settings_load_result load() const;
    [[nodiscard]] bool save(const termite_settings& settings, std::wstring& failure_reason) const;

private:
    std::filesystem::path path_;
};

}  // namespace termite
