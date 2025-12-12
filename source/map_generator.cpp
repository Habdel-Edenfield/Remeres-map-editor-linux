//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#include "main.h"
#include "map_generator.h"
#include "map.h"
#include <queue>
#include <set>


MapGenerator::MapGenerator() : noise(nullptr) {
	// Initialize with default seed
	seedRandom("default");
}

MapGenerator::~MapGenerator() {
	if (noise) {
		delete noise;
		noise = nullptr;
	}
}

void MapGenerator::setProgressCallback(ProgressCallback callback) {
	progress_callback = callback;
}

void MapGenerator::seedRandom(const std::string& seed) {
	// Try to parse as numeric value first
	unsigned long long numeric_seed = 0;

	try {
		numeric_seed = std::stoull(seed);
	} catch (...) {
		// If parsing fails, use string hash
		std::hash<std::string> hasher;
		numeric_seed = hasher(seed);
	}

	// Initialize noise generator with 32-bit portion of seed
	delete noise;
	noise = new SimplexNoise(static_cast<unsigned int>(numeric_seed & 0xFFFFFFFF));

	// Initialize RNG with mixed seed
	rng.seed(static_cast<unsigned int>((numeric_seed >> 32) ^ (numeric_seed & 0xFFFFFFFF)));
}

// Helper to check if wall
bool MapGenerator::isWallPosition(const std::vector<std::vector<int>>& grid, int x, int y, int width, int height) {
	if (x < 0 || x >= width || y < 0 || y >= height) return false;
	if (grid[y][x] == 1) return false; // Floor is not wall
	
	// Check neighbors for floor
	static const int dx[] = {-1, 1, 0, 0, -1, -1, 1, 1};
	static const int dy[] = {0, 0, -1, 1, -1, 1, -1, 1};
	
	for (int i = 0; i < 8; ++i) {
		int nx = x + dx[i];
		int ny = y + dy[i];
		
		if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
			if (grid[ny][nx] == 1) return true; // Adjacent to floor
		}
	}
	return false;
}

std::vector<MapGenerator::Room> MapGenerator::generateRooms(const DungeonConfig& config, int width, int height) {
	std::vector<Room> rooms;
	int attempts = 0;
	int max_attempts = config.room_count * 10;
	
	while (rooms.size() < config.room_count && attempts < max_attempts) {
		attempts++;
		
		int w = std::uniform_int_distribution<>(config.min_room_size, config.max_room_size)(rng);
		int h = std::uniform_int_distribution<>(config.min_room_size, config.max_room_size)(rng);
		int x = std::uniform_int_distribution<>(1, width - w - 2)(rng);
		int y = std::uniform_int_distribution<>(1, height - h - 2)(rng);
		
		Room newRoom = {x, y, w, h};
		bool overlap = false;
		
		for (const auto& r : rooms) {
			// Add padding to prevent rooms from touching
			Room expanded1 = {newRoom.x - 1, newRoom.y - 1, newRoom.w + 2, newRoom.h + 2};
			if (expanded1.intersects(r)) {
				overlap = true;
				break;
			}
		}
		
		if (!overlap) {
			rooms.push_back(newRoom);
		}
	}
	return rooms;
}


bool MapGenerator::generateIslandMap(Map* map, const IslandConfig& config,
                                    int width, int height, const std::string& seed,
                                    int startX, int startY) {
	if (!map || width <= 0 || height <= 0) {
		return false;
	}

	// Initialize random with seed
	seedRandom(seed);

	// ═══ Step 1: Generate Height Map ═══
	if (progress_callback && !progress_callback(0, 100)) {
		return false; // User cancelled
	}

	std::vector<std::vector<double>> heightMap = generateHeightMap(config, width, height);

	// ═══ Step 2: Apply Island Mask ═══
	if (progress_callback && !progress_callback(20, 100)) {
		return false;
	}

	applyIslandMask(heightMap, config, width, height);

	// Use passed offsets
	int offsetX = startX;
	int offsetY = startY;

	// ═══ Step 3: Place Tiles ═══
	if (progress_callback && !progress_callback(40, 100)) {
		return false;
	}

	placeTiles(map, heightMap, config, width, height, offsetX, offsetY);

	// ═══ Step 4: Cleanup (if enabled) ═══
	if (config.enable_cleanup) {
		if (progress_callback && !progress_callback(70, 100)) {
			return false;
		}

		cleanupTerrain(map, config, width, height, offsetX, offsetY);
	}

	// ═══ Complete ═══
	if (progress_callback) {
		progress_callback(100, 100);
	}

	return true;
}

