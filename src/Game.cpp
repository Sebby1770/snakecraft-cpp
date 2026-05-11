#include "Game.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <utility>

namespace snakecraft {
namespace {

constexpr int kMinWidth = 24;
constexpr int kMinHeight = 12;

int tileScore(Tile tile)
{
    switch (tile) {
    case Tile::Dirt:
        return 1;
    case Tile::Stone:
        return 3;
    case Tile::Wood:
        return 2;
    case Tile::Empty:
        return 0;
    }

    return 0;
}

bool isResource(Tile tile)
{
    return tile == Tile::Dirt || tile == Tile::Stone || tile == Tile::Wood;
}

} // namespace

int Inventory::count(Tile tile) const
{
    switch (tile) {
    case Tile::Dirt:
        return dirt;
    case Tile::Stone:
        return stone;
    case Tile::Wood:
        return wood;
    case Tile::Empty:
        return 0;
    }

    return 0;
}

void Inventory::add(Tile tile)
{
    switch (tile) {
    case Tile::Dirt:
        ++dirt;
        break;
    case Tile::Stone:
        ++stone;
        break;
    case Tile::Wood:
        ++wood;
        break;
    case Tile::Empty:
        break;
    }
}

bool Inventory::spend(Tile tile)
{
    switch (tile) {
    case Tile::Dirt:
        if (dirt <= 0) {
            return false;
        }
        --dirt;
        return true;
    case Tile::Stone:
        if (stone <= 0) {
            return false;
        }
        --stone;
        return true;
    case Tile::Wood:
        if (wood <= 0) {
            return false;
        }
        --wood;
        return true;
    case Tile::Empty:
        return false;
    }

    return false;
}

Game::Game(int width, int height, unsigned int seed)
    : width_(std::max(width, kMinWidth))
    , height_(std::max(height, kMinHeight))
    , world_(static_cast<std::size_t>(width_ * height_), Tile::Empty)
    , rng_(seed)
    , seed_(seed)
{
    reset();
}

void Game::reset()
{
    world_.assign(static_cast<std::size_t>(width_ * height_), Tile::Empty);
    snake_.clear();
    direction_ = Direction::Right;
    pendingDirection_ = Direction::Right;
    inventory_ = {};
    selectedBlock_ = Tile::Dirt;
    score_ = 0;
    minedBlocks_ = 0;
    builtBlocks_ = 0;
    ticks_ = 0;
    paused_ = false;
    gameOver_ = false;
    quitRequested_ = false;
    message_.clear();

    rng_.seed(seed_);
    generateWorld();

    const Point start { width_ / 2, height_ / 2 };
    snake_.push_back(start);
    snake_.push_back({ start.x - 1, start.y });
    snake_.push_back({ start.x - 2, start.y });

    carveSafeArea(start);
    spawnFood();
    setMessage("Mine, build, and keep moving.");
}

void Game::handle(Action action)
{
    switch (action) {
    case Action::None:
        return;
    case Action::Quit:
        quitRequested_ = true;
        return;
    case Action::Restart:
        reset();
        return;
    case Action::Pause:
        paused_ = !paused_;
        setMessage(paused_ ? "Paused." : "Back in motion.");
        return;
    default:
        break;
    }

    if (gameOver_) {
        return;
    }

    switch (action) {
    case Action::MoveUp:
        if (!isOpposite(Direction::Up)) {
            pendingDirection_ = Direction::Up;
        }
        break;
    case Action::MoveDown:
        if (!isOpposite(Direction::Down)) {
            pendingDirection_ = Direction::Down;
        }
        break;
    case Action::MoveLeft:
        if (!isOpposite(Direction::Left)) {
            pendingDirection_ = Direction::Left;
        }
        break;
    case Action::MoveRight:
        if (!isOpposite(Direction::Right)) {
            pendingDirection_ = Direction::Right;
        }
        break;
    case Action::Mine:
        mineAhead();
        break;
    case Action::Build:
        buildAhead();
        break;
    case Action::CycleBlock:
        cycleSelectedBlock();
        break;
    case Action::None:
    case Action::Pause:
    case Action::Restart:
    case Action::Quit:
        break;
    }
}

bool Game::tick()
{
    if (paused_ || gameOver_ || quitRequested_) {
        return false;
    }

    direction_ = pendingDirection_;
    const Point next = advance(snake_.front(), direction_);
    const bool eating = next == food_;

    if (!inside(next)) {
        gameOver_ = true;
        setMessage("You hit the edge of the world. Press R to restart.");
        return false;
    }

    if (isBlocked(next)) {
        gameOver_ = true;
        setMessage("Terrain collision. Mine before you move next time.");
        return false;
    }

    const auto bodyEnd = eating ? snake_.end() : std::prev(snake_.end());
    if (std::find(snake_.begin(), bodyEnd, next) != bodyEnd) {
        gameOver_ = true;
        setMessage("You crossed your own trail. Press R to restart.");
        return false;
    }

    snake_.push_front(next);

    if (eating) {
        score_ += 10;
        setMessage("Food collected. Snake grew.");
        spawnFood();
    } else {
        snake_.pop_back();
    }

    ++ticks_;
    return true;
}

std::string Game::render() const
{
    std::ostringstream out;

    out << "Snakecraft C++  Score " << std::setw(4) << score_
        << "  Length " << std::setw(2) << snake_.size()
        << "  Facing " << directionName()
        << "  Mined " << minedBlocks_
        << "  Built " << builtBlocks_ << '\n';

    out << "Inventory  Dirt " << inventory_.dirt
        << "  Stone " << inventory_.stone
        << "  Wood " << inventory_.wood
        << "  Selected " << tileName(selectedBlock_) << '\n';

    out << '+';
    for (int x = 0; x < width_; ++x) {
        out << '-';
    }
    out << "+\n";

    for (int y = 0; y < height_; ++y) {
        out << '|';
        for (int x = 0; x < width_; ++x) {
            const Point point { x, y };

            if (point == snake_.front()) {
                out << 'O';
                continue;
            }

            const auto body = std::find(std::next(snake_.begin()), snake_.end(), point);
            if (body != snake_.end()) {
                out << 'o';
                continue;
            }

            if (point == food_) {
                out << '@';
                continue;
            }

            out << tileGlyph(tileAt(point));
        }
        out << "|\n";
    }

    out << '+';
    for (int x = 0; x < width_; ++x) {
        out << '-';
    }
    out << "+\n";

    out << "WASD/arrows move  Space mine  E build  Tab block  P pause  R restart  Q quit\n";

    if (gameOver_) {
        out << "GAME OVER: ";
    } else if (paused_) {
        out << "PAUSED: ";
    } else {
        out << "Status: ";
    }

    out << message_ << '\n';
    return out.str();
}

bool Game::isGameOver() const
{
    return gameOver_;
}

bool Game::isQuitRequested() const
{
    return quitRequested_;
}

bool Game::isPaused() const
{
    return paused_;
}

int Game::score() const
{
    return score_;
}

int Game::minedBlocks() const
{
    return minedBlocks_;
}

int Game::builtBlocks() const
{
    return builtBlocks_;
}

int Game::width() const
{
    return width_;
}

int Game::height() const
{
    return height_;
}

std::size_t Game::snakeLength() const
{
    return snake_.size();
}

Point Game::snakeHead() const
{
    return snake_.front();
}

Point Game::food() const
{
    return food_;
}

Point Game::pointAhead() const
{
    return advance(snake_.front(), direction_);
}

Direction Game::direction() const
{
    return direction_;
}

Tile Game::selectedBlock() const
{
    return selectedBlock_;
}

const Inventory& Game::inventory() const
{
    return inventory_;
}

const std::deque<Point>& Game::snake() const
{
    return snake_;
}

const std::string& Game::message() const
{
    return message_;
}

int Game::ticks() const
{
    return ticks_;
}

Tile Game::tileAt(Point point) const
{
    if (!inside(point)) {
        return Tile::Stone;
    }

    return world_[static_cast<std::size_t>(index(point))];
}

bool Game::containsSnake(Point point) const
{
    return std::find(snake_.begin(), snake_.end(), point) != snake_.end();
}

bool Game::mineAhead()
{
    if (paused_) {
        return false;
    }

    const Point target = advance(snake_.front(), direction_);
    if (!inside(target)) {
        setMessage("Nothing to mine beyond the world edge.");
        return false;
    }

    const Tile tile = tileAt(target);
    if (!isResource(tile)) {
        setMessage("No block in front to mine.");
        return false;
    }

    world_[static_cast<std::size_t>(index(target))] = Tile::Empty;
    inventory_.add(tile);
    score_ += tileScore(tile);
    ++minedBlocks_;
    setMessage("Mined " + tileName(tile) + ".");
    return true;
}

bool Game::buildAhead()
{
    if (paused_) {
        return false;
    }

    const Point target = advance(snake_.front(), direction_);
    if (!inside(target)) {
        setMessage("Cannot build beyond the world edge.");
        return false;
    }

    if (target == food_ || containsSnake(target) || tileAt(target) != Tile::Empty) {
        setMessage("Need an empty tile in front to build.");
        return false;
    }

    if (!inventory_.spend(selectedBlock_)) {
        setMessage("No " + tileName(selectedBlock_) + " blocks in inventory.");
        return false;
    }

    world_[static_cast<std::size_t>(index(target))] = selectedBlock_;
    ++builtBlocks_;
    ++score_;
    setMessage("Placed " + tileName(selectedBlock_) + ".");
    return true;
}

void Game::setTile(Point point, Tile tile)
{
    if (!inside(point)) {
        return;
    }

    world_[static_cast<std::size_t>(index(point))] = tile;
}

void Game::setFood(Point point)
{
    if (!inside(point)) {
        return;
    }

    food_ = point;
    setTile(point, Tile::Empty);
}

bool Game::inside(Point point) const
{
    return point.x >= 0 && point.y >= 0 && point.x < width_ && point.y < height_;
}

bool Game::isBlocked(Point point) const
{
    return tileAt(point) != Tile::Empty;
}

bool Game::isOpposite(Direction next) const
{
    return (direction_ == Direction::Up && next == Direction::Down)
        || (direction_ == Direction::Down && next == Direction::Up)
        || (direction_ == Direction::Left && next == Direction::Right)
        || (direction_ == Direction::Right && next == Direction::Left);
}

Point Game::advance(Point point, Direction direction) const
{
    switch (direction) {
    case Direction::Up:
        return { point.x, point.y - 1 };
    case Direction::Down:
        return { point.x, point.y + 1 };
    case Direction::Left:
        return { point.x - 1, point.y };
    case Direction::Right:
        return { point.x + 1, point.y };
    }

    return point;
}

int Game::index(Point point) const
{
    return point.y * width_ + point.x;
}

void Game::generateWorld()
{
    std::uniform_int_distribution<int> hundred(0, 99);

    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const int roll = hundred(rng_);
            Tile tile = Tile::Empty;

            if (roll < 7) {
                tile = Tile::Dirt;
            } else if (roll < 10) {
                tile = Tile::Stone;
            } else if (roll < 12) {
                tile = Tile::Wood;
            }

            world_[static_cast<std::size_t>(index({ x, y }))] = tile;
        }
    }

    std::uniform_int_distribution<int> xdist(1, width_ - 2);
    std::uniform_int_distribution<int> ydist(1, height_ - 2);
    std::uniform_int_distribution<int> lengthDist(3, 7);

    const int veins = std::max(4, (width_ * height_) / 120);
    for (int vein = 0; vein < veins; ++vein) {
        Point cursor { xdist(rng_), ydist(rng_) };
        const Tile tile = (vein % 3 == 0) ? Tile::Stone : Tile::Dirt;
        const int length = lengthDist(rng_);

        for (int step = 0; step < length; ++step) {
            if (inside(cursor)) {
                world_[static_cast<std::size_t>(index(cursor))] = tile;
            }

            const int turn = hundred(rng_) % 4;
            if (turn == 0) {
                ++cursor.x;
            } else if (turn == 1) {
                --cursor.x;
            } else if (turn == 2) {
                ++cursor.y;
            } else {
                --cursor.y;
            }

            cursor.x = std::clamp(cursor.x, 1, width_ - 2);
            cursor.y = std::clamp(cursor.y, 1, height_ - 2);
        }
    }
}

