# Changelog

All notable changes to Snakecraft C++ are documented in this file.

## [0.4.0] - 2026-08-18

### Added
- Three lives: a wall or self-hit respawns the snake instead of ending the run immediately.
- Poison food (`x`) shrinks the snake and breaks combo.
- Terminal `--seed N` for reproducible worlds.

## [0.3.0] - 2026-08-18

### Added
- Wrap mode (`T`) turns world edges into portals instead of instant death.
- Combo scoring: consecutive food bites add bonus points; HUD shows the streak.
- Golden food (`$`) every fifth bite, worth 25 points plus combo.
- Undo (`U`) rewinds the last move, mine, or build (and even a fatal tick).
- Tests for wrap, combo/golden scoring, and undo.

### Changed
- Terminal and SDL keymaps include wrap and undo.

## [0.2.0] - 2026-07-06

### Added
- Biome-aware world generation with forest, cave, and desert regions.
- New `sand` and `ore` tiles; ore awards higher mining score.
- Persistent top-five high score table stored in `~/.snakecraft/highscores.txt`.
- Save and load support for full game state (`F5` / `F9` in SDL, `5` / `9` in terminal).
- Expanded CTest coverage for ore mining, biomes, save/load, and score tracking.
- GitHub Actions CI for build and test on macOS and Ubuntu.

### Changed
- Inventory panel and HUD now show sand and ore counts.
- SDL board background tints now reflect biome type.
- README controls and gameplay notes updated for the new systems.

## [0.1.0] - 2026-05-11

### Added
- SDL2 desktop app with animated snake, food, HUD, and overlays.
- Terminal fallback build.
- Mining, building, inventory, and procedural terrain.
- Initial CTest suite for core mechanics.