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
#include "simplex_noise.h"
#include <cmath>
#include <random>
#include <algorithm>

// Simplex grid skewing and unskewing constants
const double SimplexNoise::F2 = 0.5 * (sqrt(3.0) - 1.0);
const double SimplexNoise::G2 = (3.0 - sqrt(3.0)) / 6.0;

// Simplex grid traversal lookup table
// Used to determine simplex cell orientation in 2D
const int SimplexNoise::SIMPLEX[64][4] = {
	{0,1,2,3},{0,1,3,2},{0,0,0,0},{0,2,3,1},{0,0,0,0},{0,0,0,0},{0,0,0,0},{1,2,3,0},
	{0,2,1,3},{0,0,0,0},{0,3,1,2},{0,3,2,1},{0,0,0,0},{0,0,0,0},{0,0,0,0},{1,3,2,0},
	{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
	{1,2,0,3},{0,0,0,0},{1,3,0,2},{0,0,0,0},{0,0,0,0},{0,0,0,0},{2,3,0,1},{2,3,1,0},
	{1,0,2,3},{1,0,3,2},{0,0,0,0},{0,0,0,0},{0,0,0,0},{2,0,3,1},{0,0,0,0},{2,1,3,0},
	{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
	{2,0,1,3},{0,0,0,0},{0,0,0,0},{0,0,0,0},{3,0,1,2},{3,0,2,1},{0,0,0,0},{3,1,2,0},
	{2,1,0,3},{0,0,0,0},{0,0,0,0},{0,0,0,0},{3,1,0,2},{0,0,0,0},{3,2,0,1},{3,2,1,0}
};

SimplexNoise::SimplexNoise(unsigned int seed) {
	initializePermutation(seed);
}

void SimplexNoise::initializePermutation(unsigned int seed) {
	// Initialize permutation table with sequential values [0-255]
	for (int i = 0; i < 256; i++) {
		perm[i] = i;
	}

	// Shuffle using Mersenne Twister RNG (seeded)
	std::mt19937 rng(seed);
	for (int i = 255; i > 0; i--) {
		int j = rng() % (i + 1);
		std::swap(perm[i], perm[j]);
	}

	// Duplicate permutation table for wraparound (avoids modulo in noise())
	// Pre-compute modulo 12 for grad3 indexing
	for (int i = 0; i < 256; i++) {
		perm[256 + i] = perm[i];
		permMod12[i] = perm[i] % 12;
		permMod12[256 + i] = perm[i] % 12;
	}
}

int SimplexNoise::fastfloor(double x) {
	int xi = static_cast<int>(x);
	return x < xi ? xi - 1 : xi;
}

double SimplexNoise::dot(const int g[], double x, double y) {
	return g[0] * x + g[1] * y;
}

double SimplexNoise::noise(double xin, double yin) {
	// 3D gradient vectors projected to 2D (12 directions)
	static const int grad3[12][3] = {
		{1,1,0},{-1,1,0},{1,-1,0},{-1,-1,0},
		{1,0,1},{-1,0,1},{1,0,-1},{-1,0,-1},
		{0,1,1},{0,-1,1},{0,1,-1},{0,-1,-1}
	};

	double n0, n1, n2; // Noise contributions from the three simplex corners

	// Skew the input space to determine which simplex cell we're in
	double s = (xin + yin) * F2; // Hairy factor for 2D
	int i = fastfloor(xin + s);
	int j = fastfloor(yin + s);

	double t = (i + j) * G2;
	double X0 = i - t; // Unskew the cell origin back to (x,y) space
	double Y0 = j - t;
	double x0 = xin - X0; // The x,y distances from the cell origin
	double y0 = yin - Y0;

	// For the 2D case, the simplex shape is an equilateral triangle
	// Determine which simplex we are in
	int i1, j1; // Offsets for second (middle) corner of simplex in (i,j) coords
	if (x0 > y0) {
		i1 = 1; j1 = 0; // lower triangle, XY order: (0,0)->(1,0)->(1,1)
	} else {
		i1 = 0; j1 = 1; // upper triangle, YX order: (0,0)->(0,1)->(1,1)
	}

	// A step of (1,0) in (i,j) means a step of (1-c,-c) in (x,y), and
	// a step of (0,1) in (i,j) means a step of (-c,1-c) in (x,y), where
	// c = (3-sqrt(3))/6
	double x1 = x0 - i1 + G2; // Offsets for middle corner in (x,y) unskewed coords
	double y1 = y0 - j1 + G2;
	double x2 = x0 - 1.0 + 2.0 * G2; // Offsets for last corner in (x,y) unskewed coords
	double y2 = y0 - 1.0 + 2.0 * G2;

	// Work out the hashed gradient indices of the three simplex corners
	int ii = i & 255;
	int jj = j & 255;
	int gi0 = permMod12[ii + perm[jj]];
	int gi1 = permMod12[ii + i1 + perm[jj + j1]];
	int gi2 = permMod12[ii + 1 + perm[jj + 1]];

	// Calculate the contribution from the three corners
	double t0 = 0.5 - x0 * x0 - y0 * y0;
	if (t0 < 0) {
		n0 = 0.0;
	} else {
		t0 *= t0;
		n0 = t0 * t0 * dot(grad3[gi0], x0, y0); // (x,y) of grad3 used for 2D gradient
	}

	double t1 = 0.5 - x1 * x1 - y1 * y1;
	if (t1 < 0) {
		n1 = 0.0;
	} else {
		t1 *= t1;
		n1 = t1 * t1 * dot(grad3[gi1], x1, y1);
	}

	double t2 = 0.5 - x2 * x2 - y2 * y2;
	if (t2 < 0) {
		n2 = 0.0;
	} else {
		t2 *= t2;
		n2 = t2 * t2 * dot(grad3[gi2], x2, y2);
	}

	// Add contributions from each corner to get the final noise value
	// The result is scaled to return values in the interval [-1,1]
	return 70.0 * (n0 + n1 + n2);
}

double SimplexNoise::fractal(double x, double y, int octaves,
                            double persistence, double lacunarity) {
	double value = 0.0;
	double amplitude = 1.0;
	double frequency = 1.0;
	double maxValue = 0.0; // Used for normalizing result to [-1, 1]

	for (int i = 0; i < octaves; i++) {
		value += noise(x * frequency, y * frequency) * amplitude;

		maxValue += amplitude;

		amplitude *= persistence;
		frequency *= lacunarity;
	}

	// Normalize to [-1, 1] range
	return value / maxValue;
}
