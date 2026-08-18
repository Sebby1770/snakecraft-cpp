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
    case Tile::Sand:
        return 1;
    case Tile::Ore:
        return 8;
    case Tile::Empty:
        return 0;
    }

    return 0;
}

bool isResource(Tile tile)
{
    return tile == Tile::Dirt || tile == Tile::Stone || tile == Tile::Wood || tile == Tile::Sand
        || tile == Tile::Ore;
}

bool isPlaceable(Tile tile)
{
    return tile == Tile::Dirt || tile == Tile::Stone || tile == Tile::Wood || tile == Tile::Sand;
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
    case Tile::Sand:
        return sand;
    case Tile::Ore:
        return ore;
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
    case Tile::Sand:
        ++sand;
        break;
    case Tile::Ore:
        ++ore;
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
    case Tile::Sand:
        if (sand <= 0) {
            return false;
        }
        --sand;
        return true;
    case Tile::Ore:
    case Tile::Empty:
        return false;
    }

    return false;
}

Game::Game(int width, int height, unsigned int seed)
    : width_(std::max(width, kMinWidth))
    , height_(std::max(height, kMinHeight))
    , world_(static_cast<std::size_t>(width_ * height_), Tile::Empty)
    , biomes_(static_cast<std::size_t>(width_ * height_), Biome::Forest)
    , rng_(seed)
    , seed_(seed)
{
    reset();
}

void Game::reset()
{
    world_.assign(static_cast<std::size_t>(width_ * height_), Tile::Empty);
    biomes_.assign(static_cast<std::size_t>(width_ * height_), Biome::Forest);
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
    foodKind_ = FoodKind::Regular;
    wrapWorld_ = false;
    combo_ = 0;
    bestCombo_ = 0;
    foodsEaten_ = 0;
    lives_ = 3;
    undoStack_.clear();
    message_.clear();

    rng_.seed(seed_);
    generateWorld();

    const Point start { width_ / 2, height_ / 2 };
    snake_.push_back(start);
    snake_.push_back({ start.x - 1, start.y });
    snake_.push_back({ start.x - 2, start.y });

    carveSafeArea(start);
    spawnFood();
    setMessage("Forest, cave, and desert biomes ahead. Mine ore for big points.");
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
    case Action::SaveGame:
    case Action::LoadGame:
        return;
    case Action::ToggleWrap:
        wrapWorld_ = !wrapWorld_;
        setMessage(wrapWorld_ ? "World wraps. Edges are now portals." : "World edges are solid again.");
        return;
    case Action::Undo:
        undo();
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
    case Action::SaveGame:
    case Action::LoadGame:
    case Action::ToggleWrap:
    case Action::Undo:
    case Action::Quit:
        break;
    }
}

