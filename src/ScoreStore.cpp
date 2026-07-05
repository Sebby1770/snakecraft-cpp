#include "ScoreStore.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace snakecraft {
namespace {

constexpr int kMaxEntries = 5;

std::string currentTimestamp()
{
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::ostringstream out;
    out << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M");
    return out.str();
}

} // namespace

ScoreStore::ScoreStore(std::string path)
    : path_(std::move(path))
{
    load();
}

void ScoreStore::load()
{
    entries_.clear();

    std::ifstream input(path_);
    if (!input) {
        return;
    }

    ScoreEntry entry;
    while (input >> entry.score >> entry.length >> entry.mined >> entry.built) {
        std::getline(input >> std::ws, entry.recordedAt);
        entries_.push_back(entry);
    }
}

void ScoreStore::save() const
{
    std::ofstream output(path_, std::ios::trunc);
    if (!output) {
        return;
    }

    for (const auto& entry : entries_) {
        output << entry.score << ' '
               << entry.length << ' '
               << entry.mined << ' '
               << entry.built << ' '
               << entry.recordedAt << '\n';
    }
}

bool ScoreStore::tryAdd(int score, int length, int mined, int built)
{
    if (score <= 0) {
        return false;
    }

    entries_.push_back({ score, length, mined, built, currentTimestamp() });
    std::sort(entries_.begin(), entries_.end(), [](const ScoreEntry& lhs, const ScoreEntry& rhs) {
        return lhs.score > rhs.score;
    });

    if (static_cast<int>(entries_.size()) > kMaxEntries) {
        entries_.resize(kMaxEntries);
    }

    save();
    return entries_.front().score == score;
}

const std::vector<ScoreEntry>& ScoreStore::entries() const
{
    return entries_;
}

int ScoreStore::bestScore() const
{
    if (entries_.empty()) {
        return 0;
    }

    return entries_.front().score;
}

} // namespace snakecraft