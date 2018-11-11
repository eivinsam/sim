#version 430

uniform sampler2DRect water;

uniform float dt;
uniform float rdx;
uniform float rdy;

out vec4 new_water;

vec4 fetch(ivec2 uv)
{
	vec4 w = texelFetch(water, uv);

	w.xy = w.xy;

	return w;
}

float interface_velocity(vec2 a, vec2 b)
{
	return (a.x+b.x)/max(0.05, a.y+b.y);
}
vec3 interface_flow(float v, vec3 a, vec3 b)
{
	vec3 c = v < 0 ? a : b;
	vec3 flow = v*c;
	if (flow.z*dt*rdx > c.z)
		flow *= c.z/(flow.z*dt*rdx);
	return flow;
}


float square(float x) { return x*x; }

void main()
{
	ivec2 uv = ivec2(gl_FragCoord.xy);

	vec4 w  = fetch(uv);

	int nx = -int(uv.x > 0);
	int ny = -int(uv.y > 0);
	int px = +int(uv.x < textureSize(water).x-1);
	int py = +int(uv.y < textureSize(water).y-1);

	vec4 wpp = fetch(uv+ivec2(px,py));
	vec4 wpn = fetch(uv+ivec2(px,ny));
	vec4 wnn = fetch(uv+ivec2(nx,ny));
	vec4 wnp = fetch(uv+ivec2(nx,py));



	vec4 wpy = (fetch(uv+ivec2(0,py))*2 + wpp + wnp)*0.25;
	vec4 wpx = (fetch(uv+ivec2(px,0))*2 + wpp + wpn)*0.25;
	vec4 wny = (fetch(uv+ivec2(0,ny))*2 + wnn + wpn)*0.25;
	vec4 wnx = (fetch(uv+ivec2(nx,0))*2 + wnn + wnp)*0.25;

	vec3 fnx = interface_flow(interface_velocity(w.xz, wnx.xz), w.xyz, wnx.xyz);//*int(uv.x > 2);
	vec3 fpx = interface_flow(interface_velocity(wpx.xz, w.xz), wpx.xyz, w.xyz);//*int(uv.x < textureSize(water).x-2);
	vec3 fny = interface_flow(interface_velocity(w.yz, wny.yz), w.xyz, wny.xyz);//*int(uv.y > 2);
	vec3 fpy = interface_flow(interface_velocity(wpy.yz, w.yz), wpy.xyz, w.xyz);//*int(uv.y < textureSize(water).y-2);

	float flow = dt*( 
		(max(0,wnx.x) - min(0,wpx.x) - abs(w.x))*rdx +
		(max(0,wny.y) - min(0,wpy.y) - abs(w.y))*rdy);

	new_water = w;

	if (uv.x < 1 || uv.x > textureSize(water).x-2 || 
		uv.y < 1 || uv.y > textureSize(water).y-2) 
		new_water.xyzw = vec4(w.xy, 0, w.w);
	else
		new_water.w -= 0.002*length(w.xy)/(w.z+0.001);

	//new_water.xy = vec2(0.5);
	//if (uv.x < 1 || uv.x > textureSize(water).x-2 || 
	//	uv.y < 1 || uv.y > textureSize(water).y-2)
	//	return;
	//
	//new_water.w += max(0, -min(min(gpx, gnx), min(gpy, gny))-0.003);
}
