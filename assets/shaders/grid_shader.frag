#version 460
#extension GL_GOOGLE_include_directive : require

#include "ubo/global_ubo.glsl"
#include "common/math.glsl"

// inspired from https://vec3.ca/posts/rendering-an-infinite-grid

layout(location = 0) in vec3 fragPosWorld;
layout(location = 1) in vec3 fragPosView;
layout(location = 2) in vec2 gridCoord;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform GridPush {
    vec4 xAxisColor;
    vec4 zAxisColor;
    float gridUnitSize;
    // how many grid squares run along a grid group edge
    uint gridMinorsPerMajor;
} push;

void main() {
    // figure out our fogging values
	float viewDist = length(fragPosView);
	//var majorFog = Fog.Fade(viewDist);
	//var minorFog = Fog.ScaledFade(viewDist, vec2(0.5, 0.85));

	// find the index of the closest grid line to this pixel
	ivec2 lineIndex = ivec2(floor(gridCoord));

	// pick an appropriate width and color for the closest line (in *each* of X and Y!)
	vec2 lineWidth;
	vec3 lineColor[2];
	//vec2 lineFog;

	mat2x4 axesColors;
	axesColors[0] = push.xAxisColor;
	axesColors[1] = push.zAxisColor;

	for (uint i = 0; i < 2; i++)
	{
		float width;
		vec3 color;
		//float fog;

		if (lineIndex[i] == 0)
		{
			width = 5.0;
			color = axesColors[i].rbg;
			//fog = majorFog;
		}
		else if (lineIndex[i] % push.gridMinorsPerMajor == 0)
		{
			width = 3.0;
			color = vec3(0.5);
			//fog = majorFog;
		}
		else
		{
			width = 2.0;
			color = vec3(0.25);
			//fog = minorFog;
		}

		lineWidth[i] = width;
		lineColor[i] = color;
		//lineFog[i] = fog;
	}

	vec2 lineDist = abs(0.5 - fract(gridCoord)) * 2;
	vec2 lineMask = 1 - saturate(lineDist /
		(fwidth(gridCoord) * lineWidth));

	vec2 blendFactors = lineMask;// * lineFog;
	for (uint i = 0; i < 2; i++)
		if (lineIndex[1 - i] == 0 && lineIndex[i] != 0)
			blendFactors[i] *= smoothstep(0, 0.5, lineDist[1 - i]);

	vec3 finalColor = max(lineColor[0] * blendFactors.x, lineColor[1] * blendFactors.y);

	outColor = vec4(finalColor, 1.0);
}
