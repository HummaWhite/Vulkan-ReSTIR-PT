#ifndef MATERIAL_GLSL
#define MATERIAL_GLSL

#include "math.glsl"

const uint Diffuse = 1 << 0;
const uint Glossy = 1 << 1;
const uint Specular = 1 << 2;
const uint Reflection = 1 << 4;
const uint Transmission = 1 << 5;
const uint InvalidBSDFSample = 0x80000000;

const uint Principled = 0;
const uint Lambert = 1;
const uint MetallicWorkflow = 2;
const uint Metal = 3;
const uint Dielectric = 4;
const uint ThinDielectric = 5;
const uint Fake = 6;
const uint Light = 7;

#define MATERIAL_DIELECTRIC_USE_SCHLICK_APPROX true

struct BSDFSample {
	vec3 wi;
	float pdf;
	vec3 bsdf;
	uint type;
};

float fresnelSchlick(float cosTheta, float ior) {
    float f0 = abs(1.0 - ior) / (1.0 + ior);
    return mix(f0, 1.0, pow5(1.0 - cosTheta));
}

vec3 fresnelSchlick(float cosTheta, vec3 f0) {
    return mix(f0, vec3(1.0), pow5(1.0 - cosTheta));
}

float fresnel(float cosIn, float ior) {
#if MATERIAL_DIELECTRIC_USE_SCHLICK_APPROX
    return fresnelSchlick(cosIn, ior);
#else
    if (cosIn < 0) {
        ior = 1.0 / ior;
        cosIn = -cosIn;
    }
    float sinIn = sqrt(1.0 - cosIn * cosIn);
    float sinTr = sinIn / ior;
    if (sinTr >= 1.0) {
        return 1.0;
    }
    float cosTr = sqrt(1.0 - sinTr * sinTr);
    return (square((cosIn - ior * cosTr) / (cosIn + ior * cosTr)) +
        square((ior * cosIn - cosTr) / (ior * cosIn + cosTr))) * 0.5;
#endif
}

float schlickG(float cosTheta, float alpha) {
    float a = alpha * 0.5;
    return cosTheta / (cosTheta * (1.0 - a) + a);
}

float smithG(float cosWo, float cosWi, float alpha) {
    return schlickG(abs(cosWo), alpha) * schlickG(abs(cosWi), alpha);
}

float GTR2Distrib(float cosTheta, float alpha) {
    if (cosTheta < 1e-6) {
        return 0.0;
    }
    float aa = alpha * alpha;
    float nom = aa;
    float denom = cosTheta * cosTheta * (aa - 1.0) + 1.0;
    denom = denom * denom * Pi;
    return nom / denom;
}

float GTR2Pdf(vec3 n, vec3 m, vec3 wo, float alpha) {
    return GTR2Distrib(dot(n, m), alpha) * schlickG(dot(n, wo), alpha) *
        absDot(m, wo) / absDot(n, wo);
}

vec3 GTR2Sample(vec3 n, vec3 wo, float alpha, vec2 r) {
    mat3 transMat = matLocalToWorld(n);
    mat3 transInv = inverse(transMat);

    vec3 vh = normalize((transInv * wo) * vec3(alpha, alpha, 1.0));

    float lenSq = vh.x * vh.x + vh.y * vh.y;
    vec3 t = lenSq > 0.0 ? vec3(-vh.y, vh.x, 0.0) / sqrt(lenSq) : vec3(1.0, 0.0, 0.0);
    vec3 b = cross(vh, t);

    vec2 p = toConcentricDisk(r);
    float s = 0.5 * (vh.z + 1.0);
    p.y = (1.0 - s) * sqrt(1.0 - p.x * p.x) + s * p.y;

    vec3 wh = t * p.x + b * p.y + vh * sqrt(max(0.0, 1.0 - dot(p, p)));
    wh = vec3(wh.x * alpha, wh.y * alpha, max(0.0, wh.z));
    return normalize(transMat * wh);
}

bool isGTR2Connectible(float roughness) {
    return roughness > 0.1;
}

bool isGTR2Delta(float roughness) {
    return roughness < 0.01;
}

