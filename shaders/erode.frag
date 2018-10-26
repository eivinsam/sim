#version 430

uniform sampler2DRect water;

uniform float dt;
uniform float rdx;
uniform float rdy;

out vec4 new_water;

vec4 fetch(ivec2 uv)
{
	vec4 w = texelFetch(water, uv);

	w.xy = w.xy-0.5;

	return w;
}


float square(float x) { return x*x; }

void main()
{
	ivec2 uv = ivec2(gl_FragCoord.xy);

	vec4 w  = fetch(uv);

	vec4 wpy = (fetch(uv+ivec2(0,+1)));//*2 + wpp + wnp)*0.25;
	vec4 wpx = (fetch(uv+ivec2(+1,0)));//*2 + wpp + wpn)*0.25;
	vec4 wny = (fetch(uv+ivec2(0,-1)));//*2 + wnn + wpn)*0.25;
	vec4 wnx = (fetch(uv+ivec2(-1,0)));//*2 + wnn + wnp)*0.25;

	float gpx = w.w - wpx.w;
	float gnx = w.w - wnx.w;
	float gpy = w.w - wpy.w;
	float gny = w.w - wny.w;

	new_water = w;
	new_water.xy = vec2(0.5);
	if (uv.x < 1 || uv.x > textureSize(water).x-2 || 
		uv.y < 1 || uv.y > textureSize(water).y-2)
		return;

	new_water.w -= 0.1*(square(gpx) + square(gnx) + gpy + gny);
}
