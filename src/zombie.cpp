#include "zombie.h"
#include <algorithm>
#include <cstring>
#include "raymath.h"

namespace {
// Project-relative locations the shared zombie model may be loaded from.
// The game is launched from build/ (per README), so ../assets/... is the
// primary path; assets/models/ is tried for runs from the project root.
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
                z.attackAnimTimer = 0.45f; // play the attack animation
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
        z.faceAngle = atan2f(dir.x, dir.z);
        // Stop just before the player so zombies don't occupy the same space
        if (dist > z.attackRange + 0.35f) {
            z.position = Vector3Add(z.position, Vector3Scale(dir, z.speed * dt));
        }
    }

    // Keep the zombie on the ground plane
    z.position.y = 0.0f;

    // --- animation state driven by gameplay (only when the model is loaded) ---
    if (model.Ready()) {
        int target = model.animIdle;
        if (z.attackAnimTimer > 0.0f) {
            target = model.animAttack;
        } else if (dist > 0.05f && dist < detectRange && dist > z.attackRange + 0.35f) {
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
            // Play the punch animation once over the attack window
            float prog = 1.0f - z.attackAnimTimer / 0.45f;
            if (prog < 0.0f) prog = 0.0f;
            if (prog > 1.0f) prog = 1.0f;
            z.animFrame = (int)(prog * (float)fc);
            if (z.animFrame >= fc) z.animFrame = fc - 1;
        } else {
            // Looping idle / walk, advanced at ~30 fps (walk scaled by speed)
            float speedFactor = (z.animIdx == model.animWalk) ? (z.speed / 2.2f) : 1.0f;
            if (speedFactor < 0.0f) speedFactor = 0.0f;
            z.animFrame = (int)(z.animFrame + dt * 30.0f * speedFactor);
            while (z.animFrame >= fc) z.animFrame -= fc;
            if (z.animFrame < 0) z.animFrame = 0;
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

    // Sink into the ground while dying
    Vector3 pos{ z.position.x, z.position.y - sink, z.position.z };

    // Scale chosen so the model's ~1.05m width fits inside the 0.9m diameter
    // collision bounds (radius 0.45), giving a height of ~1.76m vs 1.8m gameplay height.
    const float scale = 0.85f;

    // Advance the shared skeleton to this zombie's pose, then draw it.
    // Bone pose is transient per-instance render state.
    UpdateModelAnimation(model.model, model.animations[ai], frame);
    DrawModelEx(model.model, pos, { 0.0f, 1.0f, 0.0f },
                z.faceAngle * RAD2DEG, { scale, scale, scale }, tint);
}
