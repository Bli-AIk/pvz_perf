#pragma once

// Approximate float trigonometry. The implementation folds arbitrary angles
// internally; do not use it where bit-identical CRT results are required.
float FastSin(float theRadians);
float FastCos(float theRadians);
// Share one angle reduction; cosine's polynomial argument stays in [-pi/2,pi/2].
void FastSinCos(float theRadians, float& sine, float& cosine);
