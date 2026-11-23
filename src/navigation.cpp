#include "navigation.h"
#include "world.h"
#include <algorithm>
#include <cmath>
#include <queue>
#include <limits>

namespace {

struct CompareNode {
    bool operator()(const int a, const int b) const {
        // This will be set via lambda with capture, so we don't use this directly
        return false;
    }
};

} // namespace

void Navigation::Build(const std::vector<Obstacle>& obstacles, float arenaHalfExtent_, float cellSize_) {
    arenaHalfExtent = arenaHalfExtent_;
    cellSize = cellSize_;

    // Grid covers [-arenaHalfExtent, arenaHalfExtent] in X and Z
    gridWidth = static_cast<int>(std::ceil(2.0f * arenaHalfExtent / cellSize));
    gridDepth = static_cast<int>(std::ceil(2.0f * arenaHalfExtent / cellSize));

    // Grid origin is the world position of cell (0,0) center
    gridOrigin = { -arenaHalfExtent + cellSize * 0.5f, 0.0f, -arenaHalfExtent + cellSize * 0.5f };

    nodes.assign(gridWidth * gridDepth, NavNode{});

    // Initialize node coordinates
    for (int z = 0; z < gridDepth; ++z) {
        for (int x = 0; x < gridWidth; ++x) {
            int idx = GridIndex(x, z);
            nodes[idx].x = x;
            nodes[idx].z = z;
        }
    }

    // Mark blocked cells based on obstacles
    for (const auto& obs : obstacles) {
        if (!obs.solid) continue;

        // Convert obstacle bounds to grid coordinates
        int minX = static_cast<int>(std::floor((obs.box.min.x - gridOrigin.x) / cellSize + 0.5f));
        int maxX = static_cast<int>(std::floor((obs.box.max.x - gridOrigin.x) / cellSize + 0.5f));
        int minZ = static_cast<int>(std::floor((obs.box.min.z - gridOrigin.z) / cellSize + 0.5f));
        int maxZ = static_cast<int>(std::floor((obs.box.max.z - gridOrigin.z) / cellSize + 0.5f));

        // Clamp to grid bounds
        minX = std::max(0, minX);
        maxX = std::min(gridWidth - 1, maxX);
        minZ = std::max(0, minZ);
        maxZ = std::min(gridDepth - 1, maxZ);

        for (int z = minZ; z <= maxZ; ++z) {
            for (int x = minX; x <= maxX; ++x) {
                // Check if cell center is actually inside the obstacle
                Vector3 cellCenter = GridToWorld(x, z);
                if (cellCenter.x >= obs.box.min.x && cellCenter.x <= obs.box.max.x &&
                    cellCenter.z >= obs.box.min.z && cellCenter.z <= obs.box.max.z) {
                    nodes[GridIndex(x, z)].blocked = true;
                }
            }
        }
    }

    // Also block perimeter walls explicitly (they're at the edge of arena)
    // The world already includes these as obstacles, but add a safety margin
    const int margin = 1; // 1 cell margin
    for (int x = 0; x < gridWidth; ++x) {
        for (int m = 0; m < margin; ++m) {
            if (m < gridDepth) nodes[GridIndex(x, m)].blocked = true;
            if (m < gridDepth) nodes[GridIndex(x, gridDepth - 1 - m)].blocked = true;
        }
    }
    for (int z = 0; z < gridDepth; ++z) {
        for (int m = 0; m < margin; ++m) {
            if (m < gridWidth) nodes[GridIndex(m, z)].blocked = true;
            if (m < gridWidth) nodes[GridIndex(gridWidth - 1 - m, z)].blocked = true;
        }
    }

    ready = true;
}

bool Navigation::WorldToGrid(const Vector3& world, int& gx, int& gz) const {
    if (!ready) return false;
    float relX = world.x - gridOrigin.x;
    float relZ = world.z - gridOrigin.z;
    gx = static_cast<int>(std::floor(relX / cellSize + 0.5f));
    gz = static_cast<int>(std::floor(relZ / cellSize + 0.5f));
    return InBounds(gx, gz);
}