std::vector<std::vector<double>> MapGenerator::generateHeightMap(
	const IslandConfig& config, int width, int height) {

	std::vector<std::vector<double>> heightMap(height, std::vector<double>(width, 0.0));

	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			// Sample noise at scaled coordinates
			double nx = x * config.noise_scale;
			double ny = y * config.noise_scale;

			// Use fractal noise for natural variation
			double value = noise->fractal(nx, ny,
			                             config.noise_octaves,
			                             config.noise_persistence,
			                             config.noise_lacunarity);

			// Normalize from [-1, 1] to [0, 1]
			heightMap[y][x] = (value + 1.0) * 0.5;
		}
	}

	return heightMap;
}

bool MapGenerator::generateDungeonMap(Map* map, const DungeonConfig& config,
									 int width, int height, const std::string& seed,
									 int startX, int startY) {
	if (!map) return false;

	// Reset RNG
	seedRandom(seed);
	if (noise) delete noise;
	noise = new SimplexNoise(); 

	// 0 = Void, 1 = Floor
	std::vector<std::vector<int>> grid(height, std::vector<int>(width, 0));

	// 1. Generate Rooms
	std::vector<Room> rooms = generateRooms(config, width, height);

	// Carve Rooms
	for (const auto& room : rooms) {
		for (int y = room.y; y < room.y + room.h; ++y) {
			for (int x = room.x; x < room.x + room.w; ++x) {
				grid[y][x] = 1;
			}
		}
	}

	if (progress_callback && !progress_callback(30, 100)) return false;

	// 2. Generate Intersections (Hubs)
	std::vector<Intersection> intersections;
	if (config.add_intersections) {
		intersections = generateIntersections(config, rooms, width, height);
		for (const auto& intersection : intersections) {
			placeIntersection(grid, intersection);
		}
	}

	// 3. Connect Rooms (Corridors)
	if (config.add_intersections && !intersections.empty()) {
		connectRoomsViaIntersections(grid, rooms, intersections, config, width, height);
	} else {
		generateCorridors(grid, rooms, config, width, height);
	}
	
	// Ensure connectivity if requested (simple sequential pass as fallback/guarantee)
	if (config.connect_all_rooms && rooms.size() > 1) {
		generateCorridors(grid, rooms, config, width, height);
	}

	// 4. Dead Ends
	if (config.add_dead_ends) {
		addDeadEnds(grid, config, width, height);
	}

	if (progress_callback && !progress_callback(60, 100)) return false;

	// 5. Optional: Natural Caves overlay
	if (config.generate_caves) {
		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				double n = noise->noise(x * 0.1, y * 0.1); 
				if (n > config.cave_threshold) { // Carve
					grid[y][x] = 1;
				}
			}
		}
	}

	if (progress_callback && !progress_callback(80, 100)) return false;

	// 6. Place on Map
	int offsetX = startX;
	int offsetY = startY;
	int z = config.target_floor;

	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			int type = grid[y][x]; // 0 = Void, 1 = Floor

			Position pos(x + offsetX, y + offsetY, z);
			Tile* tile = map->getTile(pos);
			
			if (!tile) {
				tile = map->allocator(map->createTileL(pos));
			}

			if (tile) {
				if (tile->ground) {
					delete tile->ground;
					tile->ground = nullptr;
				}

                if (type == 1) { // Floor
                    tile->ground = Item::Create(config.floor_id);
                } else if (isWallPosition(grid, x, y, width, height)) { 
					// Only place wall if adjacent to floor
                    tile->ground = Item::Create(config.floor_id); 
                    Item* wall = Item::Create(config.wall_id);
                    if (wall) tile->addItem(wall);
                }
				map->setTile(pos, tile);
			}
		}
	}
	return true;
}

void MapGenerator::applyIslandMask(std::vector<std::vector<double>>& heightMap,
                                  const IslandConfig& config, int width, int height) {

	double centerX = width / 2.0;
	double centerY = height / 2.0;
	double maxRadius = std::min(width, height) / 2.0;

	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			// Calculate distance from center (normalized to [0, 1])
			double distance = getDistance(x, y, static_cast<int>(centerX), static_cast<int>(centerY));
			double normalizedDistance = distance / (maxRadius * config.island_size);

			// Apply falloff curve
			double falloff = applyFalloff(normalizedDistance, config.island_falloff);

			// Subtract falloff from height (creates island shape)
			heightMap[y][x] -= falloff;

			// Clamp to [0, 1]
			heightMap[y][x] = std::max(0.0, std::min(1.0, heightMap[y][x]));
		}
	}
}

