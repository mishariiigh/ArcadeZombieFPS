#pragma once

#include "raylib.h"
#include <vector>

class Player;

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
    Color bodyColor{ 120, 160, 90, 255 };   // low-poly zombie green
    Color headColor{ 100, 130, 70, 255 };
};

class ZombieManager {
public:
    ZombieManager() = default;
    ~ZombieManager() = default;

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

    // Exposed for unit testing: separates overlapping zombies.
    void SeparateZombies(float dt);

private:
    std::vector<Zombie> zombies;
    float detectRange = 18.0f;

    void UpdateZombie(Zombie& z, float dt, const Vector3& playerPos, float playerRadius);
    void DrawZombie(const Zombie& z) const;
};
