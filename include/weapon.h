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

    void Update(float dt, const Camera3D& camera);
    void Draw(const Camera3D& camera, int screenW, int screenH) const;

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
};