void MapGenerator::placeTiles(Map* map, const std::vector<std::vector<double>>& heightMap,
                             const IslandConfig& config, int width, int height,
                             int offsetX, int offsetY) {

	const int z = config.target_floor;
	int tilesPlaced = 0;
	const int totalTiles = width * height;

	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			double height_value = heightMap[y][x];

			// Determine tile ID based on threshold
			// Convert threshold from [-1, 1] to [0, 1] range
			double normalized_threshold = (config.island_threshold + 1.0) * 0.5;
			uint16_t tileId = (height_value < normalized_threshold) ? config.water_id : config.ground_id;

			// Create or get tile at position
			Position pos(x + offsetX, y + offsetY, z);
			Tile* tile = map->getTile(pos);

			if (!tile) {
				// Create new tile
				tile = map->allocator(map->createTileL(pos));
			}

			if (tile) {
				// Remove existing ground if any
				if (tile->ground) {
					delete tile->ground;
					tile->ground = nullptr;
				}

				// Create new ground item
				Item* groundItem = Item::Create(tileId);
				if (groundItem) {
					tile->ground = groundItem;
				}

				// Set tile back to map (in case it was newly created)
				map->setTile(pos, tile);
			}

			// Update progress periodically
			++tilesPlaced;
			if (progress_callback && tilesPlaced % 1000 == 0) {
				int progress = 40 + static_cast<int>((tilesPlaced / (double)totalTiles) * 30);
				if (!progress_callback(progress, 100)) {
					return; // Cancelled
				}
			}
		}
	}
}

void MapGenerator::cleanupTerrain(Map* map, const IslandConfig& config,
                                 int width, int height,
                                 int offsetX, int offsetY) {

	// Step 1: Remove small land patches
	if (config.min_land_patch_size > 0) {
		removeSmallPatches(map, config.ground_id, config.min_land_patch_size, width, height, offsetX, offsetY);
	}

	if (progress_callback && !progress_callback(80, 100)) {
		return;
	}

	// Step 2: Fill small water holes
	if (config.max_water_hole_size > 0) {
		fillSmallHoles(map, config.water_id, config.ground_id, config.max_water_hole_size, width, height, offsetX, offsetY);
	}

	if (progress_callback && !progress_callback(90, 100)) {
		return;
	}

	// Step 3: Smooth coastline
	if (config.smoothing_passes > 0) {
		smoothCoastline(map, config, width, height, offsetX, offsetY);
	}
}

void MapGenerator::removeSmallPatches(Map* map, uint16_t tile_id, int min_size,
                                     int width, int height,
                                     int offsetX, int offsetY) {

	const int z = 7; // Ground level
	std::vector<std::vector<bool>> visited(height, std::vector<bool>(width, false));

	// Find opposite tile ID for replacement
	uint16_t opposite_id = (tile_id == 4608) ? 4526 : 4608; // Water <-> Grass

	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			if (visited[y][x]) continue;

			Tile* tile = map->getTile(x + offsetX, y + offsetY, z);
			if (!tile || !tile->ground) continue;

			uint16_t groundId = tile->ground->getID();
			if (groundId != tile_id) continue;

			// Found unvisited target tile - flood fill to find patch size
			int patchSize = floodFillCount(map, x, y, z, width, height, tile_id, 0, offsetX, offsetY);

			// If patch is too small, replace it
			if (patchSize < min_size && patchSize > 0) {
				floodFillCount(map, x, y, z, width, height, tile_id, opposite_id, offsetX, offsetY);
			}

			// Mark region as visited
			std::queue<std::pair<int, int>> queue;
			queue.push({x, y});
			visited[y][x] = true;

			while (!queue.empty()) {
				auto [cx, cy] = queue.front();
				queue.pop();

				// Check 4-connected neighbors
				static const int dx[] = {-1, 1, 0, 0};
				static const int dy[] = {0, 0, -1, 1};

				for (int i = 0; i < 4; ++i) {
					int nx = cx + dx[i];
					int ny = cy + dy[i];

					if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
					if (visited[ny][nx]) continue;

					Tile* neighbor = map->getTile(nx + offsetX, ny + offsetY, z);
					if (!neighbor || !neighbor->ground) continue;

					if (neighbor->ground->getID() == tile_id) {
						visited[ny][nx] = true;
						queue.push({nx, ny});
					}
				}
			}
		}
	}
}

