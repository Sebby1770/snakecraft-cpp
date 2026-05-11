#include "Game.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "test failed: " << message << '\n';
        std::exit(1);
    }
}

snakecraft::Point inFrontOf(const snakecraft::Game& game)
{
    const snakecraft::Point head = game.snakeHead();

    switch (game.direction()) {
    case snakecraft::Direction::Up:
        return { head.x, head.y - 1 };
    case snakecraft::Direction::Down:
        return { head.x, head.y + 1 };
    case snakecraft::Direction::Left:
        return { head.x - 1, head.y };
    case snakecraft::Direction::Right:
        return { head.x + 1, head.y };
    }

    return head;
}

void clearsPath(snakecraft::Game& game, int steps)
{
    auto point = game.snakeHead();
    for (int i = 0; i < steps; ++i) {
        ++point.x;
        game.setTile(point, snakecraft::Tile::Empty);
    }
}

} // namespace

int main()
{
    {
        snakecraft::Game game(30, 16, 42);
        game.setFood({ 1, 1 });

        const snakecraft::Point front = inFrontOf(game);
        game.setTile(front, snakecraft::Tile::Dirt);

        require(game.mineAhead(), "mining a dirt block should succeed");
        require(game.tileAt(front) == snakecraft::Tile::Empty, "mined tile should become empty");
        require(game.inventory().dirt == 1, "dirt should be added to inventory");
        require(game.minedBlocks() == 1, "mined block counter should increment");

        require(game.buildAhead(), "building with mined dirt should succeed");
        require(game.tileAt(front) == snakecraft::Tile::Dirt, "built tile should contain dirt");
        require(game.inventory().dirt == 0, "built dirt should be spent");
        require(game.builtBlocks() == 1, "built block counter should increment");

        game.tick();
        require(game.isGameOver(), "moving into built terrain should end the run");
    }

    {
        snakecraft::Game game(30, 16, 7);
        clearsPath(game, 3);

        const auto lengthBefore = game.snakeLength();
        const auto front = inFrontOf(game);
        game.setFood(front);

        require(game.tick(), "moving into food should advance the game");
        require(game.score() == 10, "food should add score");
        require(game.snakeLength() == lengthBefore + 1, "food should grow the snake");
    }

    {
        snakecraft::Game game(30, 16, 99);
        game.handle(snakecraft::Action::MoveLeft);
        game.tick();
        require(game.direction() == snakecraft::Direction::Right, "direct reversal should be ignored");

        game.handle(snakecraft::Action::Pause);
        require(game.isPaused(), "pause action should pause the game");
        const auto headBefore = game.snakeHead();
        game.tick();
        require(game.snakeHead() == headBefore, "paused game should not move");
    }

    std::cout << "all core mechanics passed\n";
    return 0;
}
