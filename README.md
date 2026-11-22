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

- First-person 3D camera (raylib `Camera3D`), WASD + mouse/trackpad look, sprint, jump, gravity.
- 3D first-person viewmodel (gun + arms) attached to the camera with recoil, sway, reload bob, muzzle flash, ammo counter, and reload.
- Raycast hits from screen center against procedural low-poly zombies; damage, death, removal, score.
- Zombie AI: detection range, chase, contact attack that damages the player.
- Survival arena: ground slab, perimeter walls keeping play in bounds, concrete barriers, crates, abandoned vehicles, barricades and industrial buildings with AABB collision; player vs. obstacle AABB collision with wall sliding; ground contact.
- Endless waves with increasing count/speed/health; game-over and restart.
- Built with Raylib 5.5 fetched via CMake `FetchContent` (no Homebrew required for Raylib).

## Performance

Target 60 FPS at 1280×720. Simple procedural geometry, no per-frame allocations in the hot loop, raycast hit tests only on shot, shared meshes loaded once.

Measured on an Intel Iris Pro / i7-4770HQ / 16 GB macOS Sonoma machine, rendering the arena (ground + perimeter walls + cover obstacles), the 3D viewmodel and animated zombies:

| Zombies | FPS |
|---------|-----|
| 1 | 58.9 |
| 5 | 59.6 |
| 10 | 59.6 |
| 20 | 58.3 |

These are standalone benchmark numbers (arena + viewmodel + zombies); the full game adds player, HUD and audio overhead, so in-game FPS is marginally lower but remains near target.

## First-person arms and hands

The procedural viewmodel arms were thickened (arm cylinder radius 0.024 → 0.045) and given hand blocks around the weapon grip, with wider shoulders (±0.50) and natural shoulder-to-hand proportions. Both arms connect to the grip and move with sway/recoil/reload. No external assets were added.

## Survival arena

The arena was upgraded from flat ground + scattered boxes to a cohesive low-poly survival play area:
- Larger concrete ground slab with a darker base slab defining the arena edge.
- Perimeter walls at the arena boundary keep the player (and zombies) inside the playable area.
- Cover obstacles: concrete barriers, crates, abandoned vehicles, barricades.
- Larger industrial buildings around the edges.
- All obstacles have AABB collision; player collision and wall sliding are preserved.
- Zombies can still chase and separate; obstacles are sparse enough to avoid trapping them.

## Controls note (A/D strafing)

A and D strafe camera-relative left and right (W/S forward/back, diagonals normalized). Strafing was verified at the code level: `right = cross(up, forward)`, D adds `right`, A subtracts `right`, the combined vector is normalized, and open-space strafing is not cancelled by collision. Interactive verification was not performed in this headless session; build and run the game to confirm WASD + mouse/trackpad feel.

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

## Audio system

A centralized `AudioManager` (header-only, `include/audio.h`) built on Raylib 5.5's built-in audio
provides sound effects and a looping ambient track. The audio device is initialized once in
`Game::Init()` and shut down in `Game::Shutdown()`; all resources are loaded once and released on
shutdown, with idempotent init/shutdown (no leaks or double-frees).

Master, SFX, and music volume controls are available (`SetMasterVolume`, `SetSfxVolume`,
`SetMusicVolume`) and affect the appropriate channels.

Sound effects respond to existing gameplay events:
- Gunshot, reload, empty-magazine click (weapon)
- Zombie idle growl (occasional), zombie attack, zombie death
- Player hurt
- Wave start, game over

A looping ambient drone plays during active gameplay and pauses/stops on pause, game-over, and
shutdown.

### Audio assets (assets/sounds/)

| File | Use | Source | License |
|------|-----|--------|---------|
| gunshot.wav | Weapon fire | https://opengameart.org/content/random-gunfire-sfx (iamoneabe) | CC0 1.0 |
| reload.wav | Weapon reload | https://opengameart.org/content/gun-reload-sounds (SpringySpringo) | CC0 1.0 |
| zombie_moans.ogg | Zombie idle growl | https://opengameart.org/content/zombie-moans (Darsycho) | CC0 1.0 |
| zombie_pain.ogg | Zombie death / player hurt | https://opengameart.org/content/zombie-pain (Vinrax) | CC0 1.0 |
| ambient_loop.ogg | Ambient loop | https://opengameart.org/content/loopable-dungeon-ambience (JaggedStone) | CC0 1.0 |
| empty_click.wav | Empty magazine | self-authored | CC0 1.0 |
| wave_start.wav | New wave chime | self-authored | CC0 1.0 |
| gameover.wav | Game over tone | self-authored | CC0 1.0 |

Full source/license details: `assets/sounds/CREDITS`; CC0 legal code: `assets/sounds/LICENSE`.

# ArcadeZombieFPS
