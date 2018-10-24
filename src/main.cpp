#include <oui_window.h>
#include <oui_debug.h>

#include <GL/glew.h>

#include <vector>
#include <random>

template <class T>
class Map
{
	std::vector<T> _data;

	class Location;
	class Offset
	{
		friend class Map<T>;
		friend class Map<T>::Location;
		short _val;
		constexpr Offset(short val) : _val(val) { }
	public:
		friend Offset operator+(Offset a, Offset b) { return { a._val + b._val }; }
		friend Offset operator-(Offset a, Offset b) { return { a._val - b._val }; }

		friend Location operator+(Location l, Offset o);
		friend Location operator+(Offset o, Location l);
		friend Location operator-(Location l, Offset o);
	};
public:
	Offset row;

	static constexpr Offset col = { 1 };

	Map(short width, short height) : row(width) { _data.resize(int(width)*height, 0); }

	short width() const { return row._val; }
	short height() const { return static_cast<short>(_data.size() / row._val); }

	using iterator = T*;

	class Location
	{
		friend class Map<T>;
		iterator _it;

		Location(iterator it) : _it(std::move(it)) { }
	public:

		decltype(auto) operator*() { return *_it; }
		decltype(auto) operator->() { return _it.operator->(); }

		Location& operator+=(Offset o) { _it += o._val; return *this; }

		bool operator< (const Location& that) const { return _it <  that._it; }
		bool operator<=(const Location& that) const { return _it <= that._it; }
		bool operator!=(const Location& that) const { return _it != that._it; }
		bool operator==(const Location& that) const { return _it == that._it; }
		bool operator>=(const Location& that) const { return _it >= that._it; }
		bool operator> (const Location& that) const { return _it >  that._it; }

		friend Location operator+(Location l, Offset o) { return { l._it + o._val }; }
		friend Location operator+(Offset o, Location l) { return { l._it + o._val }; }
		friend Location operator-(Location l, Offset o) { return { l._it - o._val }; }
	};

	Location operator()(short x, short y) { return _data.data() + (x + row._val * y); }


	Location begin() { return _data.data(); }
	Location end() { return _data.data() + _data.size(); }
};

const GLchar vert[] =
"#version 400\n"
"in vec3 vp;"
"void main() {"
"  gl_Position = vec4(vp, 1.0);"
"}";
const char frag[] =
"#version 430\n"
"in vec2 uv;"
"uniform float tex_shift;"
"uniform sampler2D tex;"
"out vec4 frag_colour;"
"void main() {"
//"  frag_colour = texture(tex, uv).rrrw;"
"	vec2 dx = vec2(tex_shift, 0)*0.5;"
"	vec2 dy = vec2(0, tex_shift)*0.5;"
"   vec3 normal = normalize(vec3("
"		texture(tex, uv+dx).r - texture(tex, uv-dx).r,"
"		texture(tex, uv+dy).r - texture(tex, uv-dy).r, tex_shift*0.5));"
"	frag_colour = vec4(normal.zzz/length(normal)*0.8 + 0.2, 1);"
"}";

const GLchar tess_ctrl[] = ""
"#version 430\n"
"layout(vertices = 4) out;"
"void main(void)"
"{"
"	gl_TessLevelOuter[0] = 16.0;"
"	gl_TessLevelOuter[1] = 16.0;"
"	gl_TessLevelOuter[2] = 16.0;"
"	gl_TessLevelOuter[3] = 16.0;"
"	gl_TessLevelInner[0] = 32.0;"
"	gl_TessLevelInner[1] = 32.0;"
""
"	gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;"
"}";
const GLchar tess_eval[] = ""
"#version 430\n"
"uniform float time;" 
"uniform float tex_shift;"
"uniform float tex_scale;"
"uniform sampler2D tex;"
"layout(quads, equal_spacing, ccw) in;"
"out vec2 uv;"
//""
//"vec4 interpolate(in vec4 v0, in vec4 v1, in vec4 v2, in vec4 v3)"
//"{"
//"	vec4 a = mix(v0, v1, gl_TessCoord.x);"
//"	vec4 b = mix(v3, v2, gl_TessCoord.x);"
//"	return mix(a, b, gl_TessCoord.y);"
//"}"
//""
"void main()"
"{"
//"	gl_Position = gl_ModelViewProjectionMatrix * interpolate("
//"		gl_in[0].gl_Position,"
//"		gl_in[1].gl_Position,"
//"		gl_in[2].gl_Position,"
//"		gl_in[3].gl_Position);"
"   uv = tex_shift + gl_TessCoord.xy*tex_scale;"
"   gl_Position = gl_ModelViewProjectionMatrix *"
"      vec4(2*gl_TessCoord.x-1, 2*gl_TessCoord.y-1, texture(tex, uv).x, 1);"
"}";

