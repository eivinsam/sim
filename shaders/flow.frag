#version 430

layout(binding=0) uniform sampler2DRect water;
layout(binding=1) uniform sampler2DRect sed;

uniform float dt;
uniform float rdx;
uniform float rdy;

layout(location=0) out vec4 new_water;
layout(location=1) out vec2 new_sed;

vec4 fetch(ivec2 uv)
{
	vec4 w = texelFetch(water, uv);

	//w.xy = w.xy-0.5;

	return w;
}


const float density = 0.01; // kg/m3
const float gravity = 0.5; // m/s2
const float b = 0.0125*1; // viscous drag coefficient
const float v = 0.08*1;

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
float weight_differential(vec4 a, vec4 b)
{
	return ((b.z+b.w) - (a.z+a.w))*gravity; // kgm/s2 -> force
}

void main()
{
	// ## modified shallow water model 
	// each texel (u,v) contains four pieces of information:
	// x: x-moment average - kgm/s //x-velocity in (u,v) - m/s
	// y: y-moment average - kgm/s //y-velocity in (u,v) - m/s
	// z: mass in cell (u,v) - kg
	// w: average terrain height in (u,v) in kg watercolumn equivalent - kg
	//
	// global parameters:
	// dt: change in time (s)
	// rdx: reciprocal x-length of cells (1/m)
	// rdy: reciprocal y-length of cells (1/m)
	//
	// volume must be preserved, preserve momentum as far as possible!

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

	float water_incr = 0.00000005;
	new_water.w = w.w;// + clamp(100*flow*length(vec2((wpx.w - wnx.w)*rdx, (wpy.w - wny.w)*rdy)), -0.01, 0.01); //0.05*flow/(w.z+0.001);
	new_water.z = w.z-0.0001*0 + flow;
	if (new_water.z < 0)
		new_water.z = 0;
	if (new_water.z == 0)
	{
		new_water.xy = vec2(0.5*0);
		new_water.z += water_incr;
		//new_water.w = w.w;
		return;
	}


	// calculate mass change and forces on (u,v) - units [kgm/s2, kgm/s2, kg/s]
	vec3 dw = ((fnx - fpx)*rdx + (fny - fpy)*rdy);
	
	float h = w.z*rdx*rdy/density;
	dw.xy -= vec2(
		weight_differential(wnx, wpx)*(rdx*0.5),
		weight_differential(wny, wpy)*(rdy*0.5));
		//weight_differential((2*wnx+wnn+wnp)*0.25, (2*wpx+wpp+wpn)*0.25)*(rdx+rdx), // kg/s2
		//weight_differential((2*wny+wnn+wpn)*0.25, (2*wpy+wpp+wnp)*0.25)*(rdy+rdy)) *h;
	dw.xy += vec2(
		(wpx.x + wnx.x + wpy.x + wny.x - 4*w.x)*rdx*rdx,
		(wpx.y + wnx.y + wpy.y + wny.y - 4*w.y)*rdy*rdy)*v;

	//dwdt.xy /= w.z; // units now [ m/s2, m/s2, kg/s ]
	vec2 friction = w.xy*(1-exp(-b*dt*pow(w.z+0.0001, -0.33333)));
	new_water.xy = w.xy - friction + dt*dw.xy; 
	
	if (uv.x < 1 || uv.x > textureSize(water).x-2 || 
		uv.y < 1 || uv.y > textureSize(water).y-2) 
		new_water.xyzw = vec4(w.xy, 0, w.w);
	else
		new_water.z += water_incr;
	//	new_water.z = clamp(new_water.z, 0.00,0.98)+0.00001*0;
	float next_loss = dot(abs(new_water.xy), vec2(rdx, rdy))*dt;
	if (next_loss > new_water.z)
		new_water.xy = new_water.xy*(new_water.z/next_loss);

	vec3 s = vec3(texelFetch(sed, uv).xy, w.z);

	vec3 spy = vec3(texelFetch(sed, uv+ivec2(0,py)).xy, wpy.z);
	vec3 spx = vec3(texelFetch(sed, uv+ivec2(px,0)).xy, wpx.z);
	vec3 sny = vec3(texelFetch(sed, uv+ivec2(0,ny)).xy, wny.z);
	vec3 snx = vec3(texelFetch(sed, uv+ivec2(nx,0)).xy, wnx.z);

	vec3 sfnx = interface_flow(interface_velocity(s.xz, snx.xz), s.xyz, snx.xyz);//*int(uv.x > 2);
	vec3 sfpx = interface_flow(interface_velocity(spx.xz, s.xz), spx.xyz, s.xyz);//*int(uv.x < textureSize(water).x-2);
	vec3 sfny = interface_flow(interface_velocity(s.yz, sny.yz), s.xyz, sny.xyz);//*int(uv.y > 2);
	vec3 sfpy = interface_flow(interface_velocity(spy.yz, s.yz), spy.xyz, s.xyz);//*int(uv.y < textureSize(water).y-2);

	float sflow = dt*( 
		(max(0,snx.x) - min(0,spx.x) - abs(s.x))*rdx +
		(max(0,sny.y) - min(0,spy.y) - abs(s.y))*rdy);

	//new_water.w += sflow;

	new_sed = s.xy*0.90 + 0.01*new_water.xy + ((sfnx - sfpx)*rdx + (sfny - sfpy)*rdy).xy*dt;
	float next_sloss = dot(abs(new_sed.xy), vec2(rdx, rdy))*dt;
	if (next_sloss > 0.5*new_water.z)
		new_sed.xy = new_sed.xy*(0.5*new_water.z/next_sloss);

	//new_water.xy += 0.5;
	
	//if (uv.x == 33 && uv.y == 33)
	//	new_water.z = 0.1;
}
