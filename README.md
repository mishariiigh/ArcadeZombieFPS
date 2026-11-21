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
assets/       models/zombie.glb (+ LICENSE), textures/, sounds/
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

## Zombie model asset

Zombies are rendered with a low-poly 3D glTF 2.0 model (`assets/models/zombie.glb`) instead of primitive cubes. The model is shared across all zombie instances (loaded once, unloaded on shutdown) and is animated with skeletal animation: idle (breathing), walk, attack (punch), and death.

- Source: `blocky_09_zombie.glb` from https://github.com/Vuena/free-low-poly-characters-cc0 (file `GLB/blocky_09_zombie.glb`), author Vuena.
- License: CC0 1.0 Universal (public domain); full text in `assets/models/LICENSE`. No attribution required; free for personal and commercial use.
- Animations (all from the same asset): `idle`, `walk`, `punch` (used as attack), `death`. Origin at feet, ~1.8 m tall, 22 bones.

Animation state is driven by the existing zombie gameplay logic (chasing → walk, contact attack → attack/punch, death → death + fade/sink). Detection, movement, damage, and wave mechanics are unchanged.

## First-person weapon

The 2D weapon HUD is replaced with a 3D first-person viewmodel (gun + arms) rendered inside the 3D camera and attached to the player's view. The viewmodel is built from lightweight procedural meshes (unit cube + unit cylinder generated once with `GenMeshCube`/`GenMeshCylinder`), so no external asset or license is required. It supports:

- Idle positioning with subtle breathing sway.
- Firing recoil (visual kick back/down, synchronized with the existing camera pitch kick).
- Muzzle flash at the barrel tip.
- Reload bob animation (weapon lowers and rises during reload).
- Recoil/sway that does not alter aiming, damage, raycasting, or hit detection.

All weapon gameplay (ammo, cooldown, fire rate, reload, `ViewKick`) is unchanged.

# ArcadeZombieFPS