void Game::carveSafeArea(Point center)
{
    for (int y = center.y - 3; y <= center.y + 3; ++y) {
        for (int x = center.x - 5; x <= center.x + 5; ++x) {
            const Point point { x, y };
            if (inside(point)) {
                world_[static_cast<std::size_t>(index(point))] = Tile::Empty;
            }
        }
    }
}

void Game::spawnFood()
{
    std::vector<Point> candidates;
    candidates.reserve(static_cast<std::size_t>(width_ * height_));

    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const Point point { x, y };
            if (tileAt(point) == Tile::Empty && !containsSnake(point)) {
                candidates.push_back(point);
            }
        }
    }

    if (candidates.empty()) {
        gameOver_ = true;
        setMessage("No open space left. You reshaped the whole world.");
        return;
    }

    std::uniform_int_distribution<std::size_t> pick(0, candidates.size() - 1);
    food_ = candidates[pick(rng_)];
}

void Game::cycleSelectedBlock()
{
    switch (selectedBlock_) {
    case Tile::Dirt:
        selectedBlock_ = Tile::Stone;
        break;
    case Tile::Stone:
        selectedBlock_ = Tile::Wood;
        break;
    case Tile::Wood:
    case Tile::Empty:
        selectedBlock_ = Tile::Dirt;
        break;
    }

    setMessage("Selected " + tileName(selectedBlock_) + ".");
}

void Game::setMessage(std::string message)
{
    message_ = std::move(message);
}

char Game::tileGlyph(Tile tile) const
{
    switch (tile) {
    case Tile::Empty:
        return ' ';
    case Tile::Dirt:
        return '#';
    case Tile::Stone:
        return '%';
    case Tile::Wood:
        return '|';
    }

    return '?';
}

std::string Game::tileName(Tile tile) const
{
    switch (tile) {
    case Tile::Empty:
        return "empty";
    case Tile::Dirt:
        return "dirt";
    case Tile::Stone:
        return "stone";
    case Tile::Wood:
        return "wood";
    }

    return "unknown";
}

std::string Game::directionName() const
{
    switch (direction_) {
    case Direction::Up:
        return "up";
    case Direction::Down:
        return "down";
    case Direction::Left:
        return "left";
    case Direction::Right:
        return "right";
    }

    return "unknown";
}

} // namespace snakecraft
