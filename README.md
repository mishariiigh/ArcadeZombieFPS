# ArcadeZombieFPS

A lightweight, arcade-style first-person zombie survival shooter for macOS, built in **C++17 + Raylib 5.5 + CMake**. No heavy game engine required.

## Controls

| Key | Action |
|-----|--------|
| WASD | Move |
| Mouse | Look around |
| Left Click | Shoot |
| R | Reload |
| Left Shift | Sprint |
| Space | Jump |
| ESC | Pause / resume |
| R (on game over) | Restart |

## Build & run

Requirements: macOS, CMake 3.16+, Clang, Raylib (fetched automatically via CMake FetchContent).

```bash
cd ~/Projects/ArcadeZombieFPS
mkdir -p build && cd build
cmake .. -G "Unix Makefiles"
make -j4
./ArcadeZombieFPS
```

## Project structure

```
src/          main.cpp, game.cpp, player.cpp, weapon.cpp, zombie.cpp, world.cpp
include/      game.h, player.h, weapon.h, zombie.h, world.h
assets/       models/, textures/, sounds/  (empty; all geometry is procedural)
CMakeLists.txt
README.md
.gitignore
```

## Design notes

- First-person 3D camera (raylib `Camera3D`), WASD + mouse look, sprint, jump, gravity.
- 2D HUD weapon at screen bottom with recoil, sway, muzzle flash, ammo counter, and reload.
- Raycast hits from screen center against procedural low-poly zombies; damage, death, removal, score.
- Zombie AI: detection range, chase, contact attack that damages the player.
- Flat ground + box obstacles; player vs. obstacle AABB collision with wall sliding; ground contact.
- Endless waves with increasing count/speed/health; game-over and restart.
- Built with Raylib 5.5 fetched via CMake `FetchContent` (no Homebrew required for Raylib).

## Performance

Target 60 FPS. Simple procedural geometry, no per-frame allocations in the hot loop, raycast hit tests only on shot. Tested on an Intel Iris Pro / i7-4770HQ / 16 GB macOS Sonoma machine — launches and runs at interactive frame rates.
# ArcadeZombieFPS
