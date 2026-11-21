#include "weapon.h"
#include "raymath.h"

Weapon::Weapon() {}

void Weapon::Update(float dt, const Camera3D& camera) {
    if (shootCooldown > 0.0f) {
        shootCooldown -= dt;
        if (shootCooldown < 0.0f) shootCooldown = 0.0f;
    }
    if (muzzleFlashTimer > 0.0f) {
        muzzleFlashTimer -= dt;
        if (muzzleFlashTimer < 0.0f) muzzleFlashTimer = 0.0f;
    }

    // Recoil decay
    if (recoil > 0.0f) {
        recoil -= recoilDecay * dt;
        if (recoil < 0.0f) recoil = 0.0f;
    }

    // Weapon sway (breathing / idle drift), slightly camera-pitch relative
    swayTime += dt;

    // Reload
    if (reloading) {
        reloadTimer -= dt;
        if (reloadTimer <= 0.0f) {
            reloading = false;
            int needed = clipSize - ammoInClip;
            int take = (needed < ammoReserve) ? needed : ammoReserve;
            ammoInClip += take;
            ammoReserve -= take;
        }
    }
}

ShotResult Weapon::Shoot(const Camera3D& camera) {
    if (!CanShoot()) return { { camera.position, {0, 0, 0} }, false };

    ammoInClip--;
    shootCooldown = fireRate;
    recoil += 0.32f;          // pitch kick
    muzzleFlashTimer = 0.08f;

    Vector3 direction = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    return { Ray{ camera.position, direction }, true };
}

void Weapon::Reload() {
    if (reloading) return;
    if (ammoInClip >= clipSize) return;
    if (ammoReserve <= 0) return;
    reloading = true;
    reloadTimer = reloadTime;
}

void Weapon::Reset() {
    ammoInClip = clipSize;
    ammoReserve = 36;
    reloading = false;
    reloadTimer = 0.0f;
    shootCooldown = 0.0f;
    recoil = 0.0f;
    muzzleFlashTimer = 0.0f;
}

void Weapon::Draw(const Camera3D& camera, int screenW, int screenH) const {
    int cx = screenW / 2;
    int baseY = screenH - 60;

    // Camera-relative sway: gun drifts opposite to where the camera is aiming
    Vector3 camFwd = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    float pitchSway = sinf(swayTime * 3.0f) * 3.0f - camFwd.y * 8.0f;
    float yawSway = cosf(swayTime * 2.0f) * 2.0f;

    // Recoil kick pushes the gun down on screen (camera kicks up)
    float kick = recoil * 55.0f;

    int gunX = cx + (int)yawSway - 72;
    int gunY = baseY + (int)pitchSway + (int)kick - 36;

    // Darker shade when recoiling slightly
    Color body = Color{ 55, 60, 65, 255 };
    Color trim = Color{ 110, 115, 120, 255 };

    // Gun body (main)
    DrawRectangle(gunX, gunY, 144, 64, body);
    DrawRectangleLines(gunX, gunY, 144, 64, trim);

    // Slide (top rear section)
    DrawRectangle(gunX + 10, gunY - 10, 80, 14, trim);
    DrawRectangleLines(gunX + 10, gunY - 10, 80, 14, Color{ 150, 155, 160, 255 });

    // Barrel (extends toward screen center)
    DrawRectangle(cx - 14, gunY - 32, 28, 32, Color{ 100, 105, 110, 255 });
    DrawRectangle(cx - 8, gunY - 40, 16, 8, Color{ 160, 165, 170, 255 }); // muzzle tip

    // Front sight
    DrawRectangle(cx - 2, gunY - 44, 4, 6, WHITE);

    // Grip (angled down-left)
    DrawRectangle(gunX + 40, gunY + 36, 48, 28, Color{ 35, 40, 44, 255 });
    DrawRectangleLines(gunX + 40, gunY + 36, 48, 28, trim);

    // Magazine line on grip
    DrawRectangle(gunX + 52, gunY + 44, 24, 4, Color{ 80, 85, 90, 255 });

    // Muzzle flash (brighter, larger)
    if (muzzleFlashTimer > 0.0f) {
        float flash = muzzleFlashTimer / 0.08f;
        float alpha = 235.0f * flash;
        Color flashColor{ 255, 230, 120, (unsigned char)alpha };
        DrawRectangle(cx - 14, gunY - 44, 28, 14, flashColor);
        DrawRectangle(cx - 6, gunY - 50, 12, 6, Color{ 255, 255, 220, (unsigned char)alpha });
        // Slight screen-center flare
        DrawCircle(cx, gunY - 30, 6.0f * flash, Color{ 255, 240, 150, (unsigned char)(alpha * 0.6f) });
    }
}