Vector3 Navigation::GridToWorld(int gx, int gz) const {
    return { gridOrigin.x + gx * cellSize, 0.0f, gridOrigin.z + gz * cellSize };
}

bool Navigation::IsWalkable(const Vector3& pos) const {
    if (!ready) return true;
    int gx, gz;
    if (!WorldToGrid(pos, gx, gz)) return false;
    return !nodes[GridIndex(gx, gz)].blocked;
}

Vector3 Navigation::GetNearestWalkable(const Vector3& pos) const {
    if (!ready) return pos;
    int gx, gz;
    if (WorldToGrid(pos, gx, gz) && !nodes[GridIndex(gx, gz)].blocked) {
        return pos;
    }

    // Spiral search for nearest walkable cell
    const int maxRadius = std::max(gridWidth, gridDepth);
    for (int r = 1; r < maxRadius; ++r) {
        for (int dx = -r; dx <= r; ++dx) {
            for (int dz = -r; dz <= r; ++dz) {
                if (std::abs(dx) != r && std::abs(dz) != r) continue; // Only perimeter
                int nx = gx + dx;
                int nz = gz + dz;
                if (InBounds(nx, nz) && !nodes[GridIndex(nx, nz)].blocked) {
                    return GridToWorld(nx, nz);
                }
            }
        }
    }
    return pos; // Fallback
}

float Navigation::Heuristic(int x1, int z1, int x2, int z2) const {
    // Octile distance for 8-directional movement
    int dx = std::abs(x1 - x2);
    int dz = std::abs(z1 - z2);
    return static_cast<float>(std::max(dx, dz)) + (std::sqrt(2.0f) - 1.0f) * static_cast<float>(std::min(dx, dz));
}

bool Navigation::LineOfSight(int x1, int z1, int x2, int z2) const {
    // Bresenham's line algorithm to check if path is clear
    int dx = std::abs(x2 - x1);
    int dz = std::abs(z2 - z1);
    int sx = (x1 < x2) ? 1 : -1;
    int sz = (z1 < z2) ? 1 : -1;
    int err = dx - dz;

    int x = x1;
    int z = z1;

    while (true) {
        if (!InBounds(x, z) || nodes[GridIndex(x, z)].blocked) {
            return false;
        }
        if (x == x2 && z == z2) break;

        int e2 = 2 * err;
        if (e2 > -dz) {
            err -= dz;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            z += sz;
        }
    }
    return true;
}

