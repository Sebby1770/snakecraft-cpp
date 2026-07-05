#pragma once

#include <string>
#include <vector>

namespace snakecraft {

struct ScoreEntry {
    int score = 0;
    int length = 0;
    int mined = 0;
    int built = 0;
    std::string recordedAt;
};

class ScoreStore {
public:
    explicit ScoreStore(std::string path);

    void load();
    void save() const;
    bool tryAdd(int score, int length, int mined, int built);
    [[nodiscard]] const std::vector<ScoreEntry>& entries() const;
    [[nodiscard]] int bestScore() const;

private:
    std::string path_;
    std::vector<ScoreEntry> entries_;
};

} // namespace snakecraft