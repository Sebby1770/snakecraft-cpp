#include "Game.hpp"
#include "SaveManager.hpp"
#include "ScoreStore.hpp"

#include <SDL.h>
#include <SDL_ttf.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using snakecraft::Action;
using snakecraft::Biome;
using snakecraft::Direction;
using snakecraft::Game;
using snakecraft::Point;
using snakecraft::ScoreStore;
using snakecraft::SaveManager;
using snakecraft::Tile;

constexpr int kCellSize = 24;
constexpr int kBoardX = 32;
constexpr int kBoardY = 118;
constexpr int kPanelGap = 28;
constexpr int kPanelWidth = 238;
constexpr int kWindowWidth = kBoardX + (42 * kCellSize) + kPanelGap + kPanelWidth + 32;
constexpr int kWindowHeight = 704;

struct Color {
    Uint8 r = 0;
    Uint8 g = 0;
    Uint8 b = 0;
    Uint8 a = 255;
};

struct FontSet {
    TTF_Font* title = nullptr;
    TTF_Font* body = nullptr;
    TTF_Font* small = nullptr;
};

class SdlSession {
public:
    SdlSession()
    {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
            throw std::runtime_error(SDL_GetError());
        }

        if (TTF_Init() != 0) {
            throw std::runtime_error(TTF_GetError());
        }
    }

    ~SdlSession()
    {
        TTF_Quit();
        SDL_Quit();
    }

    SdlSession(const SdlSession&) = delete;
    SdlSession& operator=(const SdlSession&) = delete;
};

struct SdlDeleter {
    void operator()(SDL_Window* value) const { SDL_DestroyWindow(value); }
    void operator()(SDL_Renderer* value) const { SDL_DestroyRenderer(value); }
    void operator()(SDL_Texture* value) const { SDL_DestroyTexture(value); }
    void operator()(SDL_Surface* value) const { SDL_FreeSurface(value); }
};

using WindowPtr = std::unique_ptr<SDL_Window, SdlDeleter>;
using RendererPtr = std::unique_ptr<SDL_Renderer, SdlDeleter>;
using TexturePtr = std::unique_ptr<SDL_Texture, SdlDeleter>;
using SurfacePtr = std::unique_ptr<SDL_Surface, SdlDeleter>;