std::vector<Vector3> Navigation::FindPath(const Vector3& start, const Vector3& goal) const {
    std::vector<Vector3> path;
    if (!ready) return path;

    int startX, startZ, goalX, goalZ;
    if (!WorldToGrid(start, startX, startZ) || !WorldToGrid(goal, goalX, goalZ)) {
        return path;
    }

    // If start or goal is blocked, find nearest walkable
    if (nodes[GridIndex(startX, startZ)].blocked) {
        Vector3 nearest = GetNearestWalkable(start);
        WorldToGrid(nearest, startX, startZ);
    }
    if (nodes[GridIndex(goalX, goalZ)].blocked) {
        Vector3 nearest = GetNearestWalkable(goal);
        WorldToGrid(nearest, goalX, goalZ);
    }

    // Check if start == goal after adjustment
    if (startX == goalX && startZ == goalZ) {
        path.push_back(GridToWorld(startX, startZ));
        return path;
    }

    // Reset node state for this search
    const int totalNodes = gridWidth * gridDepth;
    for (int i = 0; i < totalNodes; ++i) {
        nodes[i].g = 0.0f;
        nodes[i].h = 0.0f;
        nodes[i].f = 0.0f;
        nodes[i].parentX = -1;
        nodes[i].parentZ = -1;
        nodes[i].open = false;
        nodes[i].closed = false;
    }

    // A* with binary heap (priority queue)
    auto cmp = [this](int a, int b) {
        return nodes[a].f > nodes[b].f;
    };
    std::priority_queue<int, std::vector<int>, decltype(cmp)> openSet(cmp);

    int startIdx = GridIndex(startX, startZ);
    int goalIdx = GridIndex(goalX, goalZ);

    nodes[startIdx].g = 0.0f;
    nodes[startIdx].h = Heuristic(startX, startZ, goalX, goalZ);
    nodes[startIdx].f = nodes[startIdx].h;
    nodes[startIdx].open = true;
    openSet.push(startIdx);

    // 8-directional movement
    const int dirs[8][2] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1},
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };
    const float dirCost[8] = {
        1.0f, 1.0f, 1.0f, 1.0f,
        1.41421356f, 1.41421356f, 1.41421356f, 1.41421356f
    };

    bool found = false;
    while (!openSet.empty()) {
        int currentIdx = openSet.top();
        openSet.pop();

        if (nodes[currentIdx].closed) continue;
        nodes[currentIdx].closed = true;
        nodes[currentIdx].open = false;

        if (currentIdx == goalIdx) {
            found = true;
            break;
        }

        int cx = nodes[currentIdx].x;
        int cz = nodes[currentIdx].z;

        for (int i = 0; i < 8; ++i) {
            int nx = cx + dirs[i][0];
            int nz = cz + dirs[i][1];

            if (!InBounds(nx, nz)) continue;
            int neighborIdx = GridIndex(nx, nz);
            if (nodes[neighborIdx].blocked || nodes[neighborIdx].closed) continue;

            // For diagonal movement, check if cutting corners through blocked cells
            if (i >= 4) { // Diagonal
                int adjX1 = cx + dirs[i][0];
                int adjZ1 = cz;
                int adjX2 = cx;
                int adjZ2 = cz + dirs[i][1];
                if (InBounds(adjX1, adjZ1) && nodes[GridIndex(adjX1, adjZ1)].blocked) continue;
                if (InBounds(adjX2, adjZ2) && nodes[GridIndex(adjX2, adjZ2)].blocked) continue;
            }

            float tentativeG = nodes[currentIdx].g + dirCost[i];
            if (!nodes[neighborIdx].open || tentativeG < nodes[neighborIdx].g) {
                nodes[neighborIdx].parentX = cx;
                nodes[neighborIdx].parentZ = cz;
                nodes[neighborIdx].g = tentativeG;
                nodes[neighborIdx].h = Heuristic(nx, nz, goalX, goalZ);
                nodes[neighborIdx].f = nodes[neighborIdx].g + nodes[neighborIdx].h;
                if (!nodes[neighborIdx].open) {
                    nodes[neighborIdx].open = true;
                    openSet.push(neighborIdx);
                }
            }
        }
    }

    if (!found) {
        return path; // Empty path = unreachable
    }

    // Reconstruct path with string pulling (line-of-sight optimization)
    std::vector<Vector3> rawPath;
    int cx = goalX;
    int cz = goalZ;
    rawPath.push_back(GridToWorld(cx, cz));
    while (!(cx == startX && cz == startZ)) {
        int px = nodes[GridIndex(cx, cz)].parentX;
        int pz = nodes[GridIndex(cx, cz)].parentZ;
        if (px < 0 || pz < 0) break;
        cx = px;
        cz = pz;
        rawPath.push_back(GridToWorld(cx, cz));
    }
    std::reverse(rawPath.begin(), rawPath.end());

    // String pulling: simplify path by removing intermediate waypoints that have line of sight
    if (rawPath.size() <= 2) {
        return rawPath;
    }

    path.push_back(rawPath[0]);
    int lastKept = 0;
    for (size_t i = 2; i < rawPath.size(); ++i) {
        int gx1, gz1, gx2, gz2;
        WorldToGrid(rawPath[lastKept], gx1, gz1);
        WorldToGrid(rawPath[i], gx2, gz2);
        if (!LineOfSight(gx1, gz1, gx2, gz2)) {
            // Keep the previous point
            path.push_back(rawPath[i - 1]);
            lastKept = i - 1;
        }
    }
    path.push_back(rawPath.back());

    return path;
}