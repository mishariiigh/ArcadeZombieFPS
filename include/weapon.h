#pragma once

#include "raylib.h"

struct ShotResult {
    Ray ray{};
    bool fired = false;
};

class Weapon {
public:
    Weapon();
    ~Weapon() = default;

    // Loads/generates the 3D viewmodel meshes. Must be called after an OpenGL
    // context exists (i.e. after InitWindow).
    bool Init();
    // Unloads the generated meshes. Safe to call on an unloaded weapon.
    void Shutdown();
    bool Ready() const { return ready; }

    void Update(float dt, const Camera3D& camera);
    // Draws the 3D first-person viewmodel (gun + arms) inside BeginMode3D/EndMode3D,
    // positioned relative to the camera.
    void Draw3D(const Camera3D& camera);

    bool CanShoot() const { return !reloading && shootCooldown <= 0.0f && ammoInClip > 0; }
    ShotResult Shoot(const Camera3D& camera);
    void Reload();

    int AmmoInClip() const { return ammoInClip; }
    int AmmoReserve() const { return ammoReserve; }
    int ClipSize() const { return clipSize; }
    bool IsReloading() const { return reloading; }

    // Recoil pitch kick (radians) to apply to the camera for view feedback.
    float ViewKick() const { return recoil * 0.6f; }

    void Reset();

private:
    bool ready = false;
    Model cubeModel{};   // unit cube, used for gun body/slide/grip
    Model cylModel{};    // unit cylinder (local Y axis), used for barrel/arms

    int ammoInClip = 12;
    int ammoReserve = 36;
    const int clipSize = 12;

    float shootCooldown = 0.0f;
    const float fireRate = 0.13f;      // seconds between shots
    const float reloadTime = 1.4f;     // seconds
    float reloadTimer = 0.0f;
    bool reloading = false;

    float recoil = 0.0f;       // current recoil kick
    const float recoilDecay = 8.0f;
    float muzzleFlashTimer = 0.0f;

    float swayTime = 0.0f;

    // Draw a unit cube scaled/positioned/colored at a world position.
    void DrawBox(const Vector3& pos, const Vector3& size, Color color) const;
    // Draw a cylinder from start to end (world space), radius and color.
    void DrawCyl(const Vector3& start, const Vector3& end, float radius, Color color) const;
};
