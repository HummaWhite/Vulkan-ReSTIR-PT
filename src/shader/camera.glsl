#ifndef CAMERA_GLSL
#define CAMERA_GLSL

#include "math.glsl"

const uint CameraClearFlag = 0x80000000;
const uint CameraFrameIndexMask = 0x7fffffff;

struct CameraPdf {
	float pdfPos;
	float pdfDir;
};

CameraPdf makeCameraPdf(float pdfPos, float pdfDir) {
	CameraPdf ret;
	ret.pdfPos = pdfPos;
	ret.pdfDir = pdfDir;
	return ret;
}

Ray makeRay(vec3 ori, vec3 dir) {
	Ray ret;
	ret.ori = ori;
	ret.dir = dir;
	return ret;
}

Ray pinholeCameraSampleRay(Camera cam, vec2 uv, vec2 r) {
	vec2 texelSize = 1.0 / vec2(cam.filmSize);
	vec2 biasedCoord = uv + texelSize * r.xy;
	vec2 ndc = biasedCoord * 2.0 - 1.0;
	float aspect = float(cam.filmSize.x) / float(cam.filmSize.y);
	float tanFOV = tan(radians(cam.FOV * 0.5));

	vec3 pFocusPlane = vec3(ndc * vec2(aspect, 1.0) * tanFOV, 1.0);

	vec3 ori = cam.pos;
	vec3 dir = normalize(pFocusPlane);
	dir = normalize(cam.right * dir.x + cam.up * dir.y + cam.front * dir.z);

	return Ray(ori, dir);
}

Ray thinLensCameraSampleRay(Camera cam, vec2 uv, vec4 r) {
	vec2 texelSize = 1.0 / vec2(cam.filmSize);
	vec2 biasedCoord = uv + texelSize * r.xy;
	vec2 ndc = biasedCoord * 2.0 - 1.0;
	float aspect = float(cam.filmSize.x) / float(cam.filmSize.y);
	float tanFOV = tan(radians(cam.FOV * 0.5));

	vec3 pLens = vec3(toConcentricDisk(r.zw) * cam.lensRadius, 0.0);
	vec3 pFocusPlane = vec3(ndc * vec2(aspect, 1.0) * tanFOV, 1.0) * cam.focalDist;

	vec3 ori = cam.pos + cam.right * pLens.x + cam.up * pLens.y;
	vec3 dir = pFocusPlane - pLens;
	dir = normalize(cam.right * dir.x + cam.up * dir.y + cam.front * dir.z);

	return Ray(ori, dir);
}

/*
struct CameraIiSample {
	vec3 wi;
	vec3 Ii;
	float dist;
	vec2 uv;
	float pdf;
};

CameraIiSample makeCameraIiSample(vec3 wi, vec3 Ii, float dist, vec2 uv, float pdf) {
	CameraIiSample ret;
	ret.wi = wi;
	ret.Ii = Ii;
	ret.dist = dist;
	ret.uv = uv;
	ret.pdf = pdf;
	return ret;
}

const CameraIiSample InvalidIiSample = makeCameraIiSample(vec3(0.0), vec3(0.0), 0.0, vec2(0.0), 0);

bool inFilmBound(vec2 uv) {
	return uv.x >= 0 && uv.x <= 1.0 && uv.y >= 0 && uv.y <= 1.0;
}

bool thinLensCameraIsApertureDelta(Camera cam) {
	return cam.lensRadius <= 1e-6;
}

vec2 thinLensCameraGetRasterPos(Camera cam, Ray ray) {
	float cosTheta = dot(ray.dir, cam.front);
	float dFocus = uFocalDist / cosTheta;
	vec3 pFocus = cam.MatInv * (rayPoint(ray, dFocus) - cam.Pos);

	vec2 filmSize = vec2(uFilmSize);
	float aspect = filmSize.x / filmSize.y;

	pFocus /= vec3(vec2(aspect, 1.0) * uTanFOV, 1.0) * uFocalDist;
	vec2 ndc = pFocus.xy;
	return (ndc + 1.0) * 0.5;
}

vec3 thinLensCameraIe(Camera cam, Ray ray) {
	float cosTheta = dot(ray.dir, cam.front);

	if (cosTheta < 1e-6) {
		return vec3(0.0);
	}
	vec2 pRaster = thinLensCameraRasterPos(ray);

	if (!inFilmBound(pRaster)) {
		return vec3(0.0);
	}

	float tanFOVInv = 1.0 / uTanFOV;
	float cos2Theta = cosTheta * cosTheta;
	float lensArea = thinLensCameraDelta() ? 1.0 : Pi * uLensRadius * uLensRadius;
	return vec3(0.25) * square(tanFOVInv / cos2Theta) / (lensArea * cam.Asp);
}

CameraIiSample thinLensCameraSampleIi(vec3 ref, vec2 u) {
	vec3 pLens = vec3(toConcentricDisk(u) * uLensRadius, 0.0);
	vec3 y = cam.Pos + cam.R * pLens.x + cam.U * pLens.y + cam.F * pLens.z;
	float dist = distance(ref, y);

	vec3 wi = normalize(y - ref);
	float cosTheta = satDot(cam.F, -wi);

	if (cosTheta < 1e-6) {
		return InvalidIiSample;
	}

	Ray ray = makeRay(y, -wi);
	vec3 Ie = thinLensCameraIe(ray);
	vec2 uv = thinLensCameraRasterPos(ray);
	float lensArea = thinLensCameraDelta() ? 1.0 : Pi * uLensRadius * uLensRadius;
	float pdf = dist * dist / (cosTheta * lensArea);

	return makeCameraIiSample(wi, Ie, dist, uv, pdf);
}

CameraPdf thinLensCameraPdfIe(Ray ray) {
	float cosTheta = dot(cam.F, ray.dir);

	if (cosTheta < 1e-6) {
		return makeCameraPdf(0.0, 0.0);
	}
	vec2 pRaster = thinLensCameraRasterPos(ray);

	if (!inFilmBound(pRaster)) {
		return makeCameraPdf(0.0, 0.0);
	}

	float pdfPos = thinLensCameraDelta() ? 1.0 : 1.0 / (Pi * uLensRadius * uLensRadius);
	float pdfDir = 1.0 / (cosTheta * cosTheta * cosTheta);
	return makeCameraPdf(pdfPos, pdfDir);
}
*/

#endif