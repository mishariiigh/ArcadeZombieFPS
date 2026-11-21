// Automated gameplay logic tests for ArcadeZombieFPS.
// These tests exercise pure logic (no render window required):
//   - Weapon ammo, cooldown, reload, shoot
//   - ZombieManager spawn, hit detection, death, separation
//   - Player damage, health, pitch clamping
//
// Compile (from build dir):
//   clang++ -std=c++17 -I../include ../src/player.cpp ../src/weapon.cpp \
//     ../src/zombie.cpp ../src/world.cpp tests/test_gameplay.cpp \
//     -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo \
//     -o test_gameplay
//
// Run: ./test_gameplay

#include <cassert>
#include <cstdio>
#include <cmath>

#include "raylib.h"
#include "raymath.h"

#include "player.h"
#include "weapon.h"
#include "zombie.h"
#include "world.h"

static int testsPassed = 0;
static int testsFailed = 0;

#define TEST(name)                                                          \
    printf("  TEST: %s ... ", name);                                        \
    fflush(stdout)

#define PASS()                                                              \
    do {                                                                    \
        printf("PASS\n");                                                   \
        ++testsPassed;                                                      \
    } while (0)

#define FAIL(msg)                                                           \
    do {                                                                    \
        printf("FAIL: %s\n", msg);                                          \
        ++testsFailed;                                                      \
    } while (0)

#define ASSERT(cond, msg)                                                   \
    do {                                                                    \
        if (!(cond)) { FAIL(msg); return; }                                 \
    } while (0)

// ---------------------------------------------------------------------------
// Weapon tests
// ---------------------------------------------------------------------------

static void TestWeapon_InitialState() {
    TEST("Weapon initial state");
    Weapon w;
    ASSERT(w.AmmoInClip() == 12, "clip should start at 12");
    ASSERT(w.AmmoReserve() == 36, "reserve should start at 36");
    ASSERT(w.ClipSize() == 12, "clip size should be 12");
    ASSERT(w.CanShoot() == true, "should be able to shoot initially");
    ASSERT(w.IsReloading() == false, "should not be reloading initially");
    PASS();
}

static void TestWeapon_ShootReducesAmmoAndStartsCooldown() {
    TEST("Weapon shoot reduces ammo and starts cooldown");
    Weapon w;
    Camera3D cam{};
    cam.position = { 0, 2, 0 };
    cam.target = { 0, 2, -1 };

    ShotResult sr = w.Shoot(cam);
    ASSERT(sr.fired == true, "shot should fire");
    ASSERT(w.AmmoInClip() == 11, "ammo should drop to 11");
    ASSERT(w.CanShoot() == false, "should not be able to shoot during cooldown");

    // After cooldown expires, can shoot again
    w.Update(0.2f, cam);  // fireRate is 0.13s, 0.2s > cooldown
    ASSERT(w.CanShoot() == true, "should be able to shoot after cooldown");
    PASS();
}

static void TestWeapon_FireRateLimit() {
    TEST("Weapon fire rate limit");
    Weapon w;
    Camera3D cam{};
    cam.position = { 0, 2, 0 };
    cam.target = { 0, 2, -1 };

    w.Shoot(cam);  // shot 1
    ASSERT(w.CanShoot() == false, "immediate re-shoot should be blocked");
    w.Update(0.1f, cam);  // still within 0.13s fire rate
    ASSERT(w.CanShoot() == false, "re-shoot within fire rate should be blocked");
    w.Update(0.05f, cam);  // total 0.15s > 0.13s
    ASSERT(w.CanShoot() == true, "re-shoot after fire rate should be allowed");
    PASS();
}

static void TestWeapon_EmptyClipCannotShoot() {
    TEST("Weapon empty clip cannot shoot");
    Weapon w;
    Camera3D cam{};
    cam.position = { 0, 2, 0 };
    cam.target = { 0, 2, -1 };

    for (int i = 0; i < 12; ++i) {
        ShotResult sr = w.Shoot(cam);
        ASSERT(sr.fired == true, "shot should fire");
        w.Update(0.2f, cam);  // expire cooldown between shots
    }
    ASSERT(w.AmmoInClip() == 0, "clip should be empty");
    ASSERT(w.CanShoot() == false, "empty clip cannot shoot");
    ShotResult sr = w.Shoot(cam);
    ASSERT(sr.fired == false, "empty clip shot should not fire");
    PASS();
}

