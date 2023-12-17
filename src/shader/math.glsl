#ifndef MATH_GLSL
#define MATH_GLSL

const float Pi = 3.14159265358979323846;
const float PiInv = 1.0 / Pi;
const float Inf = 1.0 / 0.0;
const vec3 Black = vec3(0.0);

float sqr(float x) {
	return x * x;
}

float powerHeuristic(float pf, int fn, float pg, int gn) {
	float f = pf * float(fn);
	float g = pg * float(gn);
	return f * f / (f * f + g * g);
}

float powerHeuristic(float f, float g) {
	return f * f / (f * f + g * g);
}

vec2 toConcentricDisk(vec2 v) {
	if (v.x == 0.0 && v.y == 0.0) {
		return vec2(0.0, 0.0);
	}
	v = v * 2.0 - 1.0;
	float phi, r;

	if (v.x * v.x > v.y * v.y) {
		r = v.x;
		phi = Pi * v.y / v.x * 0.25;
	}
	else {
		r = v.y;
		phi = Pi * 0.5 - Pi * v.x / v.y * 0.25;
	}
	return vec2(r * cos(phi), r * sin(phi));
}

float satDot(vec3 a, vec3 b) {
	return max(dot(a, b), 0.0);
}

float absDot(vec3 a, vec3 b) {
	return abs(dot(a, b));
}

float distSqr(vec3 x, vec3 y) {
	return dot(x - y, x - y);
}

vec2 sphereToPlane(vec3 uv) {
	float theta = atan(uv.y, uv.x);

	if (theta < 0.0) {
		theta += Pi * 2.0;
	}
	float phi = atan(length(uv.xy), uv.z);
	return vec2(theta * PiInv * 0.5, phi * PiInv);
}

vec3 planeToSphere(vec2 uv) {
	float theta = uv.x * Pi * 2.0;
	float phi = uv.y * Pi;
	return vec3(cos(theta) * sin(phi), sin(theta) * sin(phi), cos(phi));
}

vec3 getTangent(vec3 n) {
	return (abs(n.z) > 0.999) ? vec3(0.0, 1.0, 0.0) : vec3(0.0, 0.0, 1.0);
}

mat3 matLocalToWorld(vec3 n) {
	vec3 t = getTangent(n);
	vec3 b = normalize(cross(n, t));
	t = cross(b, n);
	return mat3(t, b, n);
}

vec3 localToWorld(vec3 n, vec3 v)
{
	return normalize(matLocalToWorld(n) * v);
}

vec3 sampleCosineWeightedHemisphere(vec3 n, vec2 u) {
	vec2 uv = toConcentricDisk(u);
	float z = sqrt(1.0 - dot(uv, uv));
	return localToWorld(n, vec3(uv, z));
}

bool sameHemisphere(vec3 n, vec3 a, vec3 b) {
	return dot(n, a) * dot(n, b) > 0;
}

int maxExtent(vec3 v) {
	if (v.x > v.y) {
		return v.x > v.z ? 0 : 2;
	}
	else {
		return v.y > v.z ? 1 : 2;
	}
}

float maxComponent(vec3 v) {
	return max(v.x, max(v.y, v.z));
}

float angleBetween(vec3 x, vec3 y) {
	if (dot(x, y) < 0.0) {
		return Pi - 2.0 * asin(length(x + y) * 0.5);
	}
	else {
		return 2.0 * asin(length(y - x) * 0.5);
	}
}

vec3 sampleTriangleUniform(vec3 va, vec3 vb, vec3 vc, vec2 uv) {
	float r = sqrt(uv.y);
	float u = 1.0 - r;
	float v = uv.x * r;
	return va * (1.0 - u - v) + vb * u + vc * v;
}

float triangleArea(vec3 va, vec3 vb, vec3 vc) {
	return 0.5f * length(cross(vc - va, vb - va));
}

