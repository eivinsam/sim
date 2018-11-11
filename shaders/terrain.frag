#version 430

uniform sampler2DRect tex;

in vec2 uv;
out vec4 frag_color;

void main()
{
	vec2 dx = vec2(1, 0);
	vec2 dy = vec2(0, 1);
	vec3 normal = normalize(vec3(
		texture(tex, uv+dx).r - texture(tex, uv-dx).r,
		texture(tex, uv+dy).r - texture(tex, uv-dy).r, 0.2));
	frag_color = vec4(texture(tex, uv).r*0 + normal.zzz*0.9 + 0.1, 1);

	frag_color = vec4((uv-0.5)/textureSize(tex), 0, 1);
}
