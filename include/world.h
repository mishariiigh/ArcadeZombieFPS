#pragma once

#include "raylib.h"
#include <vector>

struct Obstacle {
    BoundingBox box{};
    Color color{ 120, 110, 100, 255 };
    bool solid = true;
};

class World {
public:
    World() = default;
    ~World() = default;

    void Init();
    void Update(float dt);
    void Draw() const;

    float GroundHeight() const { return 0.0f; }
    const std::vector<Obstacle>& Obstacles() const { return obstacles; }

    // Returns true if the player's AABB (center, radius, height) collides with any solid obstacle.
    bool PlayerCollides(const Vector3& center, float radius, float height) const;

    void DrawGround() const;
    void DrawObstacles() const;

private:
    std::vector<Obstacle> obstacles;
    Vector3 groundCenter{ 0, 0, 0 };
    Vector3 groundSize{ 120, 1, 120 };
    Color groundColor{ 70, 90, 60, 255 };
};
