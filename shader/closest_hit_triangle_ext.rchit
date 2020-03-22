#version 460 core
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;

struct RayPayload {
	vec4 color;
	uint recursionDepth;
};

layout (location = 0) rayPayloadInEXT RayPayload rayPayload;
hitAttributeEXT vec2 attribs;
layout(location = 2) rayPayloadEXT bool isHit;

struct SceneConstantBuffer {
	mat4x4 projectionToWorld;
	vec4 cameraPosition;
	vec4 lightPosition;
	vec4 lightAmbientColor;
	vec4 lightDiffuseColor;
	float reflectance;
	float elapsedTime;
};

struct PrimitiveConstantBuffer {
	vec4 albedo;
	float reflectanceCoef;
	float diffuseCoef;
	float specularCoef;
	float specularPower;
	float stepScale;

	float padding[3];
};

layout(set = 0, binding = 2, std140) uniform appData {
	SceneConstantBuffer params;
};

const vec4 kBackgroundColor = vec4(0.8f, 0.9f, 1.0f, 1.0f);
const float kInShadowRadiance = 0.35f;

struct Ray {
	vec3 origin;
	vec3 direction;
};

Ray generateCameraRay(uvec2 index, in vec3 cameraPosition, in mat4x4 projectionToWorld) {

	vec2 xy = index + 0.5;
	vec2 screenPos = xy / gl_LaunchSizeEXT.xy * 2.0 - 1.0;

	screenPos.y = -screenPos.y;

	vec4 world = projectionToWorld * vec4(screenPos, 0.0, 1.0);
	world.xyz /= world.w;

	Ray ray;
	ray.origin = cameraPosition;
	ray.direction = normalize(world.xyz - ray.origin);

	return ray;
}

layout(set = 0, binding = 3, std430) readonly buffer FacesBuffer {
	uvec4 faces[];
} FacesArray[];

layout(set = 0, binding = 4, std430) readonly buffer NormalBuffer {
	vec4 normals[];
} NormalArray[];

layout(shaderRecordEXT) buffer InlineData {
	PrimitiveConstantBuffer material;
};

vec2 texCoords(in vec3 position) {
	return position.xz;
}

void calculateRayDifferentials(out vec2 ddx_uv, out vec2 ddy_uv, in vec2 uv, in vec3 hitPosition, in vec3 surfaceNormal, in vec3 cameraPosition, in mat4x4 projectionToWorld) {
	Ray ddx = generateCameraRay(gl_LaunchIDEXT.xy + uvec2(1, 0), cameraPosition, projectionToWorld);
	Ray ddy = generateCameraRay(gl_LaunchIDEXT.xy + uvec2(0, 1), cameraPosition, projectionToWorld);

	vec3 ddx_pos = ddx.origin - ddx.direction * dot(ddx.origin - hitPosition, surfaceNormal) / dot(ddx.direction, surfaceNormal);
	vec3 ddy_pos = ddy.origin - ddy.direction * dot(ddy.origin - hitPosition, surfaceNormal) / dot(ddy.direction, surfaceNormal);

	ddx_uv = texCoords(ddx_pos) - uv;
	ddy_uv = texCoords(ddy_pos) - uv;
}

float checkersTextureBoxFilter(in vec2 uv, in vec2 dpdx, in vec2 dpdy, in uint ratio) {
	vec2 w = max(abs(dpdx), abs(dpdy));
	vec2 a = uv + 0.5 * w;
	vec2 b = uv - 0.5 * w;

	vec2 i = (floor(a) + min(fract(a) * ratio, 1.0) - floor(b) - min(fract(b) * ratio, 1.0)) / (ratio * w);
	return (1.0 - i.x) * (1.0 - i.y);
}

float analyticalCheckersTexture(in vec3 hitPosition, in vec3 surfaceNormal, in vec3 cameraPosition, in mat4x4 projectionToWorld) {
	vec2 ddx_uv;
	vec2 ddy_uv;
	vec2 uv = texCoords(hitPosition);

	calculateRayDifferentials(ddx_uv, ddy_uv, uv, hitPosition, surfaceNormal, cameraPosition, projectionToWorld);
	return checkersTextureBoxFilter(uv, ddx_uv, ddy_uv, 50);
}

vec3 hitWorldPosition() {
	return gl_WorldRayOriginEXT + gl_RayTmaxEXT * gl_WorldRayDirectionEXT;
}

float calculateDiffuseCoefficient(in vec3 hitPosition, in vec3 incidentLightRay, in vec3 normal) {
	float fNDotL = clamp(dot(-incidentLightRay, normal), 0.0, 1.0);
	return fNDotL;
}

float calculateSpecularCoefficient(in vec3 hitPosition, in vec3 incidentLightRay, in vec3 normal, in float specularPower) {
	vec3 reflectedLightRay = normalize(reflect(incidentLightRay, normal));
	return pow(clamp(dot(reflectedLightRay, normalize(-gl_WorldRayDirectionEXT)), 0.0, 1.0), specularPower);
}

