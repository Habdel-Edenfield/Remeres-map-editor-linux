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

#ifndef RME_MAP_GENERATOR_H_
#define RME_MAP_GENERATOR_H_

#include "main.h"
#include "simplex_noise.h"
#include <string>
#include <vector>
#include <functional>
#include <random>

// Forward declarations
class Map;
class Tile;

/**
 * IslandConfig - Configuration for island terrain generation
 *
 * This structure contains all parameters needed to generate an island-shaped
 * terrain using Simplex Noise with radial falloff.
 */
struct IslandConfig {
	// ═══ Noise Parameters ═══
	// Control the base noise pattern before island mask is applied

	double noise_scale = 0.01;        // Zoom level (lower = larger features)
	int noise_octaves = 4;            // Number of detail layers (4-8 typical)
	double noise_persistence = 0.5;   // Amplitude decay per octave (roughness)
	double noise_lacunarity = 2.0;    // Frequency multiplier per octave

	// ═══ Island Shape Parameters ═══
	// Control the radial falloff that creates the island shape

	double island_size = 0.8;         // Island radius (0.0-1.0, 1.0 = fills entire map)
	double island_falloff = 2.0;      // Sharpness of coastline (higher = sharper)
	double island_threshold = 0.3;    // Water/land cutoff (-1.0 to 1.0)

	// ═══ Tile IDs ═══
	// Tibia item IDs for terrain tiles

	uint16_t water_id = 4608;         // Water tile (default: sea water)
	uint16_t ground_id = 4526;        // Ground tile (default: grass)

	// ═══ Post-Processing Cleanup ═══
	// Remove artifacts and smooth terrain

	bool enable_cleanup = true;           // Enable all cleanup steps
	int min_land_patch_size = 4;         // Remove land patches smaller than N tiles
	int max_water_hole_size = 3;         // Fill water holes smaller than N tiles
	int smoothing_passes = 2;            // Number of smoothing iterations

	// ═══ Map Placement ═══
	// Target floor for generated terrain

	int target_floor = 7;                // Z-level (7 = ground level in Tibia)
};

/**
 * DungeonConfig - Configuration for dungeon generation
 */
struct DungeonConfig {
	// General
	int target_floor = 7;
	uint16_t wall_id = 1030; // Stone wall
	uint16_t floor_id = 406; // Stone floor
	
	// Rooms
	int room_count = 15;
	int min_room_size = 5;
	int max_room_size = 12;
	
	// Corridors
	int corridor_width = 2; // 1-3 typical
	
	// Caves (Natural aspect)
	bool generate_caves = true;
	double cave_scale = 0.05;
	double cave_threshold = 0.4; // 0.0 to 1.0 (empty space)
	
	// Cleanup
	bool enable_cleanup = true;

	// Advanced Layout
	bool connect_all_rooms = true;
	bool add_dead_ends = true;
	bool use_smart_pathfinding = true; // Use A*
	bool add_intersections = true;     // Add hubs
	int intersection_count = 5;
	int intersection_size = 2;
};

/**
 * MapGenerator - Procedural terrain generation engine
 */
class MapGenerator {
public:
	typedef std::function<bool(int current, int total)> ProgressCallback;

	MapGenerator();
	~MapGenerator();

	bool generateIslandMap(Map* map, const IslandConfig& config, 
			              int width, int height, const std::string& seed,
			              int startX, int startY);

	bool generateDungeonMap(Map* map, const DungeonConfig& config,
						   int width, int height, const std::string& seed,
						   int startX, int startY);

	void setProgressCallback(ProgressCallback callback);

private:
	// ═══ Core Components ═══
	SimplexNoise* noise;
	ProgressCallback progress_callback;
	std::mt19937 rng;

	// ═══ Data Structures ═══
	struct Room {
		int x, y, w, h;
		int cx() const { return x + w / 2; }
		int cy() const { return y + h / 2; }
		bool intersects(const Room& other) const {
			return (x <= other.x + other.w && x + w >= other.x &&
					y <= other.y + other.h && y + h >= other.y);
		}
	};

	struct Intersection {
		int centerX, centerY;
		int size;
		int connectionCount;
		std::vector<std::pair<int, int>> connections;
	};

	// ═══ Dungeon Generation Steps ═══
	std::vector<Room> generateRooms(const DungeonConfig& config, int width, int height);
	
	void generateCorridors(std::vector<std::vector<int>>& grid, 
	                      const std::vector<Room>& rooms, 
	                      const DungeonConfig& config, 
	                      int width, int height);

	// Smart Corridors (A*)
	void createSmartCorridor(std::vector<std::vector<int>>& grid, 
	                        int x1, int y1, int x2, int y2, 
	                        const DungeonConfig& config, 
	                        int width, int height);
	
	std::vector<std::pair<int, int>> findShortestPath(const std::vector<std::vector<int>>& grid, 
	                                                 int x1, int y1, int x2, int y2, 
	                                                 int width, int height);

	// Intersections
	std::vector<Intersection> generateIntersections(const DungeonConfig& config, 
	                                              const std::vector<Room>& rooms, 
	                                              int width, int height);
	void placeIntersection(std::vector<std::vector<int>>& grid, 
	                      const Intersection& intersection);
	void connectRoomsViaIntersections(std::vector<std::vector<int>>& grid, 
	                                 const std::vector<Room>& rooms, 
	                                 const std::vector<Intersection>& intersections, 
	                                 const DungeonConfig& config, 
	                                 int width, int height);