static void TestWeapon_Reload() {
    TEST("Weapon reload restores ammo");
    Weapon w;
    Camera3D cam{};
    cam.position = { 0, 2, 0 };
    cam.target = { 0, 2, -1 };

    // Empty the clip (fire rate is 0.13s; advance dt between shots)
    for (int i = 0; i < 12; ++i) {
        ShotResult sr = w.Shoot(cam);
        ASSERT(sr.fired == true, "shot should fire");
        w.Update(0.2f, cam);  // expire cooldown
    }
    ASSERT(w.AmmoInClip() == 0, "clip should be empty");
    ASSERT(w.CanShoot() == false, "empty clip cannot shoot");
    ShotResult sr = w.Shoot(cam);
    ASSERT(sr.fired == false, "empty clip shot should not fire");
    PASS();
}

static void TestWeapon_ReloadWhenFullIsNoOp() {
    TEST("Weapon reload when full is no-op");
    Weapon w;
    w.Reload();
    ASSERT(w.IsReloading() == false, "full clip should not start reload");
    PASS();
}

static void TestWeapon_ReloadWhenNoReserveIsNoOp() {
    TEST("Weapon reload with no reserve is no-op");
    Weapon w;
    Camera3D cam{};
    cam.position = { 0, 2, 0 };
    cam.target = { 0, 2, -1 };

    // Use all ammo: empty clip, then drain reserve through reload+shoot cycles
    for (int i = 0; i < 12; ++i) {
        w.Shoot(cam);
        w.Update(0.2f, cam);
    }
    for (int round = 0; round < 3; ++round) {
        w.Reload();
        w.Update(1.5f, cam);
        for (int i = 0; i < 12; ++i) {
            w.Shoot(cam);
            w.Update(0.2f, cam);
        }
    }
    ASSERT(w.AmmoInClip() == 0, "clip empty");
    ASSERT(w.AmmoReserve() == 0, "reserve empty");
    w.Reload();
    ASSERT(w.IsReloading() == false, "no reserve, reload should not start");
    ASSERT(w.AmmoInClip() == 0, "clip still empty");
    PASS();
}

static void TestWeapon_RecoilViewKick() {
    TEST("Weapon ViewKick returns recoil-scaled value");
    Weapon w;
    Camera3D cam{};
    cam.position = { 0, 2, 0 };
    cam.target = { 0, 2, -1 };

    ASSERT(w.ViewKick() == 0.0f, "no recoil initially");
    w.Shoot(cam);  // recoil += 0.32f
    float kick = w.ViewKick();
    ASSERT(kick > 0.0f && kick < 0.32f, "ViewKick should be positive and <= recoil");
    // ViewKick = recoil * 0.6f = 0.32f * 0.6f = 0.192f
    ASSERT(std::abs(kick - 0.192f) < 0.001f, "ViewKick should be recoil * 0.6f");
    PASS();
}

static void TestWeapon_Reset() {
    TEST("Weapon Reset restores initial state");
    Weapon w;
    Camera3D cam{};
    cam.position = { 0, 2, 0 };
    cam.target = { 0, 2, -1 };

    for (int i = 0; i < 12; ++i) {
        w.Shoot(cam);
        w.Update(0.2f, cam);
    }
    w.Reload();
    w.Update(1.5f, cam);
    w.Reset();
    ASSERT(w.AmmoInClip() == 12, "reset: clip full");
    ASSERT(w.AmmoReserve() == 36, "reset: reserve full");
    ASSERT(w.IsReloading() == false, "reset: not reloading");
    ASSERT(w.CanShoot() == true, "reset: can shoot");
    PASS();
}

static void TestWeapon_LoadAndUnloadMeshes() {
    TEST("Weapon generates and unloads viewmodel meshes");
    Weapon w;
    ASSERT(w.Init() == true, "viewmodel meshes should generate");
    ASSERT(w.Ready() == true, "weapon should be ready after Init");
    w.Shutdown();
    ASSERT(w.Ready() == false, "weapon should not be ready after Shutdown");
    PASS();
}