bool refract(vec3 n, vec3 wi, float ior, out vec3 wt) {
    float cosIn = dot(n, wi);
    if (cosIn < 0) {
        ior = 1.0 / ior;
    }
    float sin2In = max(0.0, 1.0 - cosIn * cosIn);
    float sin2Tr = sin2In / (ior * ior);

    if (sin2Tr >= 1.0) {
        return false;
    }
    float cosTr = sqrt(1.0 - sin2Tr);
    if (cosIn < 0) {
        cosTr = -cosTr;
    }
    wt = normalize(-wi / ior + n * (cosIn / ior - cosTr));
    return true;
}

vec3 lambertBSDF(vec3 albedo, vec3 n, vec3 wi) {
	return albedo * PiInv;
}

float lambertPdf(vec3 n, vec3 wi) {
	return absDot(n, wi) * PiInv;
}

bool lambertSampleBSDF(vec3 albedo, vec3 n, vec2 r, out BSDFSample s) {
	s.wi = sampleCosineWeightedHemisphere(n, r);
	s.pdf = absDot(n, s.wi) * PiInv;
	s.bsdf = albedo * PiInv;
	s.type = Diffuse | Reflection;
	return true;
}

bool dielectricSampleBSDF(Material mat, vec3 albedo, vec3 n, vec3 wo, vec3 r, out BSDFSample s) {
    float pdfReflect = fresnel(dot(n, wo), mat.ior);
    s.bsdf = albedo;

    if (r.z < pdfReflect) {
        s.wi = reflect(-wo, n);
        s.type = Specular | Reflection;
        s.pdf = 1.0;
    }
    else {
        if (!refract(n, wo, mat.ior, s.wi)) {
            s.type = InvalidBSDFSample;
            return false;
        }
        if (dot(n, wo) < 0) {
            mat.ior = 1.0 / mat.ior;
        }
        s.bsdf /= mat.ior * mat.ior;
        s.type = Specular | Transmission;
        s.pdf = 1.0;
    }
    return true;
}

vec3 metallicWorkflowBSDF(Material mat, vec3 albedo, vec3 n, vec3 wo, vec3 wi) {
    float alpha = square(mat.roughness);
    vec3 wh = normalize(wo + wi);

    float cosO = dot(n, wo);
    float cosI = dot(n, wi);

    if (cosI * cosO < 1e-7) {
        return vec3(0.0);
    }

    vec3 f = fresnelSchlick(dot(wh, wo), mix(vec3(0.08), albedo, mat.metallic));
    float g = smithG(cosO, cosI, alpha);
    float d = GTR2Distrib(dot(n, wh), alpha);

    return mix(albedo * PiInv * (1.0 - mat.metallic), vec3(g * d / (4.0 * cosI * cosO)), f);
}

float metallicWorkflowPdf(Material mat, vec3 n, vec3 wo, vec3 wi) {
    vec3 wh = normalize(wo + wi);

    return mix(
        satDot(n, wi) * PiInv,
        GTR2Pdf(n, wh, wo, square(mat.roughness)) / (4.0 * absDot(wh, wo)),
        1.0 / (2.0 - mat.metallic)
    );
}

bool metallicWorkflowSampleBSDF(Material mat, vec3 albedo, vec3 n, vec3 wo, vec3 r, out BSDFSample s) {
    float alpha = square(mat.roughness);
    s.type = Reflection;

    if (r.z > (1.0 / (2.0 - mat.metallic))) {
        s.wi = sampleCosineWeightedHemisphere(n, r.xy);
        s.type |= Diffuse;
    }
    else {
        vec3 wh = GTR2Sample(n, wo, alpha, vec2(r));
        s.wi = -reflect(wo, wh);
        s.type |= isGTR2Delta(mat.roughness) ? Specular : Glossy;
    }

    if (dot(n, s.wi) < 0.0) {
        s.type = InvalidBSDFSample;
        return false;
    }
    else {
        s.bsdf = metallicWorkflowBSDF(mat, albedo, n, wo, s.wi);
        s.pdf = metallicWorkflowPdf(mat, n, wo, s.wi);
    }
    return true;
}

vec3 metalBSDF(Material mat, vec3 albedo, vec3 n, vec3 wo, vec3 wi) {
    if (isGTR2Delta(mat.roughness)) {
        return vec3(0.0);
    }

    float alpha = square(mat.roughness);
    vec3 wh = normalize(wo + wi);

    float cosO = dot(n, wo);
    float cosI = dot(n, wi);

    if (cosI * cosO < 1e-7) {
        return vec3(0.0);
    }

    float f = fresnelSchlick(absDot(wh, wo), mat.ior);
    float g = smithG(cosO, cosI, alpha);
    float d = GTR2Distrib(dot(n, wh), alpha);

    return albedo * f * g * d / (4.0 * cosI * cosO);
}

