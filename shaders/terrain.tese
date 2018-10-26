#version 430

uniform sampler2DRect tex;

layout(quads, equal_spacing, ccw) in;
out vec2 uv;

void main()
{
   uv = 0.5 + gl_TessCoord.xy*(textureSize(tex,0)-1);
   gl_Position = gl_ModelViewProjectionMatrix *
      vec4(2*gl_TessCoord.x-1, 2*gl_TessCoord.y-1, 0*texture(tex, uv).r, 1);
}