float triangleSphericalArea(vec3 a, vec3 b, vec3 c) {
	vec3 ab = cross(a, b);
	vec3 bc = cross(b, c);
	vec3 ca = cross(c, a);

	if (dot(ab, ab) == 0.0 || dot(bc, bc) == 0.0 || dot(ca, ca) == 0.0) {
		return 0.0;
	}
	ab = normalize(ab);
	bc = normalize(bc);
	ca = normalize(ca);

	float alpha = angleBetween(ab, -ca);
	float beta = angleBetween(bc, -ab);
	float gamma = angleBetween(ca, -bc);

	return abs(alpha + beta + gamma - Pi);
}

float triangleSolidAngle(vec3 a, vec3 b, vec3 c) {
	return triangleSphericalArea(a, b, c);
}

float triangleSolidAngle(vec3 va, vec3 vb, vec3 vc, vec3 p) {
	return triangleSolidAngle(normalize(va - p), normalize(vb - p), normalize(vc - p));
}

vec3 rotateZ(vec3 v, float angle) {
	float cost = cos(angle);
	float sint = sin(angle);
	return vec3(vec2(v.x * cost - v.y * sint, v.x * sint + v.y * cost), v.z);
}

float pow5(float x) {
	float x2 = x * x;
	return x2 * x2 * x;
}

float luminance(vec3 color) {
	return dot(color, vec3(0.299, 0.587, 0.114));
}

bool isBlack(vec3 color) {
	return luminance(color) < 1e-5f;
}

bool hasNan(vec3 color) {
	return isnan(color.x) || isnan(color.y) || isnan(color.z);
}

bool hasInf(vec3 color) {
	return isinf(color.x) || isinf(color.y) || isinf(color.z);
}

vec3 clampColor(vec3 color) {
	if (hasNan(color)) {
		return vec3(0.0);
	}
	return clamp(color, vec3(0.0), vec3(1e4));
}

vec3 colorWheel(float x) {
	const float Div = 1.0 / 4.0;

	if (isnan(x)) {
		return vec3(1.0);
	} if (x < Div) {
		return vec3(0.0, x / Div, 1.0);
	}
	else if (x < Div * 2) {
		return vec3(0.0, 1.0, 2.0 - x / Div);
	}
	else if (x < Div * 3) {
		return vec3(x / Div - 2.0, 1.0, 0.0);
	}
	else {
		return vec3(1.0, 4.0 - x / Div, 0.0);
	}
}

uint hash(uint a) {
	a = (a + 0x7ed55d16u) + (a << 12u);
	a = (a ^ 0xc761c23cu) ^ (a >> 19u);
	a = (a + 0x165667b1u) + (a << 5u);
	a = (a + 0xd3a2646cu) ^ (a << 9u);
	a = (a + 0xfd7046c5u) + (a << 3u);
	a = (a ^ 0xb55a4f09u) ^ (a >> 16u);
	return a;
}

uint hash2(uint seed) {
	seed = (seed ^ 61u) ^ (seed >> 16u);
	seed *= 9u;
	seed = seed ^ (seed >> 4u);
	seed *= 0x27d4eb2du;
	seed = seed ^ (seed >> 15u);
	return seed;
}

uint makeSeed(uint seed0, uint seed1, uint index) {
	return hash2((1u << 31u) | (seed1 << 22) | seed0) ^ hash2(index);
}

uint makeSeed(uint rand, uint index) {
	return hash2(rand) + hash2(index);
}

uint urand(inout uint rng) {
	return rng = hash2(rng);
}

float sample1f(inout uint rng) {
	return float(urand(rng)) / 4294967295.0;
}

vec2 sample2f(inout uint rng) {
	return vec2(sample1f(rng), sample1f(rng));
}

vec3 sample3f(inout uint rng) {
	return vec3(sample2f(rng), sample1f(rng));
}

vec4 sample4f(inout uint rng) {
	return vec4(sample2f(rng), sample2f(rng));
}

float MISWeight(float f, float g) {
	return (f * f) / (f * f + g * g);
}

uint ceilDiv(uint x, uint y) {
	return (x + y - 1) / y;
}

int ceilDiv(int x, int y) {
	return (x + y - 1) / y;
}

float square(float x) {
	return x * x;
}

float distSquare(vec3 a, vec3 b) {
	return dot(a - b, a - b);
}

#endif