void setColor(SDL_Renderer* renderer, Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

SDL_Color toSdl(Color color)
{
    return SDL_Color { color.r, color.g, color.b, color.a };
}

void fillRect(SDL_Renderer* renderer, SDL_Rect rect, Color color)
{
    setColor(renderer, color);
    SDL_RenderFillRect(renderer, &rect);
}

void drawRect(SDL_Renderer* renderer, SDL_Rect rect, Color color)
{
    setColor(renderer, color);
    SDL_RenderDrawRect(renderer, &rect);
}

void drawLine(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, Color color)
{
    setColor(renderer, color);
    SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
}

void fillCircle(SDL_Renderer* renderer, int centerX, int centerY, int radius, Color color)
{
    setColor(renderer, color);
    for (int y = -radius; y <= radius; ++y) {
        const int span = static_cast<int>(std::sqrt((radius * radius) - (y * y)));
        SDL_RenderDrawLine(renderer, centerX - span, centerY + y, centerX + span, centerY + y);
    }
}

void drawCircle(SDL_Renderer* renderer, int centerX, int centerY, int radius, Color color)
{
    setColor(renderer, color);
    int x = radius - 1;
    int y = 0;
    int dx = 1;
    int dy = 1;
    int err = dx - (radius << 1);

    while (x >= y) {
        SDL_RenderDrawPoint(renderer, centerX + x, centerY + y);
        SDL_RenderDrawPoint(renderer, centerX + y, centerY + x);
        SDL_RenderDrawPoint(renderer, centerX - y, centerY + x);
        SDL_RenderDrawPoint(renderer, centerX - x, centerY + y);
        SDL_RenderDrawPoint(renderer, centerX - x, centerY - y);
        SDL_RenderDrawPoint(renderer, centerX - y, centerY - x);
        SDL_RenderDrawPoint(renderer, centerX + y, centerY - x);
        SDL_RenderDrawPoint(renderer, centerX + x, centerY - y);

        if (err <= 0) {
            ++y;
            err += dy;
            dy += 2;
        }

        if (err > 0) {
            --x;
            dx += 2;
            err += dx - (radius << 1);
        }
    }
}

void fillRoundedRect(SDL_Renderer* renderer, SDL_Rect rect, int radius, Color color)
{
    radius = std::max(0, std::min(radius, std::min(rect.w, rect.h) / 2));

    fillRect(renderer, { rect.x + radius, rect.y, rect.w - (radius * 2), rect.h }, color);
    fillRect(renderer, { rect.x, rect.y + radius, rect.w, rect.h - (radius * 2) }, color);
    fillCircle(renderer, rect.x + radius, rect.y + radius, radius, color);
    fillCircle(renderer, rect.x + rect.w - radius - 1, rect.y + radius, radius, color);
    fillCircle(renderer, rect.x + radius, rect.y + rect.h - radius - 1, radius, color);
    fillCircle(renderer, rect.x + rect.w - radius - 1, rect.y + rect.h - radius - 1, radius, color);
}

std::optional<std::filesystem::path> findFont()
{
    const std::array<std::filesystem::path, 8> candidates {
        "/System/Library/Fonts/Avenir Next.ttc",
        "/System/Library/Fonts/SFCompact.ttf",
        "/System/Library/Fonts/HelveticaNeue.ttc",
        "/Library/Fonts/Arial Unicode.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "C:/Windows/Fonts/arial.ttf",
    };

    for (const auto& path : candidates) {
        if (std::filesystem::exists(path)) {
            return path;
        }
    }

    return std::nullopt;
}

FontSet loadFonts()
{
    const auto fontPath = findFont();
    if (!fontPath) {
        throw std::runtime_error("Could not find a system font for SDL_ttf.");
    }

    FontSet fonts;
    fonts.title = TTF_OpenFont(fontPath->string().c_str(), 28);
    fonts.body = TTF_OpenFont(fontPath->string().c_str(), 18);
    fonts.small = TTF_OpenFont(fontPath->string().c_str(), 14);

    if (!fonts.title || !fonts.body || !fonts.small) {
        throw std::runtime_error(TTF_GetError());
    }

    return fonts;
}

void closeFonts(FontSet& fonts)
{
    if (fonts.title) {
        TTF_CloseFont(fonts.title);
    }
    if (fonts.body) {
        TTF_CloseFont(fonts.body);
    }
    if (fonts.small) {
        TTF_CloseFont(fonts.small);
    }
}

void drawText(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y, Color color)
{
    SurfacePtr surface(TTF_RenderUTF8_Blended(font, text.c_str(), toSdl(color)));
    if (!surface) {
        return;
    }

    TexturePtr texture(SDL_CreateTextureFromSurface(renderer, surface.get()));
    if (!texture) {
        return;
    }

    SDL_Rect dest { x, y, surface->w, surface->h };
    SDL_RenderCopy(renderer, texture.get(), nullptr, &dest);
}

void drawTextCentered(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, SDL_Rect area, Color color)
{
    int width = 0;
    int height = 0;
    TTF_SizeUTF8(font, text.c_str(), &width, &height);
    drawText(renderer, font, text, area.x + ((area.w - width) / 2), area.y + ((area.h - height) / 2), color);
}

std::string tileName(Tile tile)
{
    switch (tile) {
    case Tile::Dirt:
        return "Dirt";
    case Tile::Stone:
        return "Stone";
    case Tile::Wood:
        return "Wood";
    case Tile::Sand:
        return "Sand";
    case Tile::Ore:
        return "Ore";
    case Tile::Empty:
        return "Empty";
    }

    return "Unknown";
}

Color biomeTint(Biome biome)
{
    switch (biome) {
    case Biome::Forest:
        return { 28, 48, 40, 255 };
    case Biome::Cave:
        return { 24, 30, 38, 255 };
    case Biome::Desert:
        return { 52, 42, 30, 255 };
    }

    return { 32, 50, 43, 255 };
}

Color tileColor(Tile tile)
{
    switch (tile) {
    case Tile::Dirt:
        return { 141, 101, 78, 255 };
    case Tile::Stone:
        return { 144, 153, 159, 255 };
    case Tile::Wood:
        return { 190, 131, 76, 255 };
    case Tile::Sand:
        return { 214, 181, 112, 255 };
    case Tile::Ore:
        return { 96, 176, 214, 255 };
    case Tile::Empty:
        return { 32, 50, 43, 255 };
    }

    return { 255, 0, 255, 255 };
}

int hashCell(int x, int y, int salt)
{
    int value = x * 7349 + y * 9151 + salt * 2659;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    return std::abs(value);
}

SDL_Rect cellRect(Point point, int inset = 0)
{
    return {
        kBoardX + (point.x * kCellSize) + inset,
        kBoardY + (point.y * kCellSize) + inset,
        kCellSize - (inset * 2),
        kCellSize - (inset * 2),
    };
}

Action actionFromKey(SDL_Keycode key)
{
    switch (key) {
    case SDLK_w:
    case SDLK_UP:
        return Action::MoveUp;
    case SDLK_s:
    case SDLK_DOWN:
        return Action::MoveDown;
    case SDLK_a:
    case SDLK_LEFT:
        return Action::MoveLeft;
    case SDLK_d:
    case SDLK_RIGHT:
        return Action::MoveRight;
    case SDLK_SPACE:
        return Action::Mine;
    case SDLK_e:
        return Action::Build;
    case SDLK_TAB:
        return Action::CycleBlock;
    case SDLK_p:
        return Action::Pause;
    case SDLK_r:
        return Action::Restart;
    case SDLK_F5:
        return Action::SaveGame;
    case SDLK_F9:
        return Action::LoadGame;
    case SDLK_q:
    case SDLK_ESCAPE:
        return Action::Quit;
    default:
        return Action::None;
    }
}

std::chrono::milliseconds frameDelayForScore(int score)
{
    const int speedup = std::min(score / 30, 60);
    return std::chrono::milliseconds(145 - speedup);
}

void drawTile(SDL_Renderer* renderer, Point point, Tile tile, Biome biome)
{
    const SDL_Rect rect = cellRect(point, 1);

    if (tile == Tile::Empty) {
        fillRect(renderer, rect, biomeTint(biome));
        return;
    }

    fillRoundedRect(renderer, rect, 4, tileColor(tile));

    if (tile == Tile::Dirt) {
        for (int i = 0; i < 4; ++i) {
            const int px = rect.x + 4 + (hashCell(point.x, point.y, i) % std::max(1, rect.w - 8));
            const int py = rect.y + 4 + (hashCell(point.y, point.x, i) % std::max(1, rect.h - 8));
            fillCircle(renderer, px, py, 1, { 88, 62, 49, 150 });
        }
    }

    if (tile == Tile::Stone) {
        const int offset = 5 + (hashCell(point.x, point.y, 2) % 6);
        drawLine(renderer, rect.x + 4, rect.y + offset, rect.x + rect.w - 6, rect.y + rect.h - 7, { 103, 112, 120, 180 });
        drawLine(renderer, rect.x + rect.w - 7, rect.y + 5, rect.x + 7, rect.y + rect.h - 5, { 190, 197, 201, 110 });
    }

    if (tile == Tile::Wood) {
        fillRoundedRect(renderer, { rect.x + 4, rect.y + 3, 4, rect.h - 6 }, 2, { 122, 78, 45, 150 });
        fillRoundedRect(renderer, { rect.x + rect.w - 8, rect.y + 3, 4, rect.h - 6 }, 2, { 229, 167, 97, 130 });
        drawCircle(renderer, rect.x + (rect.w / 2), rect.y + (rect.h / 2), 5, { 129, 81, 45, 150 });
    }

    if (tile == Tile::Sand) {
        for (int i = 0; i < 5; ++i) {
            const int px = rect.x + 3 + (hashCell(point.x, point.y, i + 4) % std::max(1, rect.w - 6));
            const int py = rect.y + 3 + (hashCell(point.y, point.x, i + 5) % std::max(1, rect.h - 6));
            fillCircle(renderer, px, py, 1, { 255, 236, 180, 170 });
        }
    }

    if (tile == Tile::Ore) {
        drawCircle(renderer, rect.x + (rect.w / 2), rect.y + (rect.h / 2), 6, { 58, 132, 168, 220 });
        fillCircle(renderer, rect.x + (rect.w / 2) - 2, rect.y + (rect.h / 2) - 2, 2, { 210, 244, 255, 220 });
    }
}

void drawBoard(SDL_Renderer* renderer, const Game& game)
{
    const SDL_Rect board {
        kBoardX - 6,
        kBoardY - 6,
        (game.width() * kCellSize) + 12,
        (game.height() * kCellSize) + 12,
    };

    fillRoundedRect(renderer, board, 12, { 20, 34, 31, 255 });
    drawRect(renderer, board, { 93, 134, 109, 180 });

    for (int y = 0; y < game.height(); ++y) {
        for (int x = 0; x < game.width(); ++x) {
            drawTile(renderer, { x, y }, game.tileAt({ x, y }), game.biomeAt({ x, y }));
        }
    }

    setColor(renderer, { 77, 101, 85, 78 });
    for (int x = 0; x <= game.width(); ++x) {
        const int px = kBoardX + (x * kCellSize);
        SDL_RenderDrawLine(renderer, px, kBoardY, px, kBoardY + (game.height() * kCellSize));
    }

    for (int y = 0; y <= game.height(); ++y) {
        const int py = kBoardY + (y * kCellSize);
        SDL_RenderDrawLine(renderer, kBoardX, py, kBoardX + (game.width() * kCellSize), py);
    }
}

void drawFood(SDL_Renderer* renderer, Point food, double seconds)
{
    const SDL_Rect rect = cellRect(food);
    const int pulse = static_cast<int>(std::sin(seconds * 6.0) * 2.0);
    const int centerX = rect.x + (rect.w / 2);
    const int centerY = rect.y + (rect.h / 2);

    fillCircle(renderer, centerX + 1, centerY + 2, 9 + pulse, { 233, 79, 96, 255 });
    fillCircle(renderer, centerX - 3, centerY, 5, { 255, 126, 132, 165 });
    fillRect(renderer, { centerX, rect.y + 3, 3, 7 }, { 94, 62, 39, 255 });
    fillRoundedRect(renderer, { centerX + 4, rect.y + 4, 8, 5 }, 3, { 114, 190, 93, 255 });
}

void drawSnakeHead(SDL_Renderer* renderer, Point point, Direction direction)
{
    const SDL_Rect rect = cellRect(point, 2);
    fillRoundedRect(renderer, rect, 9, { 164, 236, 98, 255 });
    drawRect(renderer, rect, { 66, 137, 76, 210 });

    const int cx = rect.x + (rect.w / 2);
    const int cy = rect.y + (rect.h / 2);
    int eyeAX = cx - 5;
    int eyeAY = cy - 5;
    int eyeBX = cx - 5;
    int eyeBY = cy + 5;
    int tongueX = rect.x + rect.w + 2;
    int tongueY = cy;
    int tongueTipX = tongueX + 7;
    int tongueTipY = tongueY;

    if (direction == Direction::Left) {
        eyeAX = cx + 5;
        eyeBX = cx + 5;
        tongueX = rect.x - 2;
        tongueTipX = tongueX - 7;
    } else if (direction == Direction::Up) {
        eyeAX = cx - 5;
        eyeAY = cy + 5;
        eyeBX = cx + 5;
        eyeBY = cy + 5;
        tongueX = cx;
        tongueY = rect.y - 2;
        tongueTipX = tongueX;
        tongueTipY = tongueY - 7;
    } else if (direction == Direction::Down) {
        eyeAX = cx - 5;
        eyeAY = cy - 5;
        eyeBX = cx + 5;
        eyeBY = cy - 5;
        tongueX = cx;
        tongueY = rect.y + rect.h + 2;
        tongueTipX = tongueX;
        tongueTipY = tongueY + 7;
    }

    fillCircle(renderer, eyeAX, eyeAY, 3, { 18, 30, 25, 255 });
    fillCircle(renderer, eyeBX, eyeBY, 3, { 18, 30, 25, 255 });
    drawLine(renderer, tongueX, tongueY, tongueTipX, tongueTipY, { 230, 62, 92, 255 });
}

void drawSnake(SDL_Renderer* renderer, const Game& game)
{
    const auto& snake = game.snake();
    for (auto it = snake.rbegin(); it != snake.rend(); ++it) {
        const auto distanceFromHead = static_cast<int>(std::distance(it, snake.rend())) - 1;
        const int alpha = std::clamp(235 - (distanceFromHead * 7), 120, 235);
        const SDL_Rect rect = cellRect(*it, 3);

        fillRoundedRect(renderer, rect, 9, { 68, 191, 111, static_cast<Uint8>(alpha) });
        fillRoundedRect(renderer, { rect.x + 4, rect.y + 4, rect.w - 8, rect.h - 8 }, 7, { 95, 222, 129, static_cast<Uint8>(std::min(255, alpha + 20)) });
    }

    drawSnakeHead(renderer, game.snakeHead(), game.direction());
}

void drawHud(SDL_Renderer* renderer, const Game& game, const FontSet& fonts, const ScoreStore& scores)
{
    fillRect(renderer, { 0, 0, kWindowWidth, kWindowHeight }, { 18, 29, 27, 255 });
    fillRoundedRect(renderer, { 24, 22, kWindowWidth - 48, 74 }, 12, { 238, 234, 217, 255 });
    drawText(renderer, fonts.title, "Snakecraft", 44, 38, { 27, 45, 38, 255 });

    std::ostringstream stats;
    stats << "Score " << game.score()
          << "   Length " << game.snakeLength()
          << "   Mined " << game.minedBlocks()
          << "   Built " << game.builtBlocks();
    drawText(renderer, fonts.body, stats.str(), 252, 44, { 47, 62, 56, 255 });
    drawText(renderer, fonts.small, "Best " + std::to_string(scores.bestScore()), 252, 68, { 79, 92, 84, 255 });

    const int panelX = kBoardX + (game.width() * kCellSize) + kPanelGap;
    fillRoundedRect(renderer, { panelX, kBoardY - 6, kPanelWidth, (game.height() * kCellSize) + 12 }, 12, { 238, 234, 217, 255 });

    drawText(renderer, fonts.body, "Inventory", panelX + 22, kBoardY + 18, { 31, 45, 39, 255 });

    const std::array<std::pair<Tile, int>, 5> rows {
        std::pair { Tile::Dirt, game.inventory().dirt },
        std::pair { Tile::Stone, game.inventory().stone },
        std::pair { Tile::Wood, game.inventory().wood },
        std::pair { Tile::Sand, game.inventory().sand },
        std::pair { Tile::Ore, game.inventory().ore },
    };

    int y = kBoardY + 58;
    for (const auto& [tile, count] : rows) {
        fillRoundedRect(renderer, { panelX + 22, y, 26, 26 }, 5, tileColor(tile));
        drawText(renderer, fonts.small, tileName(tile), panelX + 60, y + 2, { 41, 52, 48, 255 });
        drawText(renderer, fonts.small, std::to_string(count), panelX + 190, y + 2, { 41, 52, 48, 255 });
        y += 42;
    }

    y += 10;
    drawText(renderer, fonts.body, "Selected", panelX + 22, y, { 31, 45, 39, 255 });
    fillRoundedRect(renderer, { panelX + 22, y + 38, 46, 46 }, 7, tileColor(game.selectedBlock()));
    drawText(renderer, fonts.body, tileName(game.selectedBlock()), panelX + 82, y + 48, { 41, 52, 48, 255 });

    y += 116;
    drawText(renderer, fonts.body, "Actions", panelX + 22, y, { 31, 45, 39, 255 });
    drawText(renderer, fonts.small, "Move  WASD / arrows", panelX + 22, y + 36, { 67, 78, 72, 255 });
    drawText(renderer, fonts.small, "Mine  Space", panelX + 22, y + 62, { 67, 78, 72, 255 });
    drawText(renderer, fonts.small, "Build  E", panelX + 22, y + 88, { 67, 78, 72, 255 });
    drawText(renderer, fonts.small, "Block  Tab", panelX + 22, y + 114, { 67, 78, 72, 255 });
    drawText(renderer, fonts.small, "Save  F5", panelX + 22, y + 140, { 67, 78, 72, 255 });
    drawText(renderer, fonts.small, "Load  F9", panelX + 22, y + 166, { 67, 78, 72, 255 });

    const std::string message = game.message().empty() ? "Ready." : game.message();
    drawText(renderer, fonts.small, message, kBoardX, kWindowHeight - 42, { 218, 228, 211, 255 });
}

void drawAimHint(SDL_Renderer* renderer, const Game& game)
{
    const Point target = game.pointAhead();
    if (target.x < 0 || target.y < 0 || target.x >= game.width() || target.y >= game.height()) {
        return;
    }

    const SDL_Rect rect = cellRect(target, 2);
    drawRect(renderer, rect, { 255, 236, 150, 150 });
    drawRect(renderer, { rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2 }, { 255, 236, 150, 90 });
}

void drawOverlay(SDL_Renderer* renderer, const Game& game, const FontSet& fonts, const ScoreStore& scores)
{
    if (!game.isPaused() && !game.isGameOver()) {
        return;
    }

    fillRect(renderer, { 0, 0, kWindowWidth, kWindowHeight }, { 7, 13, 12, 150 });

    const SDL_Rect modal {
        (kWindowWidth - 500) / 2,
        (kWindowHeight - 220) / 2,
        500,
        220,
    };

    fillRoundedRect(renderer, modal, 14, { 238, 234, 217, 255 });
    drawTextCentered(renderer, fonts.title, game.isGameOver() ? "Run Ended" : "Paused", { modal.x, modal.y + 22, modal.w, 40 }, { 31, 45, 39, 255 });
    drawTextCentered(renderer, fonts.body, game.message(), { modal.x + 28, modal.y + 78, modal.w - 56, 30 }, { 64, 75, 68, 255 });

    if (game.isGameOver() && !scores.entries().empty()) {
        std::ostringstream leaderboard;
        leaderboard << "Top score " << scores.entries().front().score;
        if (scores.entries().size() > 1) {
            leaderboard << "  |  #2 " << scores.entries()[1].score;
        }
        drawTextCentered(renderer, fonts.small, leaderboard.str(), { modal.x, modal.y + 118, modal.w, 24 }, { 79, 88, 83, 255 });
    }

    drawTextCentered(renderer, fonts.small, "R restart    P resume    Q quit", { modal.x, modal.y + 158, modal.w, 26 }, { 79, 88, 83, 255 });
}

void renderFrame(SDL_Renderer* renderer, const Game& game, const FontSet& fonts, const ScoreStore& scores, double seconds)
{
    setColor(renderer, { 18, 29, 27, 255 });
    SDL_RenderClear(renderer);
    drawHud(renderer, game, fonts, scores);
    drawBoard(renderer, game);
    drawAimHint(renderer, game);
    drawFood(renderer, game.food(), seconds);
    drawSnake(renderer, game);
    drawOverlay(renderer, game, fonts, scores);
}

void saveScreenshot(SDL_Renderer* renderer, const std::filesystem::path& path)
{
    SurfacePtr surface(SDL_CreateRGBSurfaceWithFormat(0, kWindowWidth, kWindowHeight, 32, SDL_PIXELFORMAT_ARGB8888));
    if (!surface) {
        throw std::runtime_error(SDL_GetError());
    }

    if (SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888, surface->pixels, surface->pitch) != 0) {
        throw std::runtime_error(SDL_GetError());
    }

    if (SDL_SaveBMP(surface.get(), path.string().c_str()) != 0) {
        throw std::runtime_error(SDL_GetError());
    }
}

