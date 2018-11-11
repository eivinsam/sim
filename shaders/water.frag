#version 430

layout(binding=0) uniform sampler2DRect tex;
layout(binding=1) uniform sampler2DRect sed;

in vec2 uv;
in vec3 wpos;
out vec4 frag_color;

float height(vec4 w) { return w.z + w.w; }

void main()
{
	vec2 dx = vec2(1, 0);
	vec2 dy = vec2(0, 1);
	vec3 normal = normalize(vec3(
		height(texture(tex, uv-dx)) - height(texture(tex, uv+dx)),
		height(texture(tex, uv-dy)) - height(texture(tex, uv+dy)), 0.005));


	vec4 w = texture(tex, uv);

	vec2 s = texture(sed, uv).xy;

	float wzs = w.z*200;
	float wetness = clamp(sqrt(wzs) + length(w.xy)*5000, 0, 1); //clamp(1-(1-min(wzs,1))*(1-min(wzs,1)), 0, 1);
	float rockyness = clamp((1-normal.z)*30 -7, 0, 1);


	frag_color = vec4(vec3(max(0, sin((w.w)*6.28*100)-0.8))*0.5 + 
		((normal.z*0.7+0.2)*vec3(0,0.7-10*w.z,0.6)*wetness + (1-wetness)*(
		(normal.z*0.6+0.3)*vec3(0.7, 0.98-0.4*(w.z+w.w), 0.4)*(1-rockyness) + (normal.z*0.7+0.2)*rockyness*vec3(0.65, 0.65, 0.6))  ), 1);

	vec3 eye = -gl_ModelViewMatrix[3].xyz*mat3(gl_ModelViewMatrix);
	vec3 edir = normalize(eye - wpos);
	vec3 ldir = normalize(vec3(1,1,2));
	vec3 hvec = normalize(edir + ldir);
	float ca = max(0, dot(normal, hvec));
	float ca2 = ca*ca;
	float m2 = 0.03*wetness + (1-wetness)*(0.8-0.5*rockyness);
	frag_color.xyz += 0.2*vec3(1,1,0.7)*max(0, exp((ca2-1)/(ca2*m2)) / (3.141592*m2*ca2*ca2));
	//frag_color.xyz = edir*0.5 + 0.5;

	//frag_color = vec4(w.xy*1000+0.5,0, 1);
	//frag_color = vec4(vec3(w.z)*100 + max(0, sin((w.z+w.w)*6.28*100)-0.8), 1);
}
