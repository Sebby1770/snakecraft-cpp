# Changelog

All notable changes to Snakecraft C++ are documented in this file.

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