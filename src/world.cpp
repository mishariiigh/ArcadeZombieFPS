#include "world.h"
#include <algorithm>

static BoundingBox MakeBox(float cx, float cy, float cz, float sx, float sy, float sz) {
    BoundingBox b;
    b.min = { cx - sx / 2.0f, cy - sy / 2.0f, cz - sz / 2.0f };
    b.max = { cx + sx / 2.0f, cy + sy / 2.0f, cz + sz / 2.0f };
    return b;
}

void World::Init() {
    obstacles.clear();

    const float wallHalf = groundSize.x / 2.0f - 1.0f;
    const float wallThick = 2.0f;
    const float wallHeight = 2.5f;
    struct Wall { float x, z, sx, sz; };
    Wall walls[] = {
        { -wallHalf, 0.0f, wallThick, groundSize.x },
        {  wallHalf, 0.0f, wallThick, groundSize.x },
        { 0.0f, -wallHalf, groundSize.x, wallThick },
        { 0.0f,  wallHalf, groundSize.x, wallThick },
    };
    for (const auto& w : walls) {
        Obstacle o;
        o.box = MakeBox(w.x, wallHeight / 2.0f, w.z, w.sx, wallHeight, w.sz);
        o.color = Color{ 88, 84, 80, 255 };   // concrete perimeter walls
        o.solid = true;
        obstacles.push_back(o);
    }

    struct Cover { float x, z, sx, sy, sz; };
    Cover covers[] = {
        { -14,  -8, 6, 1.6, 3 },
        {  14,  10, 6, 1.6, 3 },
        {  -8,  18, 3, 1.6, 6 },
        {  10, -20, 3, 1.6, 6 },
    };
    for (const auto& c : covers) {
        Obstacle o;
        o.box = MakeBox(c.x, c.sy / 2.0f, c.z, c.sx, c.sy, c.sz);
        o.color = Color{ 96, 74, 52, 255 };   // rusted metal cover
        o.solid = true;
        obstacles.push_back(o);
    }

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
        o.color = Color{ 74, 78, 84, 255 };   // industrial buildings
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
    Vector3 baseCenter = { groundCenter.x, groundCenter.y - groundSize.y, groundCenter.z };
    Vector3 baseSize = { groundSize.x + 4.0f, groundSize.y * 2.0f, groundSize.z + 4.0f };
    DrawCubeV(baseCenter, baseSize, groundBaseColor);

    Vector3 center = { groundCenter.x, groundCenter.y - groundSize.y / 2.0f, groundCenter.z };
    DrawCubeV(center, groundSize, groundColor);

    // Subtle lighter slab under the play area rim, well below the walking
    // surface so it can never z-fight. Keeps the arena visually framed
    // without DrawLine3D grids (which shimmered/aliased while moving).
    Vector3 slabCenter = { groundCenter.x, groundCenter.y - groundSize.y / 2.0f - 0.06f, groundCenter.z };
    Vector3 slabSize = { groundSize.x - 12.0f, groundSize.y, groundSize.z - 12.0f };
    DrawCubeV(slabCenter, slabSize, ColorAlpha(groundColor, 0.35f));
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
        // Solid faces only: the old wire outlines flickered badly at distance.
        // Shade top faces lighter for cheap ambient-occlusion-style contrast
        // between roofs, walls, and ground.
        Color top = Color{ (unsigned char)std::min(255, o.color.r + 28),
                           (unsigned char)std::min(255, o.color.g + 28),
                           (unsigned char)std::min(255, o.color.b + 28), 255 };
        Vector3 topCenter = { center.x, o.box.max.y - 0.02f, center.z };
        Vector3 topSize = { size.x, 0.04f, size.z };
        DrawCubeV(topCenter, topSize, top);
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
