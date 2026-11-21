#include "weapon.h"
#include <cmath>
#include "raymath.h"

Weapon::Weapon() {}

bool Weapon::Init() {
    Shutdown();
    cubeModel = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
    cylModel = LoadModelFromMesh(GenMeshCylinder(0.5f, 1.0f, 8));
    ready = (cubeModel.meshCount > 0) && (cylModel.meshCount > 0);
    return ready;
}

void Weapon::Shutdown() {
    if (cubeModel.meshes) {
        UnloadModel(cubeModel);
        cubeModel = {};
    }
    if (cylModel.meshes) {
        UnloadModel(cylModel);
        cylModel = {};
    }
    ready = false;
}

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

    // Weapon sway (breathing / idle drift)
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
    recoil += 0.32f;          // pitch kick (camera + visual)
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
    swayTime = 0.0f;
}

void Weapon::DrawBox(const Vector3& pos, const Vector3& size, Color color) const {
    DrawModelEx(cubeModel, pos, { 0.0f, 1.0f, 0.0f }, 0.0f, size, color);
}

void Weapon::DrawCyl(const Vector3& start, const Vector3& end, float radius, Color color) const {
    Vector3 dir = Vector3Normalize(Vector3Subtract(end, start));
    float len = Vector3Length(Vector3Subtract(end, start));
    if (len <= 0.0f) return;
    Vector3 mid = Vector3Add(start, Vector3Scale(dir, len * 0.5f));

    const Vector3 localY{ 0.0f, 1.0f, 0.0f };
    float dot = Vector3DotProduct(localY, dir);
    if (dot > 0.9999f) {
        DrawModelEx(cylModel, mid, { 0.0f, 1.0f, 0.0f }, 0.0f, { radius, len, radius }, color);
    } else if (dot < -0.9999f) {
        // Cylinder pointing straight down: rotate 180 deg around X
        DrawModelEx(cylModel, mid, { 1.0f, 0.0f, 0.0f }, 180.0f, { radius, len, radius }, color);
    } else {
        Vector3 axis = Vector3Normalize(Vector3CrossProduct(localY, dir));
        float angle = acosf(dot) * RAD2DEG;
        DrawModelEx(cylModel, mid, axis, angle, { radius, len, radius }, color);
    }
}

void Weapon::Draw3D(const Camera3D& camera) {
    if (!ready) return;

    Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, { 0.0f, 1.0f, 0.0f }));
    Vector3 up{ 0.0f, 1.0f, 0.0f };

    // --- Idle sway (breathing / micro-drift) ---
    float swayX = sinf(swayTime * 2.5f) * 0.015f;
    float swayY = cosf(swayTime * 3.0f) * 0.010f;
    Vector3 sway = Vector3Add(Vector3Scale(right, swayX), Vector3Scale(up, swayY));

    // --- Recoil visual offset: gun kicks back (-forward) and down on screen (-up) ---
    float kick = recoil;
    Vector3 recoilOffset = Vector3Add(
        Vector3Scale(forward, -kick * 0.20f),
        Vector3Scale(up, -kick * 0.45f));

    // --- Reload offset: bob the weapon down and back up during reload ---
    float reloadOffset = 0.0f;
    if (reloading) {
        float prog = 1.0f - reloadTimer / reloadTime;
        if (prog < 0.0f) prog = 0.0f;
        if (prog > 1.0f) prog = 1.0f;
        reloadOffset = sinf(prog * 3.1415926f) * 0.14f;
    }

    // Base viewmodel position in front of the camera.
    Vector3 base = Vector3Add(camera.position, Vector3Scale(forward, 0.50f));
    base = Vector3Add(base, Vector3Scale(right, 0.32f));
    base = Vector3Add(base, Vector3Scale(up, -0.22f));
    base = Vector3Add(base, recoilOffset);
    base = Vector3Add(base, sway);
    base = Vector3Add(base, Vector3Scale(up, reloadOffset));

    // Camera-space offset helper: offset.x = right, offset.y = up, offset.z = forward.
    auto offset = [&](float x, float y, float z) {
        return Vector3Add(base,
            Vector3Add(Vector3Scale(right, x), Vector3Add(Vector3Scale(up, y), Vector3Scale(forward, z))));
    };

    Color body{ 60, 65, 70, 255 };
    Color slide{ 100, 105, 110, 255 };
    Color barrel{ 90, 95, 100, 255 };
    Color grip{ 40, 45, 50, 255 };
    Color arm{ 150, 115, 95, 255 };

    // Gun body
    DrawBox( offset(0.0f, 0.0f, 0.10f), { 0.14f, 0.09f, 0.30f }, body);
    // Slide (top rear)
    DrawBox( offset(0.0f, 0.055f, 0.14f), { 0.13f, 0.04f, 0.20f }, slide);
    // Barrel (extends forward from the body)
    DrawCyl( offset(0.0f, 0.0f, 0.30f), offset(0.0f, 0.0f, 0.58f), 0.032f, barrel);
    // Grip (below the body)
    DrawBox( offset(0.0f, -0.10f, -0.04f), { 0.10f, 0.14f, 0.10f }, grip);

    // Arms: shoulders (lower corners) -> hands (near the grip)
    Vector3 shoulderL = Vector3Add(camera.position, Vector3Scale(forward, 0.25f));
    shoulderL = Vector3Add(shoulderL, Vector3Scale(right, -0.42f));
    shoulderL = Vector3Add(shoulderL, Vector3Scale(up, -0.34f));
    Vector3 shoulderR = Vector3Add(camera.position, Vector3Scale(forward, 0.25f));
    shoulderR = Vector3Add(shoulderR, Vector3Scale(right, 0.42f));
    shoulderR = Vector3Add(shoulderR, Vector3Scale(up, -0.34f));
    Vector3 handL = offset(-0.09f, -0.14f, 0.02f);
    Vector3 handR = offset(0.09f, -0.14f, 0.02f);
    DrawCyl( shoulderL, handL, 0.024f, arm);
    DrawCyl( shoulderR, handR, 0.024f, arm);

    // Muzzle flash at the barrel tip
    if (muzzleFlashTimer > 0.0f) {
        float flash = muzzleFlashTimer / 0.08f;
        if (flash > 1.0f) flash = 1.0f;
        Vector3 muzzle = offset(0.0f, 0.0f, 0.60f);
        Color flashColor{ 255, 230, 120, (unsigned char)(220.0f * flash) };
        DrawBox( muzzle, { 0.05f, 0.05f, 0.06f }, flashColor);
        Color coreColor{ 255, 255, 200, (unsigned char)(180.0f * flash) };
        DrawBox( muzzle, { 0.02f, 0.02f, 0.03f }, coreColor);
    }
}