float metalPdf(Material mat, vec3 n, vec3 wo, vec3 wi) {
    if (isGTR2Delta(mat.roughness)) {
        return 0.0;
    }
    vec3 wh = normalize(wo + wi);
    return GTR2Pdf(n, wh, wo, square(mat.roughness)) / (4.0 * absDot(wh, wo));
}

bool metalSampleBSDF(Material mat, vec3 albedo, vec3 n, vec3 wo, vec3 r, out BSDFSample s) {
    float alpha = square(mat.roughness);

    bool isDelta = isGTR2Delta(mat.roughness);

    if (isDelta) {
        s.wi = -reflect(wo, n);
    }
    else {
        vec3 wh = GTR2Sample(n, wo, alpha, r.xy);
        s.wi = -reflect(wo, wh);
    }

    if (dot(n, s.wi) < 0.0) {
        s.type = InvalidBSDFSample;
        return false;
    }
    else {
        s.bsdf = isDelta ? fresnelSchlick(absDot(n, wo), mat.ior) * albedo : metalBSDF(mat, albedo, n, wo, s.wi);
        s.pdf = isDelta ? 1.0 : metalPdf(mat, n, wo, s.wi);
        s.type = Reflection | (isDelta ? Specular : Glossy);
    }
    return true;
}

bool fakeSampleBSDF(Material mat, vec3 albedo, vec3 wo, out BSDFSample s) {
    s.wi = -wo;
    s.bsdf = albedo;
    s.pdf = 1.0;
    s.type = Specular | Transmission;
    return true;
}

vec3 evalBSDF(Material mat, vec3 albedo, vec3 n, vec3 wo, vec3 wi) {
    switch (mat.type) {
    case Lambert:
        return lambertBSDF(albedo, n, wi);
    case MetallicWorkflow:
        return metallicWorkflowBSDF(mat, albedo, n, wo, wi);
    case Metal:
        return metalBSDF(mat, albedo, n, wo, wi);
    case Dielectric:
    case Fake:
        return vec3(0.0);
    }
    return vec3(0.0);
}

float evalPdf(Material mat, vec3 n, vec3 wo, vec3 wi) {
    switch (mat.type) {
    case Lambert:
        return lambertPdf(n, wi);
    case MetallicWorkflow:
        return metallicWorkflowPdf(mat, n, wo, wi);
    case Metal:
        return metalPdf(mat, n, wo, wi);
    case Dielectric:
    case Fake:
        return 0.0;
    }
    return 0.0;
}

bool sampleBSDF(Material mat, vec3 albedo, vec3 n, vec3 wo, vec3 r, out BSDFSample s) {
    switch (mat.type) {
    case Lambert:
        return lambertSampleBSDF(albedo, n, r.xy, s);
    case MetallicWorkflow:
        return metallicWorkflowSampleBSDF(mat, albedo, n, wo, r, s);
    case Metal:
        return metalSampleBSDF(mat, albedo, n, wo, r, s);
    case Dielectric:
        return dielectricSampleBSDF(mat, albedo, n, wo, r, s);
    case Fake:
        return fakeSampleBSDF(mat, albedo, wo, s);
    }
    return false;
}

bool isBSDFDelta(Material mat) {
    switch (mat.type) {
    case Lambert:
        return false;
    case MetallicWorkflow:
        return (isGTR2Delta(mat.roughness) && mat.metallic > 0.95);
    case Metal:
        return isGTR2Delta(mat.roughness);
    case Dielectric:
    case Fake:
        return true;
    }
    return true;
}

bool isBSDFConnectible(Material mat) {
    switch (mat.type) {
    case Lambert:
        return true;
    case MetallicWorkflow:
        return (isGTR2Connectible(mat.roughness) || mat.metallic < 0.95);
    case Metal:
        return isGTR2Connectible(mat.roughness);
    case Dielectric:
    case Fake:
        return false;
    }
    return false;
}

bool isSampleTypeDelta(uint type) {
    return (type & Specular) == Specular;
}

#endif