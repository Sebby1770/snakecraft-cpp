#include "Game.hpp"
#include "Terminal.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
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

} // namespace

int main()
{
    using Clock = std::chrono::steady_clock;

    snakecraft::Terminal terminal;
    snakecraft::Game game;

    std::cout << "\x1b[2J\x1b[?25l";

    auto lastTick = Clock::now();
    while (!game.isQuitRequested()) {
        while (const auto key = terminal.readKey()) {
            game.handle(actionFromKey(*key));
        }

        const auto now = Clock::now();
        if (now - lastTick >= frameDelayForScore(game.score())) {
            game.tick();
            lastTick = now;
        }

        std::cout << "\x1b[H" << game.render() << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(12));
    }

    std::cout << "\x1b[?25h\x1b[2J\x1b[H";
    std::cout << "Thanks for playing Snakecraft C++.\n";
    return 0;
}
