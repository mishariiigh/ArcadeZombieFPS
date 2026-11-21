#include "game.h"
#include "player.h"
#include "weapon.h"
#include "zombie.h"
#include "world.h"

#include <cmath>
#include "raymath.h"

Game::Game() {
    player = new Player();
    weapon = new Weapon();
    world = new World();
    zombies = new ZombieManager();
}

Game::~Game() {
    delete zombies;
    delete world;
    delete weapon;
    delete player;
}

bool Game::Init() {
    InitWindow(screenWidth, screenHeight, "ArcadeZombieFPS - Zombie Survival FPS");
    SetTargetFPS(60);

    world->Init();
    player->Reset(camera);

    // Load the shared zombie model/animations now that an OpenGL context exists.
    if (!zombies->Init()) {
        // Non-fatal: zombies simply won't render until the asset is available.
    }

    // Generate the 3D first-person viewmodel meshes.
    if (!weapon->Init()) {
        // Non-fatal: weapon will not render in 3D until meshes are available.
    }

    // Initialize the audio device and load all sound/music resources once.
    if (!audio.Init()) {
        // Non-fatal: game continues without audio if the device/assets fail.
    }

    Restart();
    return true;
}

void Game::Run() {
    while (!WindowShouldClose()) {
        Update();
        Draw();
    }
    Shutdown();
}

void Game::Shutdown() {
    weapon->Shutdown();
    audio.Shutdown();
    zombies->Shutdown();
    CloseWindow();
}

void Game::Restart() {
    score = 0;
    wave = 1;
    waveTarget = 5;
    zombiesSpawnedThisWave = 0;
    waveTimer = 0.0f;
    spawnInterval = 2.0f;
    interWaveTimer = 0.0f;
    gameOver = false;
    paused = false;

    frameCount = 0;
    fpsTimer = 0.0f;
    fps = 0;

    player->Reset(camera);
    weapon->Reset();
    zombies->Clear();

    // Reset audio state on restart: restart ambient music and signal a new wave start.
    audio.StopMusic();
    audio.PlayMusic(AudioManager::MusicId::Ambient);
    audio.PlaySfx(AudioManager::Sfx::WaveStart);
}

void Game::SpawnWave() {
    // Spawning is handled incrementally in UpdateWave(); kept for API symmetry.
}

void Game::DamagePlayer(int amount) {
    player->TakeDamage(amount);
    if (!player->IsAlive()) {
        gameOver = true;
        EnableCursor();
        audio.StopMusic();
        audio.PlaySfx(AudioManager::Sfx::GameOver);
    }
}

void Game::AddScore(int amount) {
    score += amount;
}

Vector3 Game::RandomSpawnPos() const {
    const float half = 48.0f;
    int side = GetRandomValue(0, 3);
    float x = 0.0f, z = 0.0f;
    switch (side) {
        case 0: x = (float)GetRandomValue((int)-half, (int)half); z = -half; break;
        case 1: x = (float)GetRandomValue((int)-half, (int)half); z =  half; break;
        case 2: x = -half; z = (float)GetRandomValue((int)-half, (int)half); break;
        default: x =  half; z = (float)GetRandomValue((int)-half, (int)half); break;
    }
    return { x, 0.0f, z };
}

bool Game::SpawnPosValid(const Vector3& pos) const {
    if (Vector3Distance(pos, player->Position()) < 9.0f) return false;
    for (const auto& o : world->Obstacles()) {
        if (pos.x >= o.box.min.x && pos.x <= o.box.max.x &&
            pos.y >= o.box.min.y && pos.y <= o.box.max.y &&
            pos.z >= o.box.min.z && pos.z <= o.box.max.z) return false;
    }
    return true;
}

void Game::UpdateWave(float dt) {
    if (interWaveTimer > 0.0f) {
        interWaveTimer -= dt;
        return;
    }

    int health = 2 + wave;
    float speed = 2.0f + (float)(wave - 1) * 0.12f;
    if (speed > 4.5f) speed = 4.5f;

    if (zombiesSpawnedThisWave < waveTarget) {
        waveTimer += dt;
        if (waveTimer >= spawnInterval) {
            int tries = 0;
            Vector3 pos;
            do {
                pos = RandomSpawnPos();
                tries++;
            } while (!SpawnPosValid(pos) && tries < 12);

            if (SpawnPosValid(pos)) {
                zombies->Spawn(pos, health, speed);
                zombiesSpawnedThisWave++;
            }
            waveTimer = 0.0f;
        }
    } else if (zombies->AliveCount() == 0) {
        wave++;
        waveTarget = 5 + (wave - 1) * 3;
        zombiesSpawnedThisWave = 0;
        waveTimer = 0.0f;
        interWaveTimer = 2.5f;
        spawnInterval = (spawnInterval > 0.7f) ? spawnInterval - 0.15f : 0.7f;
    }
}

