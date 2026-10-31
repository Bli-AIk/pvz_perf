#include "FastMath.h"
#include <cmath>

namespace {
float SinPolynomial(float x)
{
	const float x2 = x * x;
	return x * (0.999999707f + x2 * (-0.166665772f + x2 * (0.008332558f +
		x2 * (-0.000198125755f + x2 * (2.70405121e-06f + x2 * -2.05342445e-08f)))));
}
}

// 实现自 Sexy.TodLib/TodCommon.cpp 逐句迁移（2026-10-07 下放共享层）。
float FastSin(float theRadians)
{
	// x*(1/2π)+0.5 的小数折叠到 [-π,π)。必须用地板取整（负角沿 -∞ 方向）——
	// 2026-10-05 B 树实机修复：int 截断向零取整，负角（x<-π 时 t<0）折叠到
	// 错半圈，最大误差 1.16（=2×振幅，选卡僵尸预览整体翻转）。
	// 保留 floorf 的向下取整语义，不能替换成整数截断。
	float t = theRadians * 0.15915494f + 0.5f;
	t -= floorf(t);
	theRadians = (t - 0.5f) * 6.2831853f;
	return SinPolynomial(theRadians);
}
float FastCos(float theRadians)
{
	// cos(x)=sin(x+π/2)：同一条多项式，域折叠照常
	return FastSin(theRadians + 1.57079633f);
}

void FastSinCos(float theRadians, float& sine, float& cosine)
{
    float phase = theRadians * 0.15915494f + 0.5f;
    phase -= floorf(phase);
    const float x = (phase - 0.5f) * 6.2831853f;
    sine = SinPolynomial(x);
    cosine = SinPolynomial(1.57079633f - std::abs(x));
}