	// Helpers
	void createCorridorTile(std::vector<std::vector<int>>& grid, int x, int y, int width, int height);
	void createImprovedCorridor(std::vector<std::vector<int>>& grid, int x1, int y1, int x2, int y2, 
	                           const DungeonConfig& config, int width, int height);
	void addDeadEnds(std::vector<std::vector<int>>& grid, const DungeonConfig& config, int width, int height);

	bool isWallPosition(const std::vector<std::vector<int>>& grid, int x, int y, int width, int height);

	// ═══ Generation Steps ═══

	/**
	 * Generate base height map using Simplex Noise
	 *
	 * Creates a 2D array of noise values [-1, 1] using fractal Brownian motion.
	 * Does NOT apply island mask yet.
	 *
	 * @param config Noise parameters (scale, octaves, persistence, lacunarity)
	 * @param width Height map width
	 * @param height Height map height
	 * @return 2D vector of height values [-1, 1]
	 */
	std::vector<std::vector<double>> generateHeightMap(
		const IslandConfig& config, int width, int height);

	void applyIslandMask(std::vector<std::vector<double>>& heightMap,
	                    const IslandConfig& config, int width, int height);

	/**
	 * Place tiles on map based on height map
	 *
	 * For each point in height map:
	 *   if height < threshold: place water_id
	 *   else: place ground_id
	 *
	 * All tiles are placed at config.target_floor (default z=7).
	 *
	 * @param map Target map
	 * @param heightMap Generated height values
	 * @param config Tile IDs and threshold
	 * @param width Map width
	 * @param height Map height
	 */
	void placeTiles(Map* map, const std::vector<std::vector<double>>& heightMap,
	               const IslandConfig& config, int width, int height,
	               int offsetX, int offsetY);

	// ═══ Post-Processing ═══

	/**
	 * Main cleanup function - runs all enabled cleanup steps
	 *
	 * 1. Remove small land patches (isolated islands)
	 * 2. Fill small water holes (lakes)
	 * 3. Smooth coastline (reduce jaggedness)
	 *
	 * @param map Map with placed tiles
	 * @param config Cleanup parameters
	 * @param width Map width
	 * @param height Map height
	 * @param offsetX Map offset X
	 * @param offsetY Map offset Y
	 */
	void cleanupTerrain(Map* map, const IslandConfig& config,
	                   int width, int height,
	                   int offsetX, int offsetY);

	/**
	 * Remove disconnected patches smaller than min_size
	 *
	 * Uses flood fill to identify connected regions of tile_id.
	 * Patches with < min_size tiles are replaced with opposite tile type.
	 *
	 * @param map Target map
	 * @param tile_id ID of tiles to check (e.g., ground_id for land patches)
	 * @param min_size Minimum patch size to keep
	 * @param width Map width
	 * @param height Map height
	 * @param offsetX Map offset X
	 * @param offsetY Map offset Y
	 */
	void removeSmallPatches(Map* map, uint16_t tile_id, int min_size,
	                       int width, int height,
	                       int offsetX, int offsetY);

	/**
	 * Fill enclosed holes smaller than max_size
	 *
	 * Similar to removeSmallPatches but for filling enclosed water/land areas.
	 *
	 * @param map Target map
	 * @param fill_id ID to replace holes with
	 * @param max_size Maximum hole size to fill
	 * @param width Map width
	 * @param height Map height
	 * @param offsetX Map offset X
	 * @param offsetY Map offset Y
	 */
	void fillSmallHoles(Map* map, uint16_t target_id, uint16_t fill_id,
	                   int max_size, int width, int height,
	                   int offsetX, int offsetY);

	/**
	 * Smooth coastline using neighbor majority voting
	 *
	 * For each tile, count neighbors of same type. If minority, convert to
	 * majority type. Reduces jagged edges and single-tile protrusions.
	 *
	 * @param map Target map
	 * @param config Tile IDs and smoothing passes
	 * @param width Map width
	 * @param height Map height
	 * @param offsetX Map offset X
	 * @param offsetY Map offset Y
	 */
	void smoothCoastline(Map* map, const IslandConfig& config,
	                    int width, int height,
	                    int offsetX, int offsetY);

	// ═══ Utility Functions ═══

	/**
	 * Flood fill to count connected region size
	 *
	 * Iterative flood fill starting from (x, y). Counts all connected tiles
	 * of target_id, optionally replacing them with replacement_id.
	 *
	 * @param map Target map
	 * @param x Starting X coordinate
	 * @param y Starting Y coordinate
	 * @param z Floor level
	 * @param width Map width
	 * @param height Map height
	 * @param target_id ID to search for
	 * @param replacement_id ID to replace with (0 = no replacement)
	 * @return Number of tiles in connected region
	 */
	int floodFillCount(Map* map, int x, int y, int z,
	                  int width, int height,
	                  uint16_t target_id, uint16_t replacement_id,
	                  int offsetX, int offsetY);

	/**
	 * Seed random number generator from string
	 *
	 * Tries to parse seed as numeric value, falls back to hashing string.
	 *
	 * @param seed Seed string
	 */
	void seedRandom(const std::string& seed);

	/**
	 * Calculate Euclidean distance from center
	 *
	 * @param x X coordinate
	 * @param y Y coordinate
	 * @param centerX Center X
	 * @param centerY Center Y
	 * @return Distance from center
	 */
	double getDistance(int x, int y, int centerX, int centerY);

	/**
	 * Apply power-based falloff curve
	 *
	 * Creates smooth or sharp falloff depending on exponent.
	 *
	 * @param distance Normalized distance [0, 1]
	 * @param falloff Falloff exponent (higher = sharper)
	 * @return Falloff value [0, 1]
	 */
	double applyFalloff(double distance, double falloff);
};

#endif // RME_MAP_GENERATOR_H_
