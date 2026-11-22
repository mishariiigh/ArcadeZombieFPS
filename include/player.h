#pragma once

#include "raylib.h"

class Player {
public:
    Player();
    ~Player() = default;

    void Reset(Camera3D& camera);
    void Update(float dt, Camera3D& camera, const class World& world);
    void DrawDebug() const;

    void TakeDamage(int amount);
    void AddPitch(float radians);
    void ApplyRecoil(float radians);
    void ResetMouseLook();
    bool IsAlive() const { return health > 0; }
    int Health() const { return health; }

    const Vector3& Position() const { return position; }
    float Radius() const { return radius; }
    float Height() const { return height; }
    Vector3 Forward() const;

    // --- Test-only seam (used by tests/test_gameplay.cpp) -------------------
    // Lets regression tests drive ApplyPhysics with a known velocity instead
    // of polling real keyboard state. Not used by game code.
    void SetVelocityForTest(const Vector3& v) { velocity = v; }
    void SetPositionForTest(const Vector3& p) { position = p; }
    void TestHook_ApplyPhysics(float dt, const class World& world) { ApplyPhysics(dt, world); }

private:
    void HandleInput(float dt, Camera3D& camera);
    void ApplyPhysics(float dt, const class World& world);
    void UpdateCameraVectors(Camera3D& camera);

    Vector3 position{ 0, 0, 0 };
    Vector3 velocity{ 0, 0, 0 };
    float yaw = 0.0f;     // radians, around Y
    float pitch = 0.0f;   // radians, up/down
    float recoilPitch = 0.0f;

    float radius = 0.35f;
    float height = 1.7f;
    float walkSpeed = 4.5f;
    float sprintSpeed = 8.0f;
    float jumpVelocity = 6.5f;
    float gravity = 24.0f;
    float mouseSensitivity = 0.003f;
    float recoilDecay = 8.0f * 0.6f;

    Vector2 previousMousePosition{};
    bool mouseNeedsReset = true;

    int health = 100;
    int maxHealth = 100;
    bool onGround = false;
};
