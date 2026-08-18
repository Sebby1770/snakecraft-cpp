#include "Game.hpp"
#include "SaveManager.hpp"
#include "ScoreStore.hpp"

#include <cstdlib>
#include <filesystem>
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
        require(game.lives() == 2, "first collision should spend a life");
        require(!game.isGameOver(), "the run continues until lives are gone");
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

    {
        snakecraft::Game game(30, 16, 55);
        const snakecraft::Point front = inFrontOf(game);
        game.setTile(front, snakecraft::Tile::Ore);

        require(game.mineAhead(), "mining ore should succeed");
        require(game.inventory().ore == 1, "ore should be added to inventory");
        require(game.score() == 8, "ore should award eight points");

        game.restoreState(
            game.score(),
            game.minedBlocks(),
            game.builtBlocks(),
            game.ticks(),
            game.inventory(),
            game.direction(),
            game.pendingDirection(),
            snakecraft::Tile::Ore);
        require(!game.buildAhead(), "ore should not be placeable");
    }

    {
        snakecraft::Game game(30, 16, 12);
        require(game.biomeAt({ 2, 4 }) == snakecraft::Biome::Forest, "left side should be forest");
        require(game.biomeAt({ 15, 4 }) == snakecraft::Biome::Cave, "middle should be cave");
        require(game.biomeAt({ 25, 4 }) == snakecraft::Biome::Desert, "right side should be desert");
    }

    {
        const auto tempDir = std::filesystem::temp_directory_path() / "snakecraft-test-save";
        std::filesystem::create_directories(tempDir);
        const auto savePath = tempDir / "save.txt";

        snakecraft::Game original(24, 14, 88);
        original.handle(snakecraft::Action::MoveUp);
        clearsPath(original, 4);

        snakecraft::SaveManager saves(savePath);
        require(saves.save(original), "save should succeed");

        const auto loaded = saves.load();
        require(loaded.has_value(), "load should succeed");
        require(loaded->score() == original.score(), "loaded score should match");
        require(loaded->snakeLength() == original.snakeLength(), "loaded snake length should match");
        require(loaded->tileAt(inFrontOf(original)) == original.tileAt(inFrontOf(original)), "loaded world should match");

        std::filesystem::remove_all(tempDir);
    }

    {
        const auto tempDir = std::filesystem::temp_directory_path() / "snakecraft-test-scores";
        std::filesystem::create_directories(tempDir);
        const auto scorePath = tempDir / "scores.txt";

        snakecraft::ScoreStore scores(scorePath.string());
        require(scores.tryAdd(42, 6, 2, 1), "first score should be recorded");
        require(scores.bestScore() == 42, "best score should update");
        require(!scores.tryAdd(30, 5, 1, 0), "lower score should not become best");
        require(scores.bestScore() == 42, "best score should remain highest");

        std::filesystem::remove_all(tempDir);
    }

    {
        snakecraft::Game game(30, 16, 3);
        game.setWrapWorld(true);
        game.setSnake({ { 29, 8 }, { 28, 8 }, { 27, 8 } });
        game.handle(snakecraft::Action::MoveRight);
        for (int x = 0; x < 30; ++x) {
            game.setTile({ x, 8 }, snakecraft::Tile::Empty);
        }
        game.setFood({ 1, 1 });
        require(game.tick(), "wrap move should stay alive");
        require(game.snakeHead().x == 0, "head should wrap to the left edge");
        require(!game.isGameOver(), "wrap should not count as an edge death");
    }

    {
        snakecraft::Game game(30, 16, 11);
        clearsPath(game, 6);
        auto front = inFrontOf(game);
        game.setFood(front);
        game.setFoodKind(snakecraft::FoodKind::Regular);
        require(game.tick(), "first food tick");
        require(game.combo() == 1, "first bite starts combo");
        require(game.score() == 10, "first regular food is 10");

        front = inFrontOf(game);
        game.setTile(front, snakecraft::Tile::Empty);
        game.setFood(front);
        game.setFoodKind(snakecraft::FoodKind::Golden);
        require(game.tick(), "golden food tick");
        require(game.combo() == 2, "combo should increment");
        require(game.score() == 37, "golden plus combo bonus should score 25+2");
    }

    {
        snakecraft::Game game(30, 16, 19);
        clearsPath(game, 4);
        const auto before = game.snakeHead();
        require(game.tick(), "first step should move");
        require(game.snakeHead() != before, "head should advance");
        require(game.undo(), "undo should rewind the step");
        require(game.snakeHead() == before, "undo should restore the previous head");
    }

    {
        snakecraft::Game game(30, 16, 21);
        clearsPath(game, 4);
        const auto length = game.snakeLength();
        const auto front = inFrontOf(game);
        game.setFood(front);
        game.setFoodKind(snakecraft::FoodKind::Poison);
        require(game.tick(), "poison tick should not end the run");
        require(game.combo() == 0, "poison should break combo");
        require(game.snakeLength() <= length, "poison should not grow the snake");
    }

    {
        snakecraft::Game game(30, 16, 23);
        require(game.lives() == 3, "new games start with three lives");
        game.setWrapWorld(false);
        game.setSnake({ { 0, 8 }, { 1, 8 }, { 2, 8 } });
        game.handle(snakecraft::Action::MoveLeft);
        game.tick();
        require(game.lives() == 2, "edge hit spends a life");
        require(!game.isGameOver(), "two lives remain");
    }

    std::cout << "all core mechanics passed\n";
    return 0;
}