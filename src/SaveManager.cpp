#include "SaveManager.hpp"

#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace snakecraft {
namespace {

constexpr char kMagic[] = "SNAKECRAFT1";

std::string tileToken(Tile tile)
{
    switch (tile) {
    case Tile::Empty:
        return ".";
    case Tile::Dirt:
        return "d";
    case Tile::Stone:
        return "s";
    case Tile::Wood:
        return "w";
    case Tile::Sand:
        return "a";
    case Tile::Ore:
        return "o";
    }

    return "?";
}

std::optional<Tile> tileFromToken(char token)
{
    switch (token) {
    case '.':
        return Tile::Empty;
    case 'd':
        return Tile::Dirt;
    case 's':
        return Tile::Stone;
    case 'w':
        return Tile::Wood;
    case 'a':
        return Tile::Sand;
    case 'o':
        return Tile::Ore;
    default:
        return std::nullopt;
    }
}

std::optional<Direction> directionFromToken(char token)
{
    switch (token) {
    case 'U':
        return Direction::Up;
    case 'D':
        return Direction::Down;
    case 'L':
        return Direction::Left;
    case 'R':
        return Direction::Right;
    default:
        return std::nullopt;
    }
}

std::string directionToken(Direction direction)
{
    switch (direction) {
    case Direction::Up:
        return "U";
    case Direction::Down:
        return "D";
    case Direction::Left:
        return "L";
    case Direction::Right:
        return "R";
    }

    return "R";
}

std::string selectedToken(Tile tile)
{
    return tileToken(tile);
}

std::vector<std::string> split(const std::string& value, char delimiter)
{
    std::vector<std::string> parts;
    std::stringstream stream(value);
    std::string part;

    while (std::getline(stream, part, delimiter)) {
        parts.push_back(part);
    }

    return parts;
}

} // namespace

SaveManager::SaveManager(std::filesystem::path path)
    : path_(std::move(path))
{
}

bool SaveManager::save(const Game& game) const
{
    std::ostringstream out;
    out << kMagic << '\n'
        << game.width() << ' ' << game.height() << ' ' << game.seed() << '\n'
        << game.score() << ' ' << game.minedBlocks() << ' ' << game.builtBlocks() << ' ' << game.ticks() << '\n'
        << directionToken(game.direction()) << ' ' << directionToken(game.pendingDirection()) << ' '
        << selectedToken(game.selectedBlock()) << '\n'
        << game.inventory().dirt << ' ' << game.inventory().stone << ' ' << game.inventory().wood << ' '
        << game.inventory().sand << ' ' << game.inventory().ore << '\n'
        << game.food().x << ' ' << game.food().y << '\n';

    for (int y = 0; y < game.height(); ++y) {
        for (int x = 0; x < game.width(); ++x) {
            out << tileToken(game.tileAt({ x, y }));
        }
        out << '\n';
    }

    for (const auto& segment : game.snake()) {
        out << segment.x << ',' << segment.y << ';';
    }
    out << '\n';

    std::ofstream file(path_, std::ios::trunc);
    if (!file) {
        return false;
    }

    file << out.str();
    return static_cast<bool>(file);
}

std::optional<Game> SaveManager::load() const
{
    std::ifstream file(path_);
    if (!file) {
        return std::nullopt;
    }

    std::string magic;
    std::getline(file, magic);
    if (magic != kMagic) {
        return std::nullopt;
    }

    int width = 0;
    int height = 0;
    unsigned int seed = 0;
    file >> width >> height >> seed;

    int score = 0;
    int mined = 0;
    int built = 0;
    int ticks = 0;
    file >> score >> mined >> built >> ticks;

    std::string directionTokenValue;
    std::string pendingTokenValue;
    std::string selectedTokenValue;
    file >> directionTokenValue >> pendingTokenValue >> selectedTokenValue;

    Inventory inventory;
    file >> inventory.dirt >> inventory.stone >> inventory.wood >> inventory.sand >> inventory.ore;

    int foodX = 0;
    int foodY = 0;
    file >> foodX >> foodY;
    file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    Game game(width, height, seed);
    game.restoreState(
        score,
        mined,
        built,
        ticks,
        inventory,
        *directionFromToken(directionTokenValue[0]),
        *directionFromToken(pendingTokenValue[0]),
        *tileFromToken(selectedTokenValue[0]));

    for (int y = 0; y < height; ++y) {
        std::string row;
        std::getline(file, row);
        if (static_cast<int>(row.size()) < width) {
            return std::nullopt;
        }

        for (int x = 0; x < width; ++x) {
            const auto tile = tileFromToken(row[static_cast<std::size_t>(x)]);
            if (!tile) {
                return std::nullopt;
            }
            game.setTile({ x, y }, *tile);
        }
    }

    std::string snakeLine;
    std::getline(file, snakeLine);
    std::deque<Point> snake;
    for (const auto& segment : split(snakeLine, ';')) {
        if (segment.empty()) {
            continue;
        }

        const auto coords = split(segment, ',');
        if (coords.size() != 2) {
            return std::nullopt;
        }

        snake.push_back({ std::stoi(coords[0]), std::stoi(coords[1]) });
    }

    if (snake.empty()) {
        return std::nullopt;
    }

    game.setSnake(snake);
    game.setFood({ foodX, foodY });
    game.syncBiomesFromColumns();
    return game;
}

bool SaveManager::exists() const
{
    return std::filesystem::exists(path_);
}

const std::filesystem::path& SaveManager::path() const
{
    return path_;
}

} // namespace snakecraft