#pragma once

#include "Game.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace snakecraft {

class SaveManager {
public:
    explicit SaveManager(std::filesystem::path path);

    [[nodiscard]] bool save(const Game& game) const;
    [[nodiscard]] std::optional<Game> load() const;
    [[nodiscard]] bool exists() const;
    [[nodiscard]] const std::filesystem::path& path() const;

private:
    std::filesystem::path path_;
};

} // namespace snakecraft