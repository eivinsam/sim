#version 430

uniform sampler2DRect tex;

layout(quads, equal_spacing, ccw) in;
out vec2 uv;
out vec3 wpos;

float height(vec4 w) { return w.z + w.w; }

void main()
{
   uv = 0.5 + gl_TessCoord.xy*(textureSize(tex,0)-1);
   wpos = vec3(2*gl_TessCoord.x-1, 2*gl_TessCoord.y-1, height(texture(tex, uv)));
   gl_Position = gl_ModelViewProjectionMatrix *
      vec4(wpos, 1);
}
