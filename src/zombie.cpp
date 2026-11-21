#include "zombie.h"
#include <algorithm>
#include "raymath.h"

int ZombieManager::Update(float dt, const Vector3& playerPos, float playerRadius) {
    int damageToPlayer = 0;

    for (auto& z : zombies) {
        UpdateZombie(z, dt, playerPos, playerRadius);

        if (z.attackCooldown > 0.0f) z.attackCooldown -= dt;
        if (z.attackCooldown < 0.0f) z.attackCooldown = 0.0f;

        if (z.alive && !z.dying) {
            float dist = Vector3Distance(z.position, playerPos);
            if (dist < z.attackRange && z.attackCooldown <= 0.0f) {
                damageToPlayer += z.attackDamage;
                z.attackCooldown = z.attackInterval;
            }
        }

        if (z.dying) {
            z.deathTimer += dt;
        }
    }

    SeparateZombies(dt);

    // Remove fully-dead zombies
    zombies.erase(std::remove_if(zombies.begin(), zombies.end(),
        [](const Zombie& z) { return z.dying && z.deathTimer > 0.6f; }),
        zombies.end());

    return damageToPlayer;
}

int ZombieManager::AliveCount() const {
    int n = 0;
    for (const auto& z : zombies) if (z.alive && !z.dying) n++;
    return n;
}

void ZombieManager::UpdateZombie(Zombie& z, float dt, const Vector3& playerPos, float /*playerRadius*/) {
    if (!z.alive || z.dying) return;

    Vector3 toPlayer = Vector3Subtract(playerPos, z.position);
    float dist = Vector3Length(toPlayer);

        if (dist > 0.05f && dist < detectRange) {
        Vector3 dir = Vector3Normalize(toPlayer);
        // Stop just before the player so zombies don't occupy the same space
        if (dist > z.attackRange + 0.35f) {
            z.position = Vector3Add(z.position, Vector3Scale(dir, z.speed * dt));
        }
    }

    // Simple bobbing vertical motion while alive
    z.position.y = 0.0f + sinf(z.position.x * 0.0f + 0.0f) * 0.0f;
}

void ZombieManager::Draw() const {
    for (const auto& z : zombies) DrawZombie(z);
}

void ZombieManager::Spawn(const Vector3& position, int health, float speed) {
    Zombie z;
    z.position = position;
    z.position.y = 0.0f;
    z.health = health;
    z.maxHealth = health;
    z.speed = speed;
    z.alive = true;
    z.dying = false;
    z.deathTimer = 0.0f;
    zombies.push_back(z);
}

void ZombieManager::Clear() {
    zombies.clear();
}

void ZombieManager::SeparateZombies(float dt) {
    const float repulsion = 6.0f; // units per second of separation push
    for (size_t i = 0; i < zombies.size(); ++i) {
        Zombie& a = zombies[i];
        if (!a.alive || a.dying) continue;
        for (size_t j = i + 1; j < zombies.size(); ++j) {
            Zombie& b = zombies[j];
            if (!b.alive || b.dying) continue;

            Vector3 diff = Vector3Subtract(a.position, b.position);
            float dist = Vector3Length(diff);
            float minDist = a.radius + b.radius;
            if (dist < minDist) {
                Vector3 dir;
                if (dist > 0.01f) {
                    dir = Vector3Normalize(diff);
                } else {
                    dir = { 1.0f, 0.0f, 0.0f }; // default push direction when exactly overlapping
                }
                float overlap = minDist - dist;
                float push = overlap * 0.5f * repulsion * dt;
                a.position = Vector3Add(a.position, Vector3Scale(dir, push));
                b.position = Vector3Subtract(b.position, Vector3Scale(dir, push));
            }
        }
    }
}

int ZombieManager::TestHit(const Ray& ray) {
    Zombie* closest = nullptr;
    float closestDist = 1e6f;

    for (auto& z : zombies) {
        if (!z.alive || z.dying) continue;

        Vector3 toCenter = Vector3Subtract(z.position, ray.position);
        float t = Vector3DotProduct(toCenter, ray.direction);
        if (t < 0.5f) continue;

        Vector3 closestPoint = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
        float dist = Vector3Distance(closestPoint, z.position);

        if (dist < z.radius + 0.7f && t < closestDist) {
            closestDist = t;
            closest = &z;
        }
    }

    if (closest) {
        closest->health -= 1;
        if (closest->health <= 0) {
            closest->alive = false;
            closest->dying = true;
            closest->deathTimer = 0.0f;
            return 10;
        }
    }
    return 0;
}

void ZombieManager::DrawZombie(const Zombie& z) const {
    if (!z.alive && !z.dying) return;

    float alpha = 255;
    if (z.dying) {
        alpha = (unsigned char)(255.0f * (1.0f - z.deathTimer / 0.6f));
        if (alpha < 0) alpha = 0;
    }
    Color body = z.bodyColor; body.a = alpha;
    Color head = z.headColor; head.a = alpha;

    // Sink into the ground while dying
    float sink = z.dying ? (z.deathTimer / 0.6f) * 1.8f : 0.0f;
    float y = z.position.y - sink;

    // Body
    DrawCubeV({ z.position.x, y + 0.85f, z.position.z }, { 0.7f, 1.5f, 0.55f }, body);
    // Head
    DrawCubeV({ z.position.x, y + 1.75f, z.position.z }, { 0.5f, 0.5f, 0.5f }, head);
    // Arms
    DrawCubeV({ z.position.x - 0.55f, y + 1.15f, z.position.z }, { 0.24f, 0.6f, 0.24f }, body);
    DrawCubeV({ z.position.x + 0.55f, y + 1.15f, z.position.z }, { 0.24f, 0.6f, 0.24f }, body);
}