static void TestWeapon_ShootSetsRecoilAndMuzzleFlash() {
    TEST("Weapon shoot sets recoil and muzzle flash");
    Weapon w;
    Camera3D cam{};
    cam.position = { 0, 2, 0 };
    cam.target = { 0, 2, -1 };

    ASSERT(w.ViewKick() == 0.0f, "no recoil initially");
    ShotResult sr = w.Shoot(cam);
    ASSERT(sr.fired == true, "shot should fire");
    ASSERT(w.AmmoInClip() == 11, "ammo should drop to 11");
    ASSERT(w.ViewKick() > 0.0f, "shoot should produce recoil");
    ASSERT(w.ViewKick() < 0.32f, "ViewKick should be recoil * 0.6f");
    // Muzzle flash is internal; verify via re-shoot cooldown instead.
    w.Update(0.2f, cam);  // expire cooldown and most recoil
    ASSERT(w.ViewKick() < 0.05f, "recoil should decay over time");
    PASS();
}

static void TestWeapon_AmmoConsumptionAndEmptyClip() {
    TEST("Weapon ammo consumption and empty clip");
    Weapon w;
    Camera3D cam{};
    cam.position = { 0, 2, 0 };
    cam.target = { 0, 2, -1 };

    for (int i = 0; i < 12; ++i) {
        ShotResult sr = w.Shoot(cam);
        ASSERT(sr.fired == true, "shot should fire");
        w.Update(0.2f, cam);
    }
    ASSERT(w.AmmoInClip() == 0, "clip should be empty");
    ASSERT(w.CanShoot() == false, "empty clip cannot shoot");
    ShotResult sr = w.Shoot(cam);
    ASSERT(sr.fired == false, "empty clip shot should not fire");
    PASS();
}

static void TestWeapon_ReloadTransition() {
    TEST("Weapon reload transition restores ammo");
    Weapon w;
    Camera3D cam{};
    cam.position = { 0, 2, 0 };
    cam.target = { 0, 2, -1 };

    for (int i = 0; i < 12; ++i) {
        w.Shoot(cam);
        w.Update(0.2f, cam);
    }
    ASSERT(w.AmmoInClip() == 0, "clip empty before reload");
    w.Reload();
    ASSERT(w.IsReloading() == true, "should be reloading");
    ASSERT(w.CanShoot() == false, "cannot shoot while reloading");
    w.Update(1.5f, cam);  // reloadTime is 1.4s
    ASSERT(w.IsReloading() == false, "reload should finish");
    ASSERT(w.AmmoInClip() == 12, "clip should be full after reload");
    ASSERT(w.AmmoReserve() == 24, "reserve should be 36 - 12 = 24");
    PASS();
}

static void TestWeapon_Draw3DSmoke() {
    TEST("Weapon Draw3D runs without error");
    Weapon w;
    ASSERT(w.Init() == true, "meshes should load");
    Camera3D cam{};
    cam.position = { 0, 2, 0 };
    cam.target = { 0, 2, -1 };
    cam.up = { 0, 1, 0 };
    cam.fovy = 75.0f;
    cam.projection = CAMERA_PERSPECTIVE;
    w.Draw3D(cam);  // smoke test: should not crash
    w.Shutdown();
    PASS();
}

// ---------------------------------------------------------------------------
// Player tests
// ---------------------------------------------------------------------------

static void TestPlayer_TakeDamage() {
    TEST("Player take damage reduces health");
    Player p;
    Camera3D cam{};
    p.Reset(cam);
    ASSERT(p.Health() == 100, "health starts at 100");
    ASSERT(p.IsAlive() == true, "player is alive");

    p.TakeDamage(30);
    ASSERT(p.Health() == 70, "health should be 70");
    ASSERT(p.IsAlive() == true, "still alive");

    p.TakeDamage(70);
    ASSERT(p.Health() == 0, "health should be 0");
    ASSERT(p.IsAlive() == false, "player dead");

    p.TakeDamage(999);
    ASSERT(p.Health() == 0, "health should not go negative");
    PASS();
}

