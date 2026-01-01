#version 460
#extension GL_GOOGLE_include_directive : require

#include "ubo/global_ubo.glsl"
#include "common/math.glsl"

// inspired from https://vec3.ca/posts/rendering-an-infinite-grid

layout(location = 0) in vec3 fragPosWorld;
layout(location = 1) in vec3 fragPosView;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform GridPush {
    vec4 xAxisColor;
    vec4 zAxisColor;
    float gridUnitSize;
    // how many grid squares run along a grid group edge
    uint gridMinorsPerMajor;
	float nearFog;
    float farFog;
} push;

float fade(float nearFog, float farFog, float value)
{
	return 1 - smoothstep(nearFog, farFog, value);
}

// scales the near and far fog distances by fogScale.x and .y, respectively
// and then applies the regular fade equation
// undefined if fogScale.x > fogScale.y
float scaledFade(float nearFog, float farFog, float value, vec2 fogScale)
{
	vec2 fogDistances = vec2(nearFog, farFog);
	vec2 fogDistScaled = fogDistances * fogScale;
	return 1 - smoothstep(fogDistScaled.x, fogDistScaled.y, value);
}

vec4 evalGridColor(vec2 gridCoord, float majorFog, float minorFog) {
	// find the index of the closest grid line to this pixel
	ivec2 lineIndex = ivec2(floor(gridCoord));

	// pick an appropriate width and color for the closest line (in *each* of X and Y!)
	vec2 lineWidth;
	vec4 lineColor[2];
	vec2 lineFog;

	mat2x4 axesColors;
	axesColors[0] = push.xAxisColor;
	axesColors[1] = push.zAxisColor;

	for (uint i = 0; i < 2; i++)
	{
		float width;
		vec4 color;
		float fog;

		if (lineIndex[i] == 0)
		{
			width = 5.0;
			color = axesColors[1 - i];
			fog = majorFog;
		}
		else if (lineIndex[i] % push.gridMinorsPerMajor == 0)
		{
			width = 3.0;
			color = vec4(0.5, 0.5, 0.5, 1.0);
			fog = majorFog;
		}
		else
		{
			width = 2.0;
			color = vec4(0.25, 0.25, 0.25, 1.0);
			fog = minorFog;
		}

		lineWidth[i] = width;
		lineColor[i] = color;
		lineFog[i] = fog;
	}

	vec2 lineDist = abs(0.5 - fract(gridCoord)) * 2;
	vec2 lineMask = 1 - saturate(lineDist /
		(fwidth(gridCoord) * lineWidth));

	vec2 blendFactors = lineMask * lineFog;
	for (uint i = 0; i < 2; i++)
		if (lineIndex[1 - i] == 0 && lineIndex[i] != 0)
			blendFactors[i] *= smoothstep(0, 0.05, lineDist[1 - i]);

	return max(lineColor[0] * blendFactors.x, lineColor[1] * blendFactors.y);
}


void main() {
    // figure out our fogging values
	const float viewDist = length(fragPosView);

	// we scale the fog distances based on the camera's height
	const float cameraHeight = abs(ubo.inverseViewMatrix[3].y);
	const float fogCameraScale = max(1.0, cameraHeight / 10.0);
	const float nearFogScaled = push.nearFog * fogCameraScale;
	const float farFogScaled = push.farFog * fogCameraScale;

	const float majorFog = fade(nearFogScaled, farFogScaled, viewDist);
	const float minorFog = scaledFade(nearFogScaled, farFogScaled, viewDist, vec2(0.5, 0.85));

	// render two grid at a time and blend them together based on camera height - tune this to get desired sas
	float gridLod = log(max(cameraHeight, 0.001) * 0.7) / log(float(push.gridMinorsPerMajor));
	gridLod = max(gridLod, 0.0);

	float lod0 = floor(gridLod);
	float lod1 = lod0 + 1.0;
	float lodBlend = smoothstep(0.0, 1.0, fract(gridLod));

	float unit0 = push.gridUnitSize * pow(float(push.gridMinorsPerMajor), lod0);
	float unit1 = push.gridUnitSize * pow(float(push.gridMinorsPerMajor), lod1);

	vec2 gridCoord0 = fragPosWorld.xz / unit0 + 0.5;
	vec2 gridCoord1 = fragPosWorld.xz / unit1 + 0.5;

	vec4 gridColor0 = evalGridColor(gridCoord0, majorFog, minorFog);
	vec4 gridColor1 = evalGridColor(gridCoord1, majorFog, minorFog);

	vec4 finalColor = mix(gridColor0, gridColor1, lodBlend);

	outColor = finalColor;
}