bool Game::tick()
{
    if (paused_ || gameOver_ || quitRequested_) {
        return false;
    }

    pushUndo();
    direction_ = pendingDirection_;
    const Point next = advance(snake_.front(), direction_);
    const bool eating = next == food_;

    if (!inside(next)) {
        return loseLife("You hit the edge of the world.");
    }

    if (isBlocked(next)) {
        return loseLife("Terrain collision. Mine before you move next time.");
    }

    const auto bodyEnd = eating ? snake_.end() : std::prev(snake_.end());
    if (std::find(snake_.begin(), bodyEnd, next) != bodyEnd) {
        return loseLife("You crossed your own trail.");
    }

    snake_.push_front(next);

    if (eating) {
        ++foodsEaten_;
        if (foodKind_ == FoodKind::Poison) {
            combo_ = 0;
            snake_.pop_back();
            if (snake_.size() > 3) {
                snake_.pop_back();
            }
            score_ = std::max(0, score_ - 5);
            setMessage("Poison apple. The snake shrank.");
        } else {
            ++combo_;
            bestCombo_ = std::max(bestCombo_, combo_);
            score_ += foodScore() + std::max(0, combo_ - 1) * 2;
            if (foodKind_ == FoodKind::Golden) {
                setMessage("Golden bite! Combo x" + std::to_string(combo_) + ".");
            } else {
                setMessage("Food collected. Combo x" + std::to_string(combo_) + ".");
            }
        }
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
        << "  Combo x" << combo_
        << "  Lives " << lives_
        << "  Mined " << minedBlocks_
        << "  Built " << builtBlocks_
        << (wrapWorld_ ? "  WRAP" : "") << '\n';

    out << "Inventory  Dirt " << inventory_.dirt
        << "  Stone " << inventory_.stone
        << "  Wood " << inventory_.wood
        << "  Sand " << inventory_.sand
        << "  Ore " << inventory_.ore
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
                out << (foodKind_ == FoodKind::Golden ? '$'
                        : foodKind_ == FoodKind::Poison ? 'x' : '@');
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

    out << "WASD move  Space mine  E build  Tab block  T wrap  U undo  5 save  9 load  P pause  R restart  Q quit\n";

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

FoodKind Game::foodKind() const
{
    return foodKind_;
}

bool Game::wrapWorld() const
{
    return wrapWorld_;
}

int Game::combo() const
{
    return combo_;
}

int Game::bestCombo() const
{
    return bestCombo_;
}

int Game::foodsEaten() const
{
    return foodsEaten_;
}

int Game::lives() const
{
    return lives_;
}

bool Game::canUndo() const
{
    return !undoStack_.empty();
}

Point Game::pointAhead() const
{
    return advance(snake_.front(), direction_);
}

Direction Game::direction() const
{
    return direction_;
}

Direction Game::pendingDirection() const
{
    return pendingDirection_;
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

unsigned int Game::seed() const
{
    return seed_;
}

Tile Game::tileAt(Point point) const
{
    if (!inside(point)) {
        return Tile::Stone;
    }

    return world_[static_cast<std::size_t>(index(point))];
}

Biome Game::biomeAt(Point point) const
{
    if (!inside(point)) {
        return Biome::Forest;
    }

    return biomes_[static_cast<std::size_t>(index(point))];
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

    pushUndo();
    const Point target = advance(snake_.front(), direction_);
    if (!inside(target)) {
        undoStack_.pop_back();
        setMessage("Nothing to mine beyond the world edge.");
        return false;
    }

    const Tile tile = tileAt(target);
    if (!isResource(tile)) {
        undoStack_.pop_back();
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

    pushUndo();
    const Point target = advance(snake_.front(), direction_);
    if (!inside(target)) {
        undoStack_.pop_back();
        setMessage("Cannot build beyond the world edge.");
        return false;
    }

    if (target == food_ || containsSnake(target) || tileAt(target) != Tile::Empty) {
        undoStack_.pop_back();
        setMessage("Need an empty tile in front to build.");
        return false;
    }

    if (!isPlaceable(selectedBlock_)) {
        undoStack_.pop_back();
        setMessage("Ore is collectible only. Cycle to dirt, stone, wood, or sand.");
        return false;
    }

    if (!inventory_.spend(selectedBlock_)) {
        undoStack_.pop_back();
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

void Game::setFoodKind(FoodKind kind)
{
    foodKind_ = kind;
}

void Game::setWrapWorld(bool enabled)
{
    wrapWorld_ = enabled;
}

bool Game::undo()
{
    if (undoStack_.empty()) {
        setMessage("Nothing to rewind.");
        return false;
    }

    const UndoFrame frame = undoStack_.back();
    undoStack_.pop_back();
    world_ = frame.world;
    snake_ = frame.snake;
    food_ = frame.food;
    foodKind_ = frame.foodKind;
    direction_ = frame.direction;
    pendingDirection_ = frame.pendingDirection;
    inventory_ = frame.inventory;
    selectedBlock_ = frame.selectedBlock;
    score_ = frame.score;
    minedBlocks_ = frame.minedBlocks;
    builtBlocks_ = frame.builtBlocks;
    ticks_ = frame.ticks;
    combo_ = frame.combo;
    bestCombo_ = frame.bestCombo;
    foodsEaten_ = frame.foodsEaten;
    lives_ = frame.lives;
    wrapWorld_ = frame.wrapWorld;
    gameOver_ = frame.gameOver;
    paused_ = false;
    quitRequested_ = false;
    setMessage("Rewound one beat.");
    return true;
}

void Game::pushUndo()
{
    UndoFrame frame;
    frame.world = world_;
    frame.snake = snake_;
    frame.food = food_;
    frame.foodKind = foodKind_;
    frame.direction = direction_;
    frame.pendingDirection = pendingDirection_;
    frame.inventory = inventory_;
    frame.selectedBlock = selectedBlock_;
    frame.score = score_;
    frame.minedBlocks = minedBlocks_;
    frame.builtBlocks = builtBlocks_;
    frame.ticks = ticks_;
    frame.combo = combo_;
    frame.bestCombo = bestCombo_;
    frame.foodsEaten = foodsEaten_;
    frame.lives = lives_;
    frame.wrapWorld = wrapWorld_;
    frame.gameOver = gameOver_;
    undoStack_.push_back(std::move(frame));
    if (undoStack_.size() > 24) {
        undoStack_.erase(undoStack_.begin());
    }
}

int Game::foodScore() const
{
    if (foodKind_ == FoodKind::Golden) {
        return 25;
    }
    if (foodKind_ == FoodKind::Poison) {
        return 0;
    }
    return 10;
}

bool Game::loseLife(const std::string& reason)
{
    if (lives_ > 1) {
        --lives_;
        combo_ = 0;
        respawnAfterHit();
        setMessage(reason + " Lives left: " + std::to_string(lives_) + ".");
        return false;
    }

    lives_ = 0;
    gameOver_ = true;
    setMessage(reason + " Press R to restart.");
    return false;
}

void Game::respawnAfterHit()
{
    const Point start { width_ / 2, height_ / 2 };
    snake_.clear();
    snake_.push_back(start);
    snake_.push_back({ start.x - 1, start.y });
    snake_.push_back({ start.x - 2, start.y });
    direction_ = Direction::Right;
    pendingDirection_ = Direction::Right;
    carveSafeArea(start);
    if (food_ == start || containsSnake(food_)) {
        spawnFood();
    }
}

void Game::setSnake(const std::deque<Point>& snake)
{
    snake_ = snake;
}

void Game::syncBiomesFromColumns()
{
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            biomes_[static_cast<std::size_t>(index({ x, y }))] = biomeForColumn(x);
        }
    }
}

void Game::restoreState(
    int score,
    int minedBlocks,
    int builtBlocks,
    int ticks,
    Inventory inventory,
    Direction direction,
    Direction pendingDirection,
    Tile selectedBlock)
{
    score_ = score;
    minedBlocks_ = minedBlocks;
    builtBlocks_ = builtBlocks;
    ticks_ = ticks;
    inventory_ = inventory;
    direction_ = direction;
    pendingDirection_ = pendingDirection;
    selectedBlock_ = selectedBlock;
    paused_ = false;
    gameOver_ = false;
    quitRequested_ = false;
    message_.clear();
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
    Point next = point;
    switch (direction) {
    case Direction::Up:
        next = { point.x, point.y - 1 };
        break;
    case Direction::Down:
        next = { point.x, point.y + 1 };
        break;
    case Direction::Left:
        next = { point.x - 1, point.y };
        break;
    case Direction::Right:
        next = { point.x + 1, point.y };
        break;
    }

    if (wrapWorld_) {
        next.x = (next.x % width_ + width_) % width_;
        next.y = (next.y % height_ + height_) % height_;
    }

    return next;
}

int Game::index(Point point) const
{
    return point.y * width_ + point.x;
}

Biome Game::biomeForColumn(int x) const
{
    const int forestEnd = width_ / 3;
    const int caveEnd = (width_ * 2) / 3;

    if (x < forestEnd) {
        return Biome::Forest;
    }

    if (x < caveEnd) {
        return Biome::Cave;
    }

    return Biome::Desert;
}

Tile Game::rollTerrainTile(Biome biome, int roll) const
{
    switch (biome) {
    case Biome::Forest:
        if (roll < 8) {
            return Tile::Wood;
        }
        if (roll < 12) {
            return Tile::Dirt;
        }
        break;
    case Biome::Cave:
        if (roll < 9) {
            return Tile::Stone;
        }
        if (roll < 11) {
            return Tile::Ore;
        }
        if (roll < 13) {
            return Tile::Dirt;
        }
        break;
    case Biome::Desert:
        if (roll < 10) {
            return Tile::Sand;
        }
        if (roll < 13) {
            return Tile::Dirt;
        }
        break;
    }

    return Tile::Empty;
}

void Game::generateWorld()
{
    std::uniform_int_distribution<int> hundred(0, 99);

    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const Biome biome = biomeForColumn(x);
            const int roll = hundred(rng_);
            const Point point { x, y };

            biomes_[static_cast<std::size_t>(index(point))] = biome;
            world_[static_cast<std::size_t>(index(point))] = rollTerrainTile(biome, roll);
        }
    }

    std::uniform_int_distribution<int> xdist(1, width_ - 2);
    std::uniform_int_distribution<int> ydist(1, height_ - 2);
    std::uniform_int_distribution<int> lengthDist(3, 7);

    const int veins = std::max(4, (width_ * height_) / 120);
    for (int vein = 0; vein < veins; ++vein) {
        Point cursor { xdist(rng_), ydist(rng_) };
        const Biome biome = biomeForColumn(cursor.x);
        Tile tile = Tile::Dirt;
        if (biome == Biome::Cave) {
            tile = (vein % 2 == 0) ? Tile::Ore : Tile::Stone;
        } else if (biome == Biome::Forest) {
            tile = Tile::Wood;
        } else {
            tile = Tile::Sand;
        }

        const int length = lengthDist(rng_);

        for (int step = 0; step < length; ++step) {
            if (inside(cursor)) {
                world_[static_cast<std::size_t>(index(cursor))] = tile;
                biomes_[static_cast<std::size_t>(index(cursor))] = biomeForColumn(cursor.x);
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
    if ((foodsEaten_ + 1) % 7 == 0) {
        foodKind_ = FoodKind::Poison;
    } else if ((foodsEaten_ + 1) % 5 == 0) {
        foodKind_ = FoodKind::Golden;
    } else {
        foodKind_ = FoodKind::Regular;
    }
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
        selectedBlock_ = Tile::Sand;
        break;
    case Tile::Sand:
    case Tile::Ore:
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
    case Tile::Sand:
        return ':';
    case Tile::Ore:
        return '*';
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
    case Tile::Sand:
        return "sand";
    case Tile::Ore:
        return "ore";
    }

    return "unknown";
}

std::string Game::biomeName(Biome biome) const
{
    switch (biome) {
    case Biome::Forest:
        return "forest";
    case Biome::Cave:
        return "cave";
    case Biome::Desert:
        return "desert";
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