GLuint newShader(GLenum type, const GLchar* data, GLint length)
{
	auto result = glCreateShader(type);
	glShaderSource(result, 1, &data, &length);
	glCompileShader(result);

	char buffer[1024];
	glGetShaderInfoLog(result, sizeof(buffer), 0, buffer);
	oui::debug::println("shader compilation:\n", buffer);

	return result;
}

GLuint setupShaders()
{
	GLuint prog = glCreateProgram();

	glAttachShader(prog, newShader(GL_VERTEX_SHADER, vert, sizeof(vert)));
	glAttachShader(prog, newShader(GL_TESS_CONTROL_SHADER, tess_ctrl, sizeof(tess_ctrl)));
	glAttachShader(prog, newShader(GL_TESS_EVALUATION_SHADER, tess_eval, sizeof(tess_eval)));
	glAttachShader(prog, newShader(GL_FRAGMENT_SHADER, frag, sizeof(frag)));

	glLinkProgram(prog);

	char buffer[1024];
	glGetProgramInfoLog(prog, sizeof(buffer), 0, buffer);
	oui::debug::println("shader linking:\n", buffer);

	return prog;
}

static std::mt19937 rng;

void recursive_mapgen(Map<unsigned short>& out, double strength)
{
	if (out.width() < 10)
	{
		for (auto it = out.begin(); it < out.end(); it += out.row)
			for (auto jt = it; jt < it + out.row; jt += out.col)
				*jt = static_cast<unsigned short>(std::normal_distribution(32000.0, 4000.0)(rng));
	}
	else
	{
		Map<unsigned short> sub(out.width() / 2 + 1, out.height() / 2 + 1);
		recursive_mapgen(sub, strength*3);
		for (auto it = sub.begin(), ot = out.begin(); it < sub.end(); it += sub.row, ot += (out.row + out.row))
			for (auto jt = it, pt = ot; jt < it + sub.row; jt += sub.col, pt += (out.col + out.col))
			{
				*pt = *jt;
			}
		for (auto it = out.begin(); it < out.end(); it += (out.row + out.row))
			for (auto jt = it + out.col; jt < it + out.row; jt += (out.col + out.col))
				*jt = (*(jt - out.col) + *(jt + out.col)) >> 1;
		for (auto it = out.begin() + out.row; it < out.end(); it += (out.row + out.row))
			for (auto jt = it; jt < it + out.row; jt += out.col)
				*jt = (*(jt - out.row) + *(jt + out.row)) >> 1;
		for (auto it = out.begin(); it < out.end(); it += out.row)
			for (auto jt = it; jt < it + out.row; jt += out.col)
				*jt += static_cast<unsigned short>(std::normal_distribution(0.0, strength)(rng));
	}
}

int main()
{
	auto window = oui::Window{ { "SIM" } };

	float quad[] =
	{
		-1, -1, 0,
		-1, +1, 0,
		+1, +1, 0,
		+1, -1, 0
	};
	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glEnableVertexAttribArray(0);

	GLuint buffer;
	glGenBuffers(1, &buffer);
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

	auto prog = setupShaders();


	short mapsize = 257;
	Map<unsigned short> map(mapsize, mapsize);
	recursive_mapgen(map, 10);

	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R16, mapsize, mapsize, 0, GL_RED, GL_UNSIGNED_SHORT, &*map.begin());
	

	glClearColor(0, 0, 0, 0);

	auto start = oui::now();

	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	glDisable(GL_BLEND);


	while (window.update())
	{
		auto time = (oui::now() - start).count()*1e-9f;
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glCullFace(GL_BACK);
		glPolygonMode(GL_FRONT, GL_FILL);

		glUseProgram(prog);
		glUniform1f(glGetUniformLocation(prog, "time"), time);
		glUniform1f(glGetUniformLocation(prog, "tex_shift"), 0.5f/mapsize);
		glUniform1f(glGetUniformLocation(prog, "tex_scale"), 1-1.0f/mapsize);
		glProgramUniform1i(prog, glGetUniformLocation(prog, "tex"), 0);
		glEnable(GL_TEXTURE_2D);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, tex); 
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluPerspective(45, window.size.x / window.size.y, 0.01f, 10.0f);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glTranslatef(0, 0, -2);
		gluLookAt(cos(time/2), sin(time/2), 1, 0, 0, 0, 0, 0, 1);



		glBindVertexArray(vao);
		glPatchParameteri(GL_PATCH_VERTICES, 4);
		glDrawArrays(GL_PATCHES, 0, 4);

		window.redraw();
	}

	return 1;
}