void MapGenerator::fillSmallHoles(Map* map, uint16_t target_id, uint16_t fill_id,
                                 int max_size, int width, int height,
                                 int offsetX, int offsetY) {

	// Same algorithm as removeSmallPatches, but fills enclosed areas
	removeSmallPatches(map, target_id, max_size, width, height, offsetX, offsetY);
}

void MapGenerator::smoothCoastline(Map* map, const IslandConfig& config,
                                   int width, int height,
                                   int offsetX, int offsetY) {

	const int z = config.target_floor;

	for (int pass = 0; pass < config.smoothing_passes; ++pass) {
		// Create a copy of current state to avoid feedback
		std::vector<std::vector<uint16_t>> tileIds(height, std::vector<uint16_t>(width, 0));

		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				Tile* tile = map->getTile(x + offsetX, y + offsetY, z);
				if (tile && tile->ground) {
					tileIds[y][x] = tile->ground->getID();
				}
			}
		}

		// Apply smoothing
		for (int y = 1; y < height - 1; ++y) {
			for (int x = 1; x < width - 1; ++x) {
				uint16_t currentId = tileIds[y][x];

				// Count neighbors of each type
				int waterCount = 0;
				int landCount = 0;

				static const int dx[] = {-1, 1, 0, 0, -1, -1, 1, 1}; // 8-connected
				static const int dy[] = {0, 0, -1, 1, -1, 1, -1, 1};

				for (int i = 0; i < 8; ++i) {
					int nx = x + dx[i];
					int ny = y + dy[i];

					if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;

					uint16_t neighborId = tileIds[ny][nx];
					if (neighborId == config.water_id) {
						++waterCount;
					} else if (neighborId == config.ground_id) {
						++landCount;
					}
				}

				// Apply majority voting
				uint16_t newId = currentId;
				if (currentId == config.water_id && landCount > waterCount) {
					newId = config.ground_id;
				} else if (currentId == config.ground_id && waterCount > landCount) {
					newId = config.water_id;
				}

				// Update tile if changed
				if (newId != currentId) {
					Tile* tile = map->getTile(x + offsetX, y + offsetY, z);
					if (tile) {
						if (tile->ground) {
							delete tile->ground;
						}
						tile->ground = Item::Create(newId);
					}
				}
			}
		}
	}
}

int MapGenerator::floodFillCount(Map* map, int x, int y, int z,
                                int width, int height,
                                uint16_t target_id, uint16_t replacement_id,
                                int offsetX, int offsetY) {

	Tile* startTile = map->getTile(x + offsetX, y + offsetY, z);
	if (!startTile || !startTile->ground) return 0;
	if (startTile->ground->getID() != target_id) return 0;

	// Iterative flood fill using BFS
	std::queue<std::pair<int, int>> queue;
	std::set<std::pair<int, int>> visited;

	queue.push({x, y});
	visited.insert({x, y});

	int count = 0;

	while (!queue.empty()) {
		auto [cx, cy] = queue.front();
		queue.pop();

		++count;

		// Replace if requested
		if (replacement_id != 0) {
			Tile* tile = map->getTile(cx + offsetX, cy + offsetY, z);
			if (tile && tile->ground) {
				delete tile->ground;
				tile->ground = Item::Create(replacement_id);
			}
		}

		// Check 4-connected neighbors
		static const int dx[] = {-1, 1, 0, 0};
		static const int dy[] = {0, 0, -1, 1};

		for (int i = 0; i < 4; ++i) {
			int nx = cx + dx[i];
			int ny = cy + dy[i];

			if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
			if (visited.count({nx, ny})) continue;

			Tile* neighbor = map->getTile(nx + offsetX, ny + offsetY, z);
			if (!neighbor || !neighbor->ground) continue;

			if (neighbor->ground->getID() == target_id) {
				visited.insert({nx, ny});
				queue.push({nx, ny});
			}
		}
	}

	return count;
}

double MapGenerator::getDistance(int x, int y, int centerX, int centerY) {
	double dx = x - centerX;
	double dy = y - centerY;
	return std::sqrt(dx * dx + dy * dy);
}