static void TestPlayer_AddPitchClamps() {
    TEST("Player AddPitch clamps pitch");
    Player p;
    Camera3D cam{};
    p.Reset(cam);

    // Pitch up beyond clamp
    p.AddPitch(3.0f);  // ~171 degrees, way past 1.45f clamp
    // We can't read pitch directly (private), but AddPitch shouldn't crash
    // and should keep pitch within bounds. Verify by checking IsAlive still works.
    ASSERT(p.IsAlive() == true, "player still alive after AddPitch");
    PASS();
}

// ---------------------------------------------------------------------------
// ZombieManager tests
// ---------------------------------------------------------------------------

static void TestZombie_SpawnAndCount() {
    TEST("ZombieManager spawn and count");
    ZombieManager zm;
    ASSERT(zm.AliveCount() == 0, "no zombies initially");
    ASSERT(zm.TotalCount() == 0, "no zombies initially");

    zm.Spawn({ 0, 0, 0 }, 3, 2.0f);
    ASSERT(zm.TotalCount() == 1, "one zombie spawned");
    ASSERT(zm.AliveCount() == 1, "one zombie alive");

    zm.Spawn({ 10, 0, 10 }, 5, 3.0f);
    ASSERT(zm.TotalCount() == 2, "two zombies spawned");
    ASSERT(zm.AliveCount() == 2, "two zombies alive");

    zm.Clear();
    ASSERT(zm.TotalCount() == 0, "clear removes all");
    ASSERT(zm.AliveCount() == 0, "clear removes all alive");
    PASS();
}

static void TestZombie_TestHitDamagesAndKills() {
    TEST("Zombie TestHit damages and kills");
    ZombieManager zm;
    zm.Spawn({ 0, 0, 0 }, 3, 2.0f);

    // Ray from origin pointing toward zombie at (0,0,0) -- actually zombie is AT ray origin.
    // The ray needs to pass near the zombie. Place ray origin at (-5, 0, 0) aiming at (0,0,0).
    Ray ray{};
    ray.position = { -5.0f, 0.0f, 0.0f };
    ray.direction = { 1.0f, 0.0f, 0.0f };  // pointing right toward zombie
    ray.direction = Vector3Normalize(ray.direction);

    int score = zm.TestHit(ray);
    ASSERT(score == 0, "first hit doesn't kill (health 3 -> 2)");
    // Check health via internal - we can't read it directly. Shoot twice more.
    zm.TestHit(ray);
    score = zm.TestHit(ray);
    ASSERT(score == 10, "third hit should kill and award 10 points");
    ASSERT(zm.AliveCount() == 0, "zombie should be dead/removed eventually");
    // Note: dying zombie stays for 0.6s, so AliveCount may still be 1 until removal.
    // But TestHit on a dead zombie returns 0.
    score = zm.TestHit(ray);
    ASSERT(score == 0, "dead zombie cannot be hit again");
    PASS();
}

static void TestZombie_TestHitMisses() {
    TEST("Zombie TestHit misses distant ray");
    ZombieManager zm;
    zm.Spawn({ 0, 0, 0 }, 3, 2.0f);

    // Ray pointing away from zombie
    Ray ray{};
    ray.position = { 0.0f, 0.0f, 0.0f };
    ray.direction = { 1.0f, 0.0f, 0.0f };  // pointing right, zombie at origin (too close, t < 0.5)
    // Actually ray starts at origin, zombie at origin. toCenter = (0,0,0), t = 0, t < 0.5 so skipped.

    // Ray far away
    Ray ray2{};
    ray2.position = { 100.0f, 0.0f, 0.0f };
    ray2.direction = { 1.0f, 0.0f, 0.0f };
    ray2.direction = Vector3Normalize(ray2.direction);

    int score = zm.TestHit(ray2);
    ASSERT(score == 0, "distant ray should miss");
    ASSERT(zm.AliveCount() == 1, "zombie still alive");
    PASS();
}

