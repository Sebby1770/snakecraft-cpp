#pragma once

#include <cstddef>
#include <deque>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace snakecraft {

struct Point {
    int x = 0;
    int y = 0;

    friend bool operator==(const Point& lhs, const Point& rhs) = default;
};

enum class Tile {
    Empty,
    Dirt,
    Stone,
    Wood
};

enum class Direction {
    Up,
    Down,
    Left,
    Right
};

enum class Action {
    None,
    MoveUp,
    MoveDown,
    MoveLeft,
    MoveRight,
    Mine,
    Build,
    CycleBlock,
    Pause,
    Restart,
    Quit
};

struct Inventory {
    int dirt = 0;
    int stone = 0;
    int wood = 0;

    [[nodiscard]] int count(Tile tile) const;
    void add(Tile tile);
    [[nodiscard]] bool spend(Tile tile);
};

class Game {
public:
    Game(int width = 42, int height = 22, unsigned int seed = std::random_device{}());

    void reset();
    void handle(Action action);
    bool tick();

    [[nodiscard]] std::string render() const;
    [[nodiscard]] bool isGameOver() const;
    [[nodiscard]] bool isQuitRequested() const;
    [[nodiscard]] bool isPaused() const;
    [[nodiscard]] int score() const;
    [[nodiscard]] int minedBlocks() const;
    [[nodiscard]] int builtBlocks() const;
    [[nodiscard]] int width() const;
    [[nodiscard]] int height() const;
    [[nodiscard]] std::size_t snakeLength() const;
    [[nodiscard]] Point snakeHead() const;
    [[nodiscard]] Direction direction() const;
    [[nodiscard]] Tile selectedBlock() const;
    [[nodiscard]] const Inventory& inventory() const;
    [[nodiscard]] Tile tileAt(Point point) const;
    [[nodiscard]] bool containsSnake(Point point) const;

    bool mineAhead();
    bool buildAhead();

    void setTile(Point point, Tile tile);
    void setFood(Point point);

private:
    [[nodiscard]] bool inside(Point point) const;
    [[nodiscard]] bool isBlocked(Point point) const;
    [[nodiscard]] bool isOpposite(Direction next) const;
    [[nodiscard]] Point advance(Point point, Direction direction) const;
    [[nodiscard]] int index(Point point) const;

    void generateWorld();
    void carveSafeArea(Point center);
    void spawnFood();
    void cycleSelectedBlock();
    void setMessage(std::string message);

    [[nodiscard]] char tileGlyph(Tile tile) const;
    [[nodiscard]] std::string tileName(Tile tile) const;
    [[nodiscard]] std::string directionName() const;

    int width_;
    int height_;
    std::vector<Tile> world_;
    std::deque<Point> snake_;
    Direction direction_ = Direction::Right;
    Direction pendingDirection_ = Direction::Right;
    Point food_;
    std::mt19937 rng_;
    unsigned int seed_;
    Inventory inventory_;
    Tile selectedBlock_ = Tile::Dirt;
    int score_ = 0;
    int minedBlocks_ = 0;
    int builtBlocks_ = 0;
    int ticks_ = 0;
    bool paused_ = false;
    bool gameOver_ = false;
    bool quitRequested_ = false;
    std::string message_;
};

} // namespace snakecraft
