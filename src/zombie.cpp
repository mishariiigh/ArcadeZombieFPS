#include "zombie.h"
#include "world.h"
#include <algorithm>
#include <cstring>
#include <cmath>
#include "raymath.h"

namespace {

const char* ResolveModelPath(const char* preferred) {
    static const char* candidates[] = {
        "assets/models/zombie.glb",
        "../assets/models/zombie.glb",
        "../../assets/models/zombie.glb",
    };
    if (preferred && FileExists(preferred)) return preferred;
    for (const char* p : candidates) {
        if (FileExists(p)) return p;
    }
    return "assets/models/zombie.glb";
}

int FindAnimationIndex(ModelAnimation* animations, int count, const char* name) {
    for (int i = 0; i < count; ++i) {
        if (animations && std::strcmp(animations[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

} // namespace

// ---------------------------------------------------------------------------
// ZombieModel (shared, loaded once)
// ---------------------------------------------------------------------------

bool ZombieModel::Init(const char* modelPath) {
    Shutdown();
    const char* path = ResolveModelPath(modelPath);
    model = LoadModel(path);
    if (model.meshCount == 0 || !model.meshes) {
        if (model.meshes) UnloadModel(model);
        model = {};
        return false;
    }

    animCount = 0;
    animations = LoadModelAnimations(path, &animCount);

    animIdle = FindAnimationIndex(animations, animCount, "idle");
    animWalk = FindAnimationIndex(animations, animCount, "walk");
    animAttack = FindAnimationIndex(animations, animCount, "punch");
    animDeath = FindAnimationIndex(animations, animCount, "death");
    return true;
}

void ZombieModel::Shutdown() {
    if (animations) {
        UnloadModelAnimations(animations, animCount);
        animations = nullptr;
        animCount = 0;
    }
    if (model.meshes) {
        UnloadModel(model);
    }
    model = {};
    animIdle = animWalk = animAttack = animDeath = -1;
}

// ---------------------------------------------------------------------------
// ZombieManager
// ---------------------------------------------------------------------------

bool ZombieManager::Init() {
    return model.Init(nullptr);
}

void ZombieManager::Shutdown() {
    model.Shutdown();
}

void ZombieManager::InitNavigation(const World& world) {
    navigation.Build(world.Obstacles(), 75.0f, 1.0f);
}

int ZombieManager::Update(float dt, const Vector3& playerPos, float playerRadius) {
    int damageToPlayer = 0;

    for (auto& z : zombies) {
        UpdateZombie(z, dt, playerPos, playerRadius);

        if (z.attackCooldown > 0.0f) z.attackCooldown -= dt;
        if (z.attackCooldown < 0.0f) z.attackCooldown = 0.0f;
        if (z.attackAnimTimer > 0.0f) {
            z.attackAnimTimer -= dt;
            if (z.attackAnimTimer < 0.0f) z.attackAnimTimer = 0.0f;
        }

        if (z.alive && !z.dying) {
            float dist = Vector3Distance(z.position, playerPos);
            if (dist < z.attackRange && z.attackCooldown <= 0.0f) {
                damageToPlayer += z.attackDamage;
                z.attackCooldown = z.attackInterval;
                z.attackAnimTimer = 0.45f;
            }
        }

        if (z.dying) {
            z.deathTimer += dt;
        }
    }

    SeparateZombies(dt);

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

void ZombieManager::UpdateZombie(Zombie& z, float dt, const Vector3& playerPos, float playerRadius) {
    if (!z.alive || z.dying) return;

    UpdateZombieNavigation(z, dt, playerPos, playerRadius);

    // Keep the zombie on the ground plane
    z.position.y = 0.0f;

    // --- animation state driven by gameplay (only when the model is loaded) ---
    if (model.Ready()) {
        Vector3 toPlayer = Vector3Subtract(playerPos, z.position);
        float dist = Vector3Length(toPlayer);

        int target = model.animIdle;
        if (z.attackAnimTimer > 0.0f) {
            target = model.animAttack;
        } else if (z.hasValidPath && z.pathIndex < z.path.size() && dist > z.attackRange + 0.35f) {
            target = model.animWalk;
        }
        if (target < 0) target = model.animIdle;

        if (target != z.animIdx) {
            z.animIdx = target;
            z.animFrame = 0;
        }

        ModelAnimation& anim = model.animations[z.animIdx];
        int fc = anim.frameCount;
        if (fc <= 0) fc = 1;

        if (z.attackAnimTimer > 0.0f) {
            float prog = 1.0f - z.attackAnimTimer / 0.45f;
            if (prog < 0.0f) prog = 0.0f;
            if (prog > 1.0f) prog = 1.0f;
            z.animFrame = (int)(prog * (float)fc);
            if (z.animFrame >= fc) z.animFrame = fc - 1;
        } else {
            float speedFactor = (z.animIdx == model.animWalk) ? (z.speed / 2.2f) : 1.0f;
            if (speedFactor < 0.0f) speedFactor = 0.0f;
            z.animFrame = (int)(z.animFrame + dt * 30.0f * speedFactor);
            while (z.animFrame >= fc) z.animFrame -= fc;
            if (z.animFrame < 0) z.animFrame = 0;
        }
    }
}

void ZombieManager::UpdateZombieNavigation(Zombie& z, float dt, const Vector3& playerPos, float playerRadius) {
    Vector3 toPlayer = Vector3Subtract(playerPos, z.position);
    float dist = Vector3Length(toPlayer);

    // If player is out of detection range, idle
    if (dist > detectRange) {
        z.hasValidPath = false;
        z.path.clear();
        z.pathIndex = 0;
        z.pathRecalcTimer = 0.0f;
        z.stuckTimer = 0.0f;
        return;
    }

    // Face the player (for animation orientation)
    if (dist > 0.05f) {
        Vector3 dir = Vector3Normalize(toPlayer);
        z.faceAngle = atan2f(dir.x, dir.z);
    }

    // If close enough to attack, stop path following
    if (dist <= z.attackRange + 0.35f) {
        z.hasValidPath = false;
        z.path.clear();
        z.pathIndex = 0;
        z.pathRecalcTimer = 0.0f;
        z.stuckTimer = 0.0f;
        z.lastPosition = z.position;
        return;
    }

    // If navigation not ready, fall back to direct movement
    if (!navigation.Ready()) {
        if (dist > z.attackRange + 0.35f) {
            Vector3 dir = Vector3Normalize(toPlayer);
            z.position = Vector3Add(z.position, Vector3Scale(dir, z.speed * dt));
        }
        return;
    }

    // If no valid path, try direct movement as fallback (e.g., when pathfinding fails)
    if (!z.hasValidPath || z.path.empty()) {
        // Try to calculate path
        RecalculatePath(z, playerPos);
        // If still no path, move directly toward player as last resort
        if (!z.hasValidPath && dist > z.attackRange + 0.35f) {
            Vector3 dir = Vector3Normalize(toPlayer);
            Vector3 newPos = Vector3Add(z.position, Vector3Scale(dir, z.speed * dt));
            if (navigation.IsWalkable(newPos)) {
                z.position = newPos;
            }
        }
        return;
    }

    // Check if we need to recalculate path
    bool needRecalc = false;

    // First path calculation
    if (!z.hasValidPath || z.path.empty()) {
        needRecalc = true;
    }
    // Player moved significantly from last path target
    else if (Vector3Distance(playerPos, z.lastPathTarget) > 3.0f) {
        needRecalc = true;
    }
    // Periodic recalculation
    else {
        z.pathRecalcTimer += dt;
        if (z.pathRecalcTimer > 2.0f) {
            needRecalc = true;
        }
    }

    // Current waypoint reached or invalid
    if (z.hasValidPath && z.pathIndex < z.path.size()) {
        Vector3 toWaypoint = Vector3Subtract(z.path[z.pathIndex], z.position);
        toWaypoint.y = 0.0f;
        float wpDist = Vector3Length(toWaypoint);

        if (wpDist < 0.8f) {
            z.pathIndex++;
            if (z.pathIndex >= z.path.size()) {
                needRecalc = true;
            }
        }
    } else if (z.hasValidPath) {
        needRecalc = true;
    }

    if (needRecalc) {
        RecalculatePath(z, playerPos);
    }

    // Follow path if we have one
    if (z.hasValidPath && z.pathIndex < z.path.size()) {
        Vector3 targetWaypoint = z.path[z.pathIndex];
        Vector3 toWaypoint = Vector3Subtract(targetWaypoint, z.position);
        toWaypoint.y = 0.0f;
        float wpDist = Vector3Length(toWaypoint);

        if (wpDist > 0.05f) {
            Vector3 dir = Vector3Normalize(toWaypoint);
            Vector3 newPos = Vector3Add(z.position, Vector3Scale(dir, z.speed * dt));

            // Check if new position is walkable
            if (navigation.IsWalkable(newPos)) {
                z.position = newPos;
            } else {
                // Path is blocked, need recalculation
                z.hasValidPath = false;
            }
        }
    }

    // Stuck detection
    if (z.hasValidPath) {
        float moved = Vector3Distance(z.position, z.lastPosition);
        if (moved < z.speed * dt * 0.3f) { // Less than 30% of expected movement
            z.stuckTimer += dt;
        } else {
            z.stuckTimer = 0.0f;
        }
        z.lastPosition = z.position;

        if (z.stuckTimer > 1.5f) {
            HandleStuck(z, playerPos);
        }
    }
}

void ZombieManager::RecalculatePath(Zombie& z, const Vector3& playerPos) {
    Vector3 start = z.position;
    start.y = 0.0f;
    Vector3 goal = playerPos;
    goal.y = 0.0f;

    z.path = navigation.FindPath(start, goal);
    if (!z.path.empty()) {
        z.pathIndex = 0;
        z.hasValidPath = true;
        z.lastPathTarget = playerPos;
        z.pathRecalcTimer = 0.0f;
        z.stuckTimer = 0.0f;
        z.lastPosition = z.position;
    } else {
        // No path found - try to find a walkable position near the player
        Vector3 nearest = navigation.GetNearestWalkable(goal);
        if (Vector3Distance(nearest, goal) < 5.0f) {
            z.path = navigation.FindPath(start, nearest);
            if (!z.path.empty()) {
                z.pathIndex = 0;
                z.hasValidPath = true;
                z.lastPathTarget = playerPos;
                z.pathRecalcTimer = 0.0f;
                z.stuckTimer = 0.0f;
                z.lastPosition = z.position;
            }
        }

        if (z.path.empty()) {
            z.hasValidPath = false;
            z.pathIndex = 0;
        }
    }
}

bool ZombieManager::IsStuck(const Zombie& z, float dt) const {
    float moved = Vector3Distance(z.position, z.lastPosition);
    return moved < z.speed * dt * 0.3f && z.stuckTimer > 1.5f;
}

void ZombieManager::HandleStuck(Zombie& z, const Vector3& playerPos) {
    z.stuckTimer = 0.0f;

    // Try to recalculate path
    RecalculatePath(z, playerPos);

    // If still no path, try to find any walkable direction
    if (!z.hasValidPath) {
        // Try a few random directions around the zombie
        for (int attempt = 0; attempt < 8; ++attempt) {
            float angle = (float)attempt * (3.14159f / 4.0f);
            Vector3 testDir{ std::sin(angle), 0.0f, std::cos(angle) };
            Vector3 testPos = Vector3Add(z.position, Vector3Scale(testDir, 3.0f));
            if (navigation.IsWalkable(testPos)) {
                z.path = navigation.FindPath(z.position, testPos);
                if (!z.path.empty()) {
                    z.pathIndex = 0;
                    z.hasValidPath = true;
                    z.pathRecalcTimer = 0.0f;
                    z.lastPosition = z.position;
                    return;
                }
            }
        }
    }
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
    z.attackCooldown = 0.0f;
    z.attackAnimTimer = 0.0f;
    z.animIdx = -1;
    z.animFrame = 0;
    z.faceAngle = 0.0f;
    z.path.clear();
    z.pathIndex = 0;
    z.pathRecalcTimer = 0.0f;
    z.lastPathTarget = { 0, 0, 0 };
    z.stuckTimer = 0.0f;
    z.lastPosition = position;
    z.hasValidPath = false;
    zombies.push_back(z);
}

void ZombieManager::Clear() {
    zombies.clear();
}

void ZombieManager::SeparateZombies(float dt) {
    const float repulsion = 6.0f;
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
                    dir = { 1.0f, 0.0f, 0.0f };
                }
                float overlap = minDist - dist;
                float push = overlap * 0.5f * repulsion * dt;
                Vector3 pushA = Vector3Scale(dir, push);
                Vector3 pushB = Vector3Scale(dir, -push);

                // Only apply separation if the resulting positions are walkable
                Vector3 newPosA = Vector3Add(a.position, pushA);
                Vector3 newPosB = Vector3Add(b.position, pushB);
                if (navigation.IsWalkable(newPosA)) a.position = newPosA;
                if (navigation.IsWalkable(newPosB)) b.position = newPosB;
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
            closest->animIdx = model.animDeath;
            closest->animFrame = 0;
            return 10;
        }
    }
    return 0;
}

void ZombieManager::DrawZombie(const Zombie& z) const {
    if (!z.alive && !z.dying) return;
    if (!model.Ready()) return;

    float alpha = 255;
    float sink = 0.0f;
    int ai = z.animIdx;
    int frame = z.animFrame;

    if (z.dying && model.animDeath >= 0) {
        alpha = (unsigned char)(255.0f * (1.0f - z.deathTimer / 0.6f));
        if (alpha < 0) alpha = 0;
        sink = (z.deathTimer / 0.6f) * 1.8f;
        ai = model.animDeath;
        int fc = model.animations[ai].frameCount;
        if (fc <= 0) fc = 1;
        float prog = z.deathTimer / 0.6f;
        if (prog > 1.0f) prog = 1.0f;
        frame = (int)(prog * (float)fc);
        if (frame >= fc) frame = fc - 1;
    }

    if (ai < 0) ai = model.animIdle;
    if (ai < 0 || !model.animations) return;

    Color tint = WHITE;
    tint.a = alpha;

    Vector3 pos{ z.position.x, z.position.y - sink, z.position.z };

    const float scale = 0.85f;

    UpdateModelAnimation(model.model, model.animations[ai], frame);
    DrawModelEx(model.model, pos, { 0.0f, 1.0f, 0.0f },
                z.faceAngle * RAD2DEG, { scale, scale, scale }, tint);
}