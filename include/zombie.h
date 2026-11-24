#pragma once

#include "raylib.h"
#include "navigation.h"
#include <vector>

class Player;
class World;

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
    int animHit = -1;      // "wave" clip reused as a brief pain reaction

    ZombieModel() = default;
    ~ZombieModel() = default;

    // Loads the model and animations from a project-relative path.
    // Must be called after an OpenGL context exists.
    bool Init(const char* modelPath);
    // Unloads the model and animations. Safe to call on a default/unloaded model.
    void Shutdown();
    bool Ready() const { return animCount > 0 && model.meshCount > 0; };
};

struct Zombie {
    // Animation states, in decreasing priority: Death > Attack > Hit > Walk > Idle.
    enum class AnimState { Idle, Walk, Attack, Hit, Death };

    // Attack timing (shared so tests can be deterministic).
    static constexpr float AttackDuration = 0.60f;  // total punch animation time
    static constexpr float AttackImpactTime = 0.30f; // time after attack start when damage lands
    static constexpr float DeathDuration = 2.00f;   // full death clip before removal
    static constexpr float HitReactionTime = 0.18f; // brief stagger on a non-lethal hit
    // Zombies inside this radius around the player spread out instead of stacking.
    static constexpr float EngageRange = 2.50f;

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
    bool attackHitApplied = false;  // damage for the current swing already dealt
    float hitTimer = 0.0f;          // >0 while the hit reaction plays
    int animIdx = -1;               // current animation index
    int animFrame = 0;              // current frame within the animation
    AnimState animState = AnimState::Idle;
    float faceAngle = 0.0f;         // yaw towards the player (radians)
    float approachAngle = 0.0f;     // preferred direction to circle around the player
    Color bodyColor{ 120, 160, 90, 255 };   // low-poly zombie green (kept for reference)
    Color headColor{ 100, 130, 70, 255 };

    // Navigation state
    std::vector<Vector3> path;          // current path waypoints (world coords, y=0)
    size_t pathIndex = 0;               // current waypoint index in path
    float pathRecalcTimer = 0.0f;       // time since last path recalculation
    Vector3 lastPathTarget{ 0, 0, 0 };  // player position when path was calculated
    float stuckTimer = 0.0f;            // time spent making insufficient progress
    Vector3 lastPosition{ 0, 0, 0 };    // position at last stuck check
    bool hasValidPath = false;          // whether current path is valid
};

class ZombieManager {
public:
    ZombieManager() = default;
    ~ZombieManager() = default;

    // Loads the shared zombie model/animations. Call after window creation.
    bool Init();
    // Unloads the shared model/animations. Call before window destruction.
    void Shutdown();

    // Initialize navigation grid from world obstacles. Call after World::Init().
    void InitNavigation(const World& world);

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
    const Navigation& GetNavigation() const { return navigation; }

    // Exposed for unit testing: separates overlapping zombies.
    void SeparateZombies(float dt);

private:
    std::vector<Zombie> zombies;
    ZombieModel model;
    Navigation navigation;
    float detectRange = 18.0f;

    void UpdateZombie(Zombie& z, float dt, const Vector3& playerPos, float playerRadius);
    void UpdateZombieNavigation(Zombie& z, float dt, const Vector3& playerPos, float playerRadius);
    void UpdateAnimationState(Zombie& z, float dt, const Vector3& playerPos);
    void RecalculatePath(Zombie& z, const Vector3& playerPos);
    bool IsStuck(const Zombie& z, float dt) const;
    void HandleStuck(Zombie& z, const Vector3& playerPos);
    void DrawZombie(const Zombie& z) const;
    int ResolveAnimationIndex(const char* name);
};
