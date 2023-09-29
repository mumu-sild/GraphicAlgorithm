#pragma once

#define _USE_MATH_DEFINES
#include <cmath>
#include <random>
#include <GLM/gtc/matrix_transform.hpp>
#include <GLM/gtc/type_ptr.hpp>
const float PI = (float)M_PI;

inline glm::vec2 sampleHammersley(glm::u32 i, glm::u32 n)
{
	glm::u32 bits = i;
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	float vdc = float(bits) * 2.3283064365386963e-10f;
	return glm::vec2(float(i) / float(n), vdc);
}

inline glm::vec3 sampleUniformSphere(float u, float v)
{
	float z = 1.0f - 2.0f * u;
	float r = sqrt(glm::max(0.0f, 1.0f - z * z));
	float phi = 2.0f * glm::pi<float>() * v;

	float sinPhi = sin(phi);
	float cosPhi = cos(phi);

	float x = r * cosPhi;
	float y = r * sinPhi;

	return glm::vec3(x, y, z);
}

struct CubeUV
{
	int index;
	float u, v;
};

inline glm::vec3 CubeUV2XYZ(const CubeUV& c)
{
	float u = c.u * 2.f - 1.f;
	float v = c.v * 2.f - 1.f;
	switch (c.index)
	{
	case 0: return { 1,  v, -u }; 	// +x
	case 1: return { -1,  v,  u }; 	// -x
	case 2: return { u,  1, -v };  // +y
	case 3: return { u, -1,  v };	// -y
	case 4: return { u,  v,  1 };  // +z
	case 5: return { -u,  v, -1 };	// -z
	}
	return glm::vec3();
}

/*
* 原理：
* 1. 计算平面上一点(x,y,1)投影到球面上后的坐标p'。
* 	p' = (x,y,1)/sqrt(x^2+y^2+1);
*
* 2. p'关于x方向和y方向的方向导数，两个方向导数叉乘的模长即为微面元的面积。
* 	得到dA = 1/[(x^2+y^2+1)^{3/2}]-----过程很复杂，结果很简单
*
* 3. 对微面元积分，求得（0,0,1）- (x,y,1)对应四边形的投影面积(立体角)，
*    然后利用这个公式就能求得一个像素的立体角。
*	 f(x,y) = tan^{-1}{xy/sqrt(x^2+y^2+1)} 
* 
* 总结：公式即为计算f(x,y) = tan^{-1}{xy/sqrt(x^2+y^2+1)} 的值
* 
* 疑问？：不会是负值吗？
*/
static double surfaceArea(double x, double y)
{
	//atan2(y,x)：返回（x,y）到(1,0)的夹角【弧度】
	return atan2(x * y, sqrt(x * x + y * y + 1.0));
}

