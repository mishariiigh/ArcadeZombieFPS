#pragma once

#include "raylib.h"
#include <vector>
#include <cstdint>

class Player;
class Weapon;
class World;
class ZombieManager;

class Game {
public:
    Game();
    ~Game();

    bool Init();
    void Run();
    void Shutdown();

private:
    void Update();
    void Draw();
    void SpawnWave();
    void UpdateWave(float dt);
    Vector3 RandomSpawnPos() const;
    bool SpawnPosValid(const Vector3& pos) const;
    void Restart();
    void DamagePlayer(int amount);
    void AddScore(int amount);

    Camera3D camera{};
    int screenWidth = 1280;
    int screenHeight = 720;
    bool paused = false;
    bool gameOver = false;

    int score = 0;
    float waveTimer = 0.0f;
    float spawnInterval = 2.0f;
    float interWaveTimer = 0.0f;
    int wave = 1;
    int zombiesSpawnedThisWave = 0;
    int waveTarget = 5;

    // FPS tracking
    int frameCount = 0;
    float fpsTimer = 0.0f;
    int fps = 0;

    Player* player = nullptr;
    Weapon* weapon = nullptr;
    World* world = nullptr;
    ZombieManager* zombies = nullptr;
};
