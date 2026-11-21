#include "world.h"

static BoundingBox MakeBox(float cx, float cy, float cz, float sx, float sy, float sz) {
    BoundingBox b;
    b.min = { cx - sx / 2.0f, cy - sy / 2.0f, cz - sz / 2.0f };
    b.max = { cx + sx / 2.0f, cy + sy / 2.0f, cz + sz / 2.0f };
    return b;
}

void World::Init() {
    obstacles.clear();

    struct Building { float x, z, sx, sy, sz; };
    Building buildings[] = {
        {-18, -16, 6, 4, 6},
        { 22, -10, 8, 5, 4},
        {-26,  12, 5, 3, 7},
        { 16,  22, 6, 6, 5},
        { -9,  28, 4, 3, 6},
        { 30,   5, 7, 4, 7},
        {-30, -26, 5, 5, 5},
        {  0, -36, 10, 3, 10},
        { 34, -20, 4, 4, 4},
        {-34,  18, 6, 4, 6},
        { 10, -30, 5, 6, 5},
        { -2,  40, 7, 3, 7},
    };

    for (const auto& b : buildings) {
        Obstacle o;
        o.box = MakeBox(b.x, b.sy / 2.0f, b.z, b.sx, b.sy, b.sz);
        o.color = Color{ 110, 105, 100, 255 };
        o.solid = true;
        obstacles.push_back(o);
    }
}

void World::Update(float /*dt*/) {}

void World::Draw() const {
    DrawGround();
    DrawObstacles();
}

void World::DrawGround() const {
    Vector3 center = { groundCenter.x, groundCenter.y - groundSize.y / 2.0f, groundCenter.z };
    DrawCubeV(center, groundSize, groundColor);
    // Grid lines for ground readability
    int step = 10;
    float half = groundSize.x / 2.0f;
    for (float i = -half; i <= half; i += step) {
        DrawLine3D({ i, 0.05f, -half }, { i, 0.05f, half }, Color{ 50, 60, 45, 255 });
        DrawLine3D({ -half, 0.05f, i }, { half, 0.05f, i }, Color{ 50, 60, 45, 255 });
    }
}

void World::DrawObstacles() const {
    for (const auto& o : obstacles) {
        Vector3 center = { (o.box.min.x + o.box.max.x) / 2.0f,
                           (o.box.min.y + o.box.max.y) / 2.0f,
                           (o.box.min.z + o.box.max.z) / 2.0f };
        Vector3 size = { o.box.max.x - o.box.min.x,
                         o.box.max.y - o.box.min.y,
                         o.box.max.z - o.box.min.z };
        DrawCubeV(center, size, o.color);
        DrawCubeWiresV(center, size, o.color.r > 200 ? Color{ 200, 190, 180, 255 } : Color{ 60, 55, 50, 255 });
    }
}

bool World::PlayerCollides(const Vector3& center, float radius, float height) const {
    BoundingBox playerBox;
    playerBox.min = { center.x - radius, center.y - height / 2.0f, center.z - radius };
    playerBox.max = { center.x + radius, center.y + height / 2.0f, center.z + radius };

    for (const auto& o : obstacles) {
        if (!o.solid) continue;
        if (CheckCollisionBoxes(playerBox, o.box)) return true;
    }
    return false;
}