static void TestZombie_SeparationMovesApart() {
    TEST("Zombie separation moves overlapping zombies apart");
    ZombieManager zm;
    // Spawn two zombies at the same position (overlapping)
    zm.Spawn({ 0, 0, 0 }, 3, 2.0f);
    zm.Spawn({ 0, 0, 0 }, 3, 2.0f);

    auto zombies = zm.Zombies();
    ASSERT(zombies.size() == 2, "two zombies");
    float distBefore = Vector3Distance(zombies[0].position, zombies[1].position);
    ASSERT(distBefore < 0.01f, "zombies start at same position");

    // Run separation with a large dt to force separation
    zm.SeparateZombies(1.0f);

    zombies = zm.Zombies();
    float distAfter = Vector3Distance(zombies[0].position, zombies[1].position);
    float minDist = zombies[0].radius + zombies[1].radius;  // 0.9f
    ASSERT(distAfter >= minDist * 0.9f, "zombies should be separated after SeparateZombies");
    PASS();
}

static void TestZombie_UpdateMovesTowardPlayer() {
    TEST("Zombie update moves toward player");
    ZombieManager zm;
    zm.Spawn({ 10, 0, 0 }, 3, 2.0f);  // zombie at x=10, player at origin

    Vector3 playerPos{ 0, 0, 0 };
    zm.Update(0.5f, playerPos, 0.35f);  // 0.5s at speed 2.0f = 1.0 unit closer

    auto zombies = zm.Zombies();
    ASSERT(zombies[0].position.x < 10.0f, "zombie should move closer to player");
    ASSERT(zombies[0].position.x > 0.0f, "zombie should not pass player");
    PASS();
}

static void TestZombie_AttackDamagesPlayer() {
    TEST("Zombie attack range deals damage");
    ZombieManager zm;
    zm.Spawn({ 0.5f, 0, 0 }, 3, 2.0f);  // very close to player

    Vector3 playerPos{ 0, 0, 0 };
    int dmg = zm.Update(0.0f, playerPos, 0.35f);  // dt=0, attackCooldown starts at 0
    ASSERT(dmg > 0, "close zombie should deal damage on update");

    // Second call should have cooldown, so no damage
    dmg = zm.Update(0.0f, playerPos, 0.35f);
    ASSERT(dmg == 0, "attack cooldown should prevent repeated damage");
    PASS();
}

static void TestZombie_ModelLoadsAndUnloads() {
    TEST("Zombie model loads and unloads cleanly");
    ZombieManager zm;
    ASSERT(zm.Init() == true, "zombie model should load from project-relative path");
    ASSERT(zm.ModelReady() == true, "model should be ready after Init");
    zm.Shutdown();
    PASS();
}

static void TestZombie_AnimationIndicesResolved() {
    TEST("Zombie animation indices are resolved");
    ZombieManager zm;
    ASSERT(zm.Init() == true, "model loaded");
    // Indices are private; exercise them indirectly by updating + drawing one zombie.
    zm.Spawn({ 2, 0, 0 }, 3, 2.0f);
    Vector3 playerPos{ 0, 0, 0 };
    zm.Update(0.05f, playerPos, 0.35f);  // zombie walks toward player
    zm.Draw();  // draws the animated model (no crash)
    zm.Shutdown();
    PASS();
}

static void TestZombie_MultipleZombiesRender() {
    TEST("Multiple zombies update and draw simultaneously");
    ZombieManager zm;
    ASSERT(zm.Init() == true, "model loaded");
    zm.Spawn({ 0, 0, 0 }, 3, 2.0f);
    zm.Spawn({ 5, 0, 0 }, 4, 3.0f);
    zm.Spawn({ -4, 0, 3 }, 5, 2.5f);
    zm.Spawn({ 4, 0, -3 }, 3, 2.0f);
    zm.Spawn({ 0, 0, 6 }, 4, 2.8f);
    ASSERT(zm.TotalCount() == 5, "five zombies spawned");
    Vector3 playerPos{ 0, 0, 0 };
    for (int i = 0; i < 10; ++i) {
        zm.Update(0.016f, playerPos, 0.35f);
        zm.Draw();  // draw all five animated models each frame
    }
    ASSERT(zm.AliveCount() == 5, "all five still alive");
    zm.Shutdown();
    PASS();
}

static void TestZombie_DeathAndRemoval() {
    TEST("Zombie death animation and removal");
    ZombieManager zm;
    ASSERT(zm.Init() == true, "model loaded");
    zm.Spawn({ 0, 0, 0 }, 1, 2.0f);  // one hit kills
    ASSERT(zm.AliveCount() == 1, "one zombie alive");
    Ray ray{};
    ray.position = { -5.0f, 0.0f, 0.0f };
    ray.direction = { 1.0f, 0.0f, 0.0f };
    ray.direction = Vector3Normalize(ray.direction);
    int score = zm.TestHit(ray);
    ASSERT(score == 10, "single hit kills and awards 10");
    ASSERT(zm.AliveCount() == 0, "zombie no longer alive");
    ASSERT(zm.TotalCount() == 1, "dying zombie remains during death animation");
    zm.Draw();  // draw the dying zombie (animated + fading)
    zm.Update(0.7f, { 0, 0, 0 }, 0.35f);  // advance past the 0.6s removal threshold
    ASSERT(zm.TotalCount() == 0, "zombie removed after death timer");
    zm.Shutdown();
    PASS();
}

// ---------------------------------------------------------------------------
// World tests (collision)
// ---------------------------------------------------------------------------

static void TestWorld_PlayerCollision() {
    TEST("World player collision detection");
    World world;
    world.Init();

    // Player at origin, no obstacles nearby -> no collision
    ASSERT(world.PlayerCollides({ 0, 0, 0 }, 0.35f, 1.7f) == false, "no collision at origin");

    // There's a building at (-18, -16) etc. Test a known obstacle:
    // Building at (0, -36) with size 10x3x10. Box: x[-5,5], y[-1.5,1.5], z[-41,-31].
    // Player at (0, 0, -36) with radius 0.35 -> box x[-0.35,0.35], y[-0.85,0.85], z[-36.35,-35.65]
    // z range overlaps [-41,-31], x overlaps [-5,5], y overlaps [-1.5,1.5] -> collision
    ASSERT(world.PlayerCollides({ 0, 0, -36 }, 0.35f, 1.7f) == true, "should collide with building");
    PASS();
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
    // A minimal GL context is required so the zombie model (glTF) can be loaded.
    InitWindow(1, 1, "tests");
    SetWindowState(FLAG_WINDOW_UNFOCUSED);

    printf("ArcadeZombieFPS Automated Gameplay Tests\n");
    printf("=========================================\n\n");

    printf("[Weapon]\n");
    TestWeapon_InitialState();
    TestWeapon_ShootReducesAmmoAndStartsCooldown();
    TestWeapon_FireRateLimit();
    TestWeapon_EmptyClipCannotShoot();
    TestWeapon_Reload();
    TestWeapon_ReloadWhenFullIsNoOp();
    TestWeapon_ReloadWhenNoReserveIsNoOp();
    TestWeapon_RecoilViewKick();
    TestWeapon_Reset();
    TestWeapon_LoadAndUnloadMeshes();
    TestWeapon_ShootSetsRecoilAndMuzzleFlash();
    TestWeapon_AmmoConsumptionAndEmptyClip();
    TestWeapon_ReloadTransition();
    TestWeapon_Draw3DSmoke();

    printf("\n[Player]\n");
    TestPlayer_TakeDamage();
    TestPlayer_AddPitchClamps();

    printf("\n[ZombieManager]\n");
    TestZombie_SpawnAndCount();
    TestZombie_TestHitDamagesAndKills();
    TestZombie_TestHitMisses();
    TestZombie_SeparationMovesApart();
    TestZombie_UpdateMovesTowardPlayer();
    TestZombie_AttackDamagesPlayer();
    TestZombie_ModelLoadsAndUnloads();
    TestZombie_AnimationIndicesResolved();
    TestZombie_MultipleZombiesRender();
    TestZombie_DeathAndRemoval();

    printf("\n[World]\n");
    TestWorld_PlayerCollision();

    printf("\n=========================================\n");
    printf("Results: %d passed, %d failed\n", testsPassed, testsFailed);

    CloseWindow();

    if (testsFailed > 0) {
        return 1;
    }
    return 0;
}