void Game::Update() {
    float dt = GetFrameTime();

    // FPS tracking (updated twice per second)
    frameCount++;
    fpsTimer += dt;
    if (fpsTimer >= 0.5f) {
        fps = (int)(frameCount / fpsTimer);
        frameCount = 0;
        fpsTimer = 0.0f;
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        paused = !paused;
        if (paused) audio.PauseMusic();
        else audio.ResumeMusic();
    }

    if (paused || gameOver) {
        EnableCursor();
        if (gameOver && IsKeyPressed(KEY_R)) {
            Restart();
            DisableCursor();
        }
        return;
    }

    DisableCursor();

    // Keep the ambient music stream alive during active play.
    audio.Update();

    player->Update(dt, camera, *world);
    weapon->Update(dt, camera);
    world->Update(dt);

    int dmg = zombies->Update(dt, player->Position(), player->Radius());
    if (dmg > 0) {
        audio.PlaySfx(AudioManager::Sfx::ZombieAttack);
        audio.PlaySfx(AudioManager::Sfx::PlayerHurt);
    }
    DamagePlayer(dmg);

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        ShotResult sr = weapon->Shoot(camera);
        if (sr.fired) {
            audio.PlaySfx(AudioManager::Sfx::Gunshot);
            int pts = zombies->TestHit(sr.ray);
            if (pts > 0) {
                AddScore(pts);
                audio.PlaySfx(AudioManager::Sfx::ZombieDeath);
            }
            // Recoil view kick: pitch camera up when firing
            player->AddPitch(weapon->ViewKick());
        } else if (weapon->AmmoInClip() == 0 && !weapon->IsReloading()) {
            audio.PlaySfx(AudioManager::Sfx::EmptyClick);
        }
    }

    if (IsKeyPressed(KEY_R)) {
        bool wasReloading = weapon->IsReloading();
        weapon->Reload();
        if (!wasReloading && weapon->IsReloading()) {
            audio.PlaySfx(AudioManager::Sfx::Reload);
        }
    }

    int waveBefore = wave;
    UpdateWave(dt);
    if (wave > waveBefore) {
        audio.PlaySfx(AudioManager::Sfx::WaveStart);
    }
}

void Game::Draw() {
    const Color fogColor{ 92, 112, 96, 255 };
    ClearBackground(fogColor);

    BeginMode3D(camera);
        world->Draw();
        zombies->Draw();
        weapon->Draw3D(camera);
    EndMode3D();

    // 2D HUD (crosshair and text; the weapon is now rendered in 3D above)

    int cx = screenWidth / 2;
    int cy = screenHeight / 2;

    // Crosshair
    DrawLine(cx - 10, cy, cx + 10, cy, BLACK);
    DrawLine(cx, cy - 10, cx, cy + 10, BLACK);
    DrawLine(cx - 6, cy, cx + 6, cy, WHITE);
    DrawLine(cx, cy - 6, cx, cy + 6, WHITE);

    // HUD text
    DrawText(TextFormat("SCORE  %d", score), 16, 16, 24, RAYWHITE);
    DrawText(TextFormat("HEALTH  %d", player->Health()), 16, 50, 24, RAYWHITE);
    DrawText(TextFormat("WAVE  %d", wave), 16, 84, 24, RAYWHITE);
    DrawText(TextFormat("FPS  %d", fps), screenWidth - 120, 16, 24, RAYWHITE);

    int ammoY = 120;
    if (weapon->IsReloading()) {
        DrawText("RELOADING...", 16, ammoY, 24, Color{ 255, 120, 120, 255 });
    } else {
        DrawText(TextFormat("AMMO  %d / %d", weapon->AmmoInClip(), weapon->AmmoReserve()),
                 16, ammoY, 24, RAYWHITE);
    }

    if (interWaveTimer > 0.0f) {
        const char* msg = TextFormat("WAVE %d CLEARED", wave - 1);
        int w = MeasureText(msg, 40);
        DrawText(msg, cx - w / 2, screenHeight / 3, 40, Color{ 200, 220, 140, 255 });
    }

    if (paused) {
        const char* msg = "PAUSED";
        int w = MeasureText(msg, 40);
        DrawText(msg, cx - w / 2, screenHeight / 2 - 20, 40, Color{ 220, 220, 220, 255 });
        const char* hint = "Press ESC to resume";
        int hw = MeasureText(hint, 20);
        DrawText(hint, cx - hw / 2, screenHeight / 2 + 30, 20, Color{ 180, 180, 180, 255 });
    }

    if (gameOver) {
        const char* msg = "GAME OVER";
        int w = MeasureText(msg, 50);
        DrawText(msg, cx - w / 2, screenHeight / 2 - 40, 50, Color{ 255, 80, 80, 255 });
        DrawText(TextFormat("FINAL SCORE  %d", score), cx - MeasureText(TextFormat("FINAL SCORE  %d", score), 24) / 2,
                 screenHeight / 2 + 20, 24, RAYWHITE);
        const char* hint = "Press R to restart";
        int hw = MeasureText(hint, 24);
        DrawText(hint, cx - hw / 2, screenHeight / 2 + 60, 24, Color{ 200, 200, 200, 255 });
    }

    EndDrawing();
}