struct CliOptions {
    bool screenshot = false;
    std::filesystem::path screenshotPath;
};

CliOptions parseOptions(int argc, char** argv)
{
    CliOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--screenshot" && i + 1 < argc) {
            options.screenshot = true;
            options.screenshotPath = argv[++i];
        }
    }
    return options;
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const CliOptions options = parseOptions(argc, argv);
        const SdlSession session;
        FontSet fonts = loadFonts();

        const Uint32 windowFlags = options.screenshot ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN;
        WindowPtr window(SDL_CreateWindow(
            "Snakecraft",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            kWindowWidth,
            kWindowHeight,
            windowFlags));

        if (!window) {
            throw std::runtime_error(SDL_GetError());
        }

        const Uint32 rendererFlags = options.screenshot
            ? SDL_RENDERER_SOFTWARE
            : (SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

        RendererPtr renderer(SDL_CreateRenderer(window.get(), -1, rendererFlags));
        if (!renderer) {
            renderer.reset(SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_SOFTWARE));
        }

        if (!renderer) {
            throw std::runtime_error(SDL_GetError());
        }

        SDL_SetRenderDrawBlendMode(renderer.get(), SDL_BLENDMODE_BLEND);

        const auto dataDir = [&]() {
            const auto home = std::getenv("HOME");
            if (home != nullptr) {
                return std::filesystem::path(home) / ".snakecraft";
            }
            return std::filesystem::current_path() / ".snakecraft";
        }();
        std::filesystem::create_directories(dataDir);

        ScoreStore scores((dataDir / "highscores.txt").string());
        SaveManager saves(dataDir / "savegame.txt");

        Game game(42, 22, options.screenshot ? 1770 : static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count()));

        if (options.screenshot) {
            for (int i = 0; i < 3; ++i) {
                game.tick();
            }
            renderFrame(renderer.get(), game, fonts, scores, 1.0);
            SDL_RenderPresent(renderer.get());
            saveScreenshot(renderer.get(), options.screenshotPath);
            closeFonts(fonts);
            return 0;
        }

        auto lastTick = std::chrono::steady_clock::now();
        bool running = true;

        while (running && !game.isQuitRequested()) {
            SDL_Event event {};
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    running = false;
                }

                if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                    const Action action = actionFromKey(event.key.keysym.sym);
                    if (action == Action::SaveGame) {
                        if (saves.save(game)) {
                            game.handle(Action::None);
                        }
                        continue;
                    }

                    if (action == Action::LoadGame) {
                        if (auto loaded = saves.load()) {
                            game = std::move(*loaded);
                        }
                        continue;
                    }

                    game.handle(action);
                }
            }

            const auto now = std::chrono::steady_clock::now();
            if (now - lastTick >= frameDelayForScore(game.score())) {
                const bool wasOver = game.isGameOver();
                game.tick();
                if (!wasOver && game.isGameOver()) {
                    scores.tryAdd(
                        game.score(),
                        static_cast<int>(game.snakeLength()),
                        game.minedBlocks(),
                        game.builtBlocks());
                }
                lastTick = now;
            }

            const double seconds = SDL_GetTicks64() / 1000.0;
            renderFrame(renderer.get(), game, fonts, scores, seconds);
            SDL_RenderPresent(renderer.get());
            SDL_Delay(8);
        }

        closeFonts(fonts);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Snakecraft failed: " << error.what() << '\n';
        return 1;
    }
}
