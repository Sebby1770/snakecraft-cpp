#include "Game.hpp"
#include "SaveManager.hpp"
#include "ScoreStore.hpp"
#include "Terminal.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <thread>

namespace {

snakecraft::Action actionFromKey(char key)
{
    const auto lower = static_cast<char>(std::tolower(static_cast<unsigned char>(key)));

    switch (lower) {
    case 'w':
        return snakecraft::Action::MoveUp;
    case 'a':
        return snakecraft::Action::MoveLeft;
    case 's':
        return snakecraft::Action::MoveDown;
    case 'd':
        return snakecraft::Action::MoveRight;
    case ' ':
        return snakecraft::Action::Mine;
    case 'e':
        return snakecraft::Action::Build;
    case '\t':
        return snakecraft::Action::CycleBlock;
    case 'p':
        return snakecraft::Action::Pause;
    case 'r':
        return snakecraft::Action::Restart;
    case '5':
        return snakecraft::Action::SaveGame;
    case '9':
        return snakecraft::Action::LoadGame;
    case 'q':
        return snakecraft::Action::Quit;
    default:
        return snakecraft::Action::None;
    }
}

std::chrono::milliseconds frameDelayForScore(int score)
{
    const int speedup = std::min(score / 30, 60);
    return std::chrono::milliseconds(145 - speedup);
}

std::filesystem::path dataDirectory()
{
    const auto home = std::getenv("HOME");
    if (home != nullptr) {
        return std::filesystem::path(home) / ".snakecraft";
    }

    return std::filesystem::current_path() / ".snakecraft";
}

void recordHighScore(snakecraft::ScoreStore& scores, const snakecraft::Game& game)
{
    if (!game.isGameOver()) {
        return;
    }

    if (scores.tryAdd(
            game.score(),
            static_cast<int>(game.snakeLength()),
            game.minedBlocks(),
            game.builtBlocks())) {
        std::cout << "\nNew high score: " << game.score() << '\n';
    }
}

} // namespace

int main()
{
    using Clock = std::chrono::steady_clock;

    const auto dataDir = dataDirectory();
    std::filesystem::create_directories(dataDir);

    snakecraft::Terminal terminal;
    snakecraft::Game game;
    snakecraft::ScoreStore scores((dataDir / "highscores.txt").string());
    snakecraft::SaveManager saves(dataDir / "savegame.txt");

    std::cout << "\x1b[2J\x1b[?25l";
    if (scores.bestScore() > 0) {
        std::cout << "Best score so far: " << scores.bestScore() << '\n';
    }

    auto lastTick = Clock::now();
    while (!game.isQuitRequested()) {
        while (const auto key = terminal.readKey()) {
            const auto action = actionFromKey(*key);
            if (action == snakecraft::Action::SaveGame) {
                if (saves.save(game)) {
                    std::cout << "\nSaved game to " << saves.path() << '\n';
                }
                continue;
            }

            if (action == snakecraft::Action::LoadGame) {
                if (auto loaded = saves.load()) {
                    game = std::move(*loaded);
                    std::cout << "\nLoaded game from " << saves.path() << '\n';
                } else {
                    std::cout << "\nNo save file found.\n";
                }
                continue;
            }

            game.handle(action);
        }

        const auto now = Clock::now();
        if (now - lastTick >= frameDelayForScore(game.score())) {
            const bool wasOver = game.isGameOver();
            game.tick();
            if (!wasOver && game.isGameOver()) {
                recordHighScore(scores, game);
            }
            lastTick = now;
        }

        std::cout << "\x1b[H" << game.render() << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(12));
    }

    std::cout << "\x1b[?25h\x1b[2J\x1b[H";
    std::cout << "Thanks for playing Snakecraft C++.\n";
    return 0;
}