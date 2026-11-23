#pragma once

#include "raylib.h"
#include <vector>
#include <array>

struct NavNode {
    int x = 0;
    int z = 0;
    float g = 0.0f;
    float h = 0.0f;
    float f = 0.0f;
    int parentX = -1;
    int parentZ = -1;
    bool open = false;
    bool closed = false;
    bool blocked = false;
};

class Navigation {
public:
    Navigation() = default;
    ~Navigation() = default;

    // Build navigation grid from world obstacles.
    // arenaHalfExtent: half-size of the square arena (e.g., 75.0f for 150x150 arena)
    // cellSize: size of each grid cell in world units (e.g., 1.0f)
    void Build(const std::vector<struct Obstacle>& obstacles, float arenaHalfExtent, float cellSize);

    // Find path from start to goal in world coordinates.
    // Returns empty vector if no path found.
    // Path is in world coordinates (Vector3 with y=0).
    std::vector<Vector3> FindPath(const Vector3& start, const Vector3& goal) const;

    // Check if a world position is walkable (not in a blocked cell).
    bool IsWalkable(const Vector3& pos) const;

    // Get the nearest walkable position to a given position.
    Vector3 GetNearestWalkable(const Vector3& pos) const;

    // Get grid dimensions
    int GridWidth() const { return gridWidth; }
    int GridDepth() const { return gridDepth; }
    float CellSize() const { return cellSize; }
    float ArenaHalfExtent() const { return arenaHalfExtent; }
    const Vector3& GridOrigin() const { return gridOrigin; }

    // Convert world position to grid coordinates
    bool WorldToGrid(const Vector3& world, int& gx, int& gz) const;
    // Convert grid coordinates to world position (cell center)
    Vector3 GridToWorld(int gx, int gz) const;

    bool Ready() const { return ready; }

private:
    mutable std::vector<NavNode> nodes;
    int gridWidth = 0;
    int gridDepth = 0;
    float cellSize = 1.0f;
    float arenaHalfExtent = 75.0f;
    Vector3 gridOrigin{ 0.0f, 0.0f, 0.0f };
    bool ready = false;

    int GridIndex(int x, int z) const { return z * gridWidth + x; }
    bool InBounds(int x, int z) const { return x >= 0 && x < gridWidth && z >= 0 && z < gridDepth; }

    float Heuristic(int x1, int z1, int x2, int z2) const;
    void ReconstructPath(int goalX, int goalZ, std::vector<Vector3>& path) const;
    bool LineOfSight(int x1, int z1, int x2, int z2) const;
};