#pragma once

#include "raylib.h"
#include <vector>

class Player;

// Shared, loaded-once 3D zombie model (glTF 2.0) with skeletal animations.
// One model + animation set is reused by every zombie instance.
struct ZombieModel {
    Model model{};
    ModelAnimation* animations = nullptr;
    int animCount = 0;
    int animIdle = -1;
    int animWalk = -1;
    int animAttack = -1;
    int animDeath = -1;

    ZombieModel() = default;
    ~ZombieModel() = default;

    // Loads the model and animations from a project-relative path.
    // Must be called after an OpenGL context exists.
    bool Init(const char* modelPath);
    // Unloads the model and animations. Safe to call on a default/unloaded model.
    void Shutdown();
    bool Ready() const { return animCount > 0 && model.meshCount > 0; }
};

struct Zombie {
    Vector3 position{ 0, 0, 0 };
    float radius = 0.45f;
    float height = 1.8f;
    float speed = 2.2f;
    int health = 3;
    int maxHealth = 3;
    bool alive = true;
    bool dying = false;
    float deathTimer = 0.0f;
    float attackCooldown = 0.0f;
    float attackRange = 1.1f;
    float attackDamage = 12;
    float attackInterval = 1.0f;
    // Animation state
    float attackAnimTimer = 0.0f;   // >0 while the attack animation plays
    int animIdx = -1;               // current animation index
    int animFrame = 0;              // current frame within the animation
    float faceAngle = 0.0f;         // yaw towards the player (radians)
    Color bodyColor{ 120, 160, 90, 255 };   // low-poly zombie green (kept for reference)
    Color headColor{ 100, 130, 70, 255 };
};

class ZombieManager {
public:
    ZombieManager() = default;
    ~ZombieManager() = default;

    // Loads the shared zombie model/animations. Call after window creation.
    bool Init();
    // Unloads the shared model/animations. Call before window destruction.
    void Shutdown();

    int Update(float dt, const Vector3& playerPos, float playerRadius);
    void Draw() const;
    void Spawn(const Vector3& position, int health = 3, float speed = 2.2f);
    void Clear();

    // Raycast hit test against all zombies. Applies damage; returns score points
    // awarded (10 when a zombie is killed, 0 otherwise).
    int TestHit(const Ray& ray);

    const std::vector<Zombie>& Zombies() const { return zombies; }
    int AliveCount() const;
    int TotalCount() const { return (int)zombies.size(); }
    bool ModelReady() const { return model.Ready(); }

    // Exposed for unit testing: separates overlapping zombies.
    void SeparateZombies(float dt);

private:
    std::vector<Zombie> zombies;
    ZombieModel model;
    float detectRange = 18.0f;

    void UpdateZombie(Zombie& z, float dt, const Vector3& playerPos, float playerRadius);
    void DrawZombie(const Zombie& z) const;
    int ResolveAnimationIndex(const char* name);
};
