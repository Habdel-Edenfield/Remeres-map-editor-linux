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

#ifndef RME_SIMPLEX_NOISE_H_
#define RME_SIMPLEX_NOISE_H_

/**
 * SimplexNoise - 2D coherent noise generator
 *
 * Implementation of Ken Perlin's Simplex Noise algorithm (2001).
 * Provides smooth, continuous noise for procedural terrain generation.
 *
 * Algorithm characteristics:
 * - O(1) time complexity per sample
 * - Gradient-based (no grid artifacts)
 * - Deterministic (same seed → same output)
 * - Output range: [-1.0, 1.0]
 *
 * Reference: "Simplex Noise Demystified" by Stefan Gustavson (2005)
 * Original implementation from TFS Custom Editor (otmapgen.cpp)
 */
class SimplexNoise {
public:
	/**
	 * Constructor
	 * @param seed Random seed for permutation table initialization
	 */
	explicit SimplexNoise(unsigned int seed = 0);

	/**
	 * Generate 2D simplex noise value
	 * @param x X coordinate in noise space
	 * @param y Y coordinate in noise space
	 * @return Noise value in range [-1.0, 1.0]
	 */
	double noise(double x, double y);

	/**
	 * Generate fractal noise using multiple octaves (Fractal Brownian Motion)
	 * @param x X coordinate in noise space
	 * @param y Y coordinate in noise space
	 * @param octaves Number of noise layers (typically 4-8)
	 * @param persistence Amplitude decay per octave (typically 0.5)
	 * @param lacunarity Frequency multiplier per octave (typically 2.0)
	 * @return Combined noise value (normalized by total weight)
	 */
	double fractal(double x, double y, int octaves = 4,
	               double persistence = 0.5, double lacunarity = 2.0);

private:
	// Permutation table for gradient selection
	int perm[512];        // Duplicated for wraparound [0-255, 0-255]
	int permMod12[512];   // Pre-computed modulo 12 for grad3 indexing

	// Simplex grid constants
	static const int SIMPLEX[64][4];  // Simplex traversal lookup table
	static const double F2;            // Skewing factor: 0.5 * (sqrt(3.0) - 1.0)
	static const double G2;            // Unskewing factor: (3.0 - sqrt(3.0)) / 6.0

	/**
	 * Initialize permutation table with seeded shuffle
	 * @param seed Random seed for Mersenne Twister RNG
	 */
	void initializePermutation(unsigned int seed);

	/**
	 * Compute dot product of gradient vector and distance vector
	 * @param g Gradient vector (from grad3 table)
	 * @param x X component of distance
	 * @param y Y component of distance
	 * @return Dot product result
	 */
	double dot(const int g[], double x, double y);

	/**
	 * Fast floor function for integer casting
	 * @param x Input value
	 * @return Floored integer
	 */
	int fastfloor(double x);
};

#endif // RME_SIMPLEX_NOISE_H_