double MapGenerator::applyFalloff(double distance, double falloff) {
	if (distance < 0.0) return 0.0;
	if (distance > 1.0) return 1.0;
	return std::pow(distance, falloff);
}

// ═══ A* Pathfinding ═══

std::vector<std::pair<int, int>> MapGenerator::findShortestPath(
    const std::vector<std::vector<int>>& grid, 
    int x1, int y1, int x2, int y2, 
    int width, int height) {
    
    struct Node {
        int x, y;
        int g, h;
        std::pair<int, int> parent; // Store parent coords directly
        int f() const { return g + h; }
        bool operator>(const Node& other) const { return f() > other.f(); }
    };

    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> openSet;
    std::vector<std::vector<int>> gScore(height, std::vector<int>(width, 999999));
    std::vector<std::vector<std::pair<int, int>>> cameFrom(height, std::vector<std::pair<int, int>>(width, {-1, -1}));
    
    gScore[y1][x1] = 0;
    openSet.push({x1, y1, 0, std::abs(x2 - x1) + std::abs(y2 - y1), {-1, -1}});
    
    while (!openSet.empty()) {
        Node current = openSet.top();
        openSet.pop();
        
        if (current.x == x2 && current.y == y2) {
            std::vector<std::pair<int, int>> path;
            int cx = x2, cy = y2;
            while (cx != -1) {
                path.push_back({cx, cy});
                std::pair<int, int> p = cameFrom[cy][cx];
                cx = p.first;
                cy = p.second;
            }
            std::reverse(path.begin(), path.end());
            return path;
        }
        
        static const int dx[] = {-1, 1, 0, 0};
        static const int dy[] = {0, 0, -1, 1};
        
        for (int i = 0; i < 4; ++i) {
            int nx = current.x + dx[i];
            int ny = current.y + dy[i];
            
            if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                // Determine movement cost
                // Floor (1) = low cost, Wall/Void (0) = high cost
                // We strongly prefer existing floors/corridors
                int cost = (grid[ny][nx] == 1) ? 1 : 5;
                
                int tentative_g = gScore[current.y][current.x] + cost;
                
                if (tentative_g < gScore[ny][nx]) {
                    cameFrom[ny][nx] = {current.x, current.y};
                    gScore[ny][nx] = tentative_g;
                    int h = std::abs(x2 - nx) + std::abs(y2 - ny);
                    openSet.push({nx, ny, tentative_g, h, {current.x, current.y}});
                }
            }
        }
    }
    return {}; // No path
}

void MapGenerator::createCorridorTile(std::vector<std::vector<int>>& grid, int x, int y, int width, int height) {
    if (x >= 0 && x < width && y >= 0 && y < height) {
        grid[y][x] = 1;
    }
}

void MapGenerator::createImprovedCorridor(std::vector<std::vector<int>>& grid, int x1, int y1, int x2, int y2, const DungeonConfig& config, int width, int height) {
    // Basic L-shape corridor with width
    int cx = x1;
    int cy = y1;
    
    // Horizontal then Vertical or vice-versa
    if (std::uniform_int_distribution<>(0, 1)(rng)) {
        while (cx != x2) {
            cx += (x2 > cx) ? 1 : -1;
            for(int w=0; w<config.corridor_width; ++w) createCorridorTile(grid, cx, cy+w, width, height);
        }
        while (cy != y2) {
            cy += (y2 > cy) ? 1 : -1;
             for(int w=0; w<config.corridor_width; ++w) createCorridorTile(grid, cx, cy+(x2>x1?w:0), width, height); // Simplified width
             for(int w=0; w<config.corridor_width; ++w) createCorridorTile(grid, cx+w, cy, width, height);
        }
    } else {
         while (cy != y2) {
            cy += (y2 > cy) ? 1 : -1;
            for(int w=0; w<config.corridor_width; ++w) createCorridorTile(grid, cx+w, cy, width, height);
        }
        while (cx != x2) {
             cx += (x2 > cx) ? 1 : -1;
             for(int w=0; w<config.corridor_width; ++w) createCorridorTile(grid, cx, cy+w, width, height);
        }
    }
}