vec4 calculatePhongLighting(in vec4 albedo, in vec3 normal, in bool isInShadow, in float diffuseCoeff, in float specularCoef, in float specularPower) {
	vec3 hitPosition = hitWorldPosition();
	vec3 lightPosition = params.lightPosition.xyz;
	float shadowFactor = isInShadow ? kInShadowRadiance : 1.0;
	vec3 incidentLightRay = normalize(hitPosition - lightPosition);

	vec4 lightDiffuseColor = params.lightDiffuseColor;
	float kd = calculateDiffuseCoefficient(hitPosition, incidentLightRay, normal);
	vec4 diffuseColor = shadowFactor * diffuseCoeff * kd * lightDiffuseColor * albedo;

	vec4 specularColor = vec4(0.0, 0.0, 0.0, 0.0);
	if (!isInShadow) {
		vec4 lightSpecularColor = vec4(1.0, 1.0, 1.0, 1.0);
		float ks = calculateSpecularCoefficient(hitPosition, incidentLightRay, normal, specularPower);
		specularColor = specularCoef * ks * lightSpecularColor;
	}

	vec4 ambientColor = params.lightAmbientColor;
	vec4 ambientColorMin = params.lightAmbientColor - 0.1;
	vec4 ambientColorMax = params.lightAmbientColor;
	float a = 1.0 - clamp(dot(normal, vec3(0.0, -1.0, 0.0)), 0.0, 1.0);
	ambientColor = albedo * mix(ambientColorMin, ambientColorMax, a);

	return ambientColor + diffuseColor + specularColor;
}

vec4 calculatePhongLighting(in vec4 albedo, in vec3 normal, in bool isInShadow) {
	return calculatePhongLighting(albedo, normal, isInShadow, 1.0, 1.0, 50);
}

vec4 calculatePhongLighting(in vec4 albedo, in vec3 normal, in bool isInShadow, in float diffuseCoeff) {
	return calculatePhongLighting(albedo, normal, isInShadow, diffuseCoeff, 1.0, 50);
}

vec4 calculatePhongLighting(in vec4 albedo, in vec3 normal, in bool isInShadow, in float diffuseCoeff, in float specularCoef) {
	return calculatePhongLighting(albedo, normal, isInShadow, diffuseCoeff, specularCoef, 50);
}

vec3 fresnelReflectanceSchlick(in vec3 I, in vec3 N, in vec3 f0) {
	float cosi = clamp(dot(-I, N), 0.0, 1.0);
	return f0 + (1.0 - f0) * pow(1.0 - cosi, 5.0);
}

vec4 traceRadianceRay(in Ray ray, in uint currentRayRecursionDepth) {

	if (currentRayRecursionDepth > 3) {
		return vec4(0.0, 0.0, 0.0, 0.0);
	}

	vec3 origin = ray.origin;
	vec3 direction = ray.direction;

	float tmin = 0;
	float tmax = 10000;

	rayPayload.color = vec4(0.0, 0.0, 0.0, 0.0);
	rayPayload.recursionDepth = currentRayRecursionDepth + 1;
	traceRayEXT(topLevelAS, gl_RayFlagsCullBackFacingTrianglesEXT, 0xff, 0, 0, 0, origin, tmin, direction, tmax, 0);

	return rayPayload.color;
}

bool traceShadowRayAndReportIfHit(in Ray ray, in uint currentRayRecursionDepth) {

	if (currentRayRecursionDepth > 3) {
		return false;
	}

	vec3 origin = ray.origin;
	vec3 direction = ray.direction;
	float tmin = 0;
	float tmax = 10000;

	isHit = true;

	traceRayEXT(topLevelAS, gl_RayFlagsCullBackFacingTrianglesEXT | gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT, 0xff, 0, 0, 1, origin, tmin, direction, tmax, 2);

	return isHit;
}

void main() {
	uint indexSizeInBytes = 2;
	uint indicesPerTriangle = 3;
	uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
	uint baseIndex = gl_PrimitiveID * triangleIndexStride;

	const uvec4 face = FacesArray[nonuniformEXT(gl_InstanceCustomIndexEXT)].faces[gl_PrimitiveID];

	const vec3 triangleNormal = NormalArray[nonuniformEXT(gl_InstanceCustomIndexEXT)].normals[int(face.x)].xyz;

	vec3 hitPosition = hitWorldPosition();

	Ray shadowRay = { hitPosition, normalize(params.lightPosition.xyz - hitPosition) };
	bool shadowRayHit = traceShadowRayAndReportIfHit(shadowRay, rayPayload.recursionDepth);

	float checkers = analyticalCheckersTexture(hitPosition, triangleNormal, params.cameraPosition.xyz, params.projectionToWorld);

	vec4 reflectedColor = vec4(0.0, 0.0, 0.0, 0.0);
	if (material.reflectanceCoef > 0.001) {
		Ray reflectionRay = { hitPosition, reflect(gl_WorldRayDirectionEXT, triangleNormal) };
		vec4 reflectionColor = traceRadianceRay(reflectionRay, rayPayload.recursionDepth);

		vec3 fresnelR = fresnelReflectanceSchlick(gl_WorldRayDirectionEXT, triangleNormal, material.albedo.xyz);
		reflectedColor = material.reflectanceCoef * vec4(fresnelR, 1.0) * reflectionColor;
	}

	vec4 phongColor = calculatePhongLighting(material.albedo, triangleNormal, shadowRayHit, material.diffuseCoef, material.specularCoef, material.specularPower);
	vec4 color = checkers * (phongColor + reflectedColor);

	float t = gl_RayTmaxEXT;
	color = mix(color, kBackgroundColor, 1.0 - exp(-0.000002 * t * t * t));

	rayPayload.color = color;
}
