#include "player.h"
#include "world.h"
#include "raymath.h"

Player::Player() {}

void Player::Reset(Camera3D& camera) {
    position = { 0.0f, 2.0f, 0.0f };
    velocity = { 0.0f, 0.0f, 0.0f };
    yaw = 0.0f;
    pitch = 0.0f;
    health = maxHealth;
    onGround = true;
    UpdateCameraVectors(camera);
}

Vector3 Player::Forward() const {
    Vector3 f;
    f.x = cosf(pitch) * sinf(yaw);
    f.y = sinf(pitch);
    f.z = cosf(pitch) * cosf(yaw);
    f.y = 0.0f; // flatten to XZ plane
    return Vector3Normalize(f);
}

void Player::UpdateCameraVectors(Camera3D& camera) {
    Vector3 forward;
    float effectivePitch = pitch + recoilPitch;
    forward.x = cosf(effectivePitch) * sinf(yaw);
    forward.y = sinf(effectivePitch);
    forward.z = cosf(effectivePitch) * cosf(yaw);
    camera.position = position;
    camera.position.y += height;
    camera.target = Vector3Add(camera.position, forward);
    camera.up = { 0.0f, 1.0f, 0.0f };
    camera.fovy = 75.0f;
    camera.projection = CAMERA_PERSPECTIVE;
}

void Player::HandleInput(float dt, Camera3D& camera) {
    int cx = GetScreenWidth() / 2;
    int cy = GetScreenHeight() / 2;

    Vector2 currentMouse = GetMousePosition();
    Vector2 mouseDelta{ 0.0f, 0.0f };
    if (!mouseNeedsReset) {
        mouseDelta = Vector2Subtract(currentMouse, previousMousePosition);
    } else {
        mouseNeedsReset = false;
    }

    yaw += mouseDelta.x * mouseSensitivity;
    pitch -= mouseDelta.y * mouseSensitivity;
    if (pitch > 1.45f) pitch = 1.45f;
    if (pitch < -1.45f) pitch = -1.45f;

    previousMousePosition = { (float)cx, (float)cy };
    SetMousePosition(cx, cy);

    Vector3 forward = Forward();
    Vector3 right = Vector3Normalize(Vector3CrossProduct({ 0.0f, 1.0f, 0.0f }, forward));

    Vector3 move{ 0.0f, 0.0f, 0.0f };
    if (IsKeyDown(KEY_W)) move = Vector3Add(move, forward);
    if (IsKeyDown(KEY_S)) move = Vector3Subtract(move, forward);
    if (IsKeyDown(KEY_D)) move = Vector3Add(move, right);
    if (IsKeyDown(KEY_A)) move = Vector3Subtract(move, right);

    float speed = walkSpeed;
    if (IsKeyDown(KEY_LEFT_SHIFT)) speed = sprintSpeed;

    if (Vector3Length(move) > 0.0f) move = Vector3Normalize(move);
    velocity.x = move.x * speed;
    velocity.z = move.z * speed;

    if (IsKeyPressed(KEY_SPACE) && onGround) {
        velocity.y = jumpVelocity;
        onGround = false;
    }
}

void Player::ApplyRecoil(float radians) {
    recoilPitch = radians;
    if (recoilPitch > 1.45f) recoilPitch = 1.45f;
    if (recoilPitch < -1.45f) recoilPitch = -1.45f;
}

void Player::ResetMouseLook() {
    mouseNeedsReset = true;
    previousMousePosition = { (float)(GetScreenWidth() / 2), (float)(GetScreenHeight() / 2) };
}

void Player::ApplyPhysics(float dt, const World& world) {
    // Horizontal movement with wall sliding
    Vector3 tryX = { position.x + velocity.x * dt, position.y, position.z };
    Vector3 tryZ = { position.x, position.y, position.z + velocity.z * dt };

    if (!world.PlayerCollides(tryX, radius, height)) position = tryX;
    if (!world.PlayerCollides(tryZ, radius, height)) position = tryZ;

    // Vertical movement + gravity
    velocity.y -= gravity * dt;
    Vector3 tryY = { position.x, position.y + velocity.y * dt, position.z };

    float groundY = world.GroundHeight();
    if (tryY.y - height / 2.0f < groundY) {
        position.y = groundY + height / 2.0f;
        velocity.y = 0.0f;
        onGround = true;
    } else {
        position = tryY;
        onGround = false;
    }
}

void Player::Update(float dt, Camera3D& camera, const World& world) {
    HandleInput(dt, camera);

    // Recoil recovery: transient camera kick decays toward zero each frame.
    if (recoilPitch > 0.0f) {
        recoilPitch -= recoilDecay * dt;
        if (recoilPitch < 0.0f) recoilPitch = 0.0f;
    } else if (recoilPitch < 0.0f) {
        recoilPitch += recoilDecay * dt;
        if (recoilPitch > 0.0f) recoilPitch = 0.0f;
    }

    ApplyPhysics(dt, world);
    UpdateCameraVectors(camera);
}

void Player::TakeDamage(int amount) {
    health -= amount;
    if (health < 0) health = 0;
}

void Player::AddPitch(float radians) {
    pitch += radians;
    if (pitch > 1.45f) pitch = 1.45f;
    if (pitch < -1.45f) pitch = -1.45f;
}

void Player::DrawDebug() const {
    DrawSphereWires(position, radius, 8, 8, YELLOW);
    DrawCubeWires({ position.x, position.y - height / 2.0f, position.z },
                  radius * 2.0f, height, radius * 2.0f, GOLD);
}