void MapGenerator::createSmartCorridor(std::vector<std::vector<int>>& grid, 
                                      int x1, int y1, int x2, int y2, 
                                      const DungeonConfig& config, 
                                      int width, int height) {
    if (config.use_smart_pathfinding) {
        auto path = findShortestPath(grid, x1, y1, x2, y2, width, height);
        if (!path.empty()) {
            for (const auto& p : path) {
                // Apply width
                for(int dy = 0; dy < config.corridor_width; ++dy) {
                    for(int dx = 0; dx < config.corridor_width; ++dx) {
                        createCorridorTile(grid, p.first + dx, p.second + dy, width, height);
                    }
                }
            }
            return;
        }
    }
    // Fallback
    createImprovedCorridor(grid, x1, y1, x2, y2, config, width, height);
}


// ═══ Intersections ═══

std::vector<MapGenerator::Intersection> MapGenerator::generateIntersections(
    const DungeonConfig& config, 
    const std::vector<Room>& rooms, 
    int width, int height) {
    
    std::vector<Intersection> intersections;
    int attempts = 0;
    
    while (intersections.size() < config.intersection_count && attempts < 100) {
        attempts++;
        int x = std::uniform_int_distribution<>(10, width - 10)(rng);
        int y = std::uniform_int_distribution<>(10, height - 10)(rng);
        
        bool far_enough = true;
        // Check distance from rooms
        for (const auto& r : rooms) {
            if (x >= r.x - 5 && x <= r.x + r.w + 5 && 
                y >= r.y - 5 && y <= r.y + r.h + 5) {
                far_enough = false; 
                break;
            }
        }
        
        if (far_enough) {
            intersections.push_back({x, y, config.intersection_size, 4, {}});
        }
    }
    return intersections;
}

void MapGenerator::placeIntersection(std::vector<std::vector<int>>& grid, const Intersection& inter) {
    int r = inter.size;
    for (int y = inter.centerY - r; y <= inter.centerY + r; ++y) {
        for (int x = inter.centerX - r; x <= inter.centerX + r; ++x) {
            createCorridorTile(grid, x, y, grid[0].size(), grid.size());
        }
    }
}

void MapGenerator::connectRoomsViaIntersections(
    std::vector<std::vector<int>>& grid, 
    const std::vector<Room>& rooms, 
    const std::vector<Intersection>& intersections, 
    const DungeonConfig& config, 
    int width, int height) {
    
    // Connect each room to nearest intersection
    for (const auto& room : rooms) {
        int bestDist = 999999;
        const Intersection* bestInter = nullptr;
        
        for (const auto& inter : intersections) {
            int d = std::abs(room.cx() - inter.centerX) + std::abs(room.cy() - inter.centerY);
            if (d < bestDist) {
                bestDist = d;
                bestInter = &inter;
            }
        }
        
        if (bestInter) {
            createSmartCorridor(grid, room.cx(), room.cy(), bestInter->centerX, bestInter->centerY, config, width, height);
        }
    }
    
    // Connect intersections to each other (Sequential for now, MST is better but complex)
    for (size_t i = 0; i < intersections.size() - 1; ++i) {
        createSmartCorridor(grid, intersections[i].centerX, intersections[i].centerY, 
                           intersections[i+1].centerX, intersections[i+1].centerY, config, width, height);
    }
}

void MapGenerator::generateCorridors(std::vector<std::vector<int>>& grid, 
                                    const std::vector<Room>& rooms, 
                                    const DungeonConfig& config, 
                                    int width, int height) {
    for (size_t i = 1; i < rooms.size(); ++i) {
        createSmartCorridor(grid, rooms[i-1].cx(), rooms[i-1].cy(), rooms[i].cx(), rooms[i].cy(), config, width, height);
    }
}

void MapGenerator::addDeadEnds(std::vector<std::vector<int>>& grid, const DungeonConfig& config, int width, int height) {
    int count = 10; // Approximate
    for (int i = 0; i < count; ++i) {
        int x = std::uniform_int_distribution<>(1, width - 2)(rng);
        int y = std::uniform_int_distribution<>(1, height - 2)(rng);
        
        if (grid[y][x] == 1) { // Start from existing floor
            int len = std::uniform_int_distribution<>(5, 15)(rng);
            int dir = std::uniform_int_distribution<>(0, 3)(rng);
            int dx = (dir == 0) ? 1 : (dir == 1 ? -1 : 0);
            int dy = (dir == 2) ? 1 : (dir == 3 ? -1 : 0);
            
            for (int s = 0; s < len; ++s) {
                x += dx; y += dy;
                if (x > 0 && x < width-1 && y > 0 && y < height-1) {
                    grid[y][x] = 1;
                }
            }
        }
    }
}
