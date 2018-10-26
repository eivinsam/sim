#include <oui_window.h>
#include <oui_debug.h>

#include <GL/glew.h>

#include <vector>
#include <random>

#include <string>
#include <fstream>

std::string read_file(const char* filename)
{
	std::string result;
	std::ifstream in(filename, std::ios::binary);

	in.seekg(0, std::ios::end);
	result.resize(static_cast<size_t>(in.tellg()));
	in.seekg(0);
	in.read(result.data(), result.size());
	return result;
}

template <class T>
class Map
{
	std::vector<T> _data;
public:
	class Location;
	class Offset
	{
		friend class Map<T>;
		friend class Map<T>::Location;
		short _val;
		constexpr Offset(short val) : _val(val) { }
	public:

		Offset operator+() const { return { +_val }; }
		Offset operator-() const { return { -_val }; }

		constexpr friend Offset operator+(Offset a, Offset b) { return { a._val + b._val }; }
		constexpr friend Offset operator-(Offset a, Offset b) { return { a._val - b._val }; }

		constexpr friend Offset operator*(short c, Offset o) { return { o._val * c }; }
		constexpr friend Offset operator*(Offset o, short c) { return { o._val * c }; }

		friend Location operator+(Location l, Offset o);
		friend Location operator+(Offset o, Location l);
		friend Location operator-(Location l, Offset o);
	};
	Offset row;

	static constexpr Offset col = { 1 };

	Map(short width, short height) : row(width) { _data.resize(int(width)*height); }

	short width() const { return row._val; }
	short height() const { return static_cast<short>(_data.size() / row._val); }

	using iterator = T*;

	class Location
	{
		friend class Map<T>;
		iterator _it;

		Location(iterator it) : _it(std::move(it)) { }
	public:

		decltype(auto) operator[](Offset o) { return _it[o._val]; }
		decltype(auto) operator*() { return *_it; }
		decltype(auto) operator->() { return &*_it; }

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
GLuint newShader(GLenum type, const std::string& text)
{
	return newShader(type, text.c_str(), static_cast<int>(text.size()));
}

GLuint setupShaders()
{
	GLuint prog = glCreateProgram();

	glAttachShader(prog, newShader(GL_VERTEX_SHADER,          read_file("shaders/terrain.vert")));
	glAttachShader(prog, newShader(GL_TESS_CONTROL_SHADER,    read_file("shaders/terrain.tesc")));
	glAttachShader(prog, newShader(GL_TESS_EVALUATION_SHADER, read_file("shaders/terrain.tese")));
	glAttachShader(prog, newShader(GL_FRAGMENT_SHADER,        read_file("shaders/terrain.frag")));

	glLinkProgram(prog);

	char buffer[1024];
	glGetProgramInfoLog(prog, sizeof(buffer), 0, buffer);
	oui::debug::println("shader linking:\n", buffer);

	return prog;
}
GLuint setupFlowShaders()
{
	GLuint prog = glCreateProgram();

	glAttachShader(prog, newShader(GL_VERTEX_SHADER,   read_file("shaders/flow.vert")));
	glAttachShader(prog, newShader(GL_FRAGMENT_SHADER, read_file("shaders/flow.frag")));

	glLinkProgram(prog);

	char buffer[1024];
	glGetProgramInfoLog(prog, sizeof(buffer), 0, buffer);
	oui::debug::println("shader linking:\n", buffer);

	return prog;
}
GLuint setupErodeShaders()
{
	GLuint prog = glCreateProgram();

	glAttachShader(prog, newShader(GL_VERTEX_SHADER,   read_file("shaders/erode.vert")));
	glAttachShader(prog, newShader(GL_FRAGMENT_SHADER, read_file("shaders/erode.frag")));

	glLinkProgram(prog);

	char buffer[1024];
	glGetProgramInfoLog(prog, sizeof(buffer), 0, buffer);
	oui::debug::println("shader linking:\n", buffer);

	return prog;
}
GLuint setupWaterShaders()
{
	GLuint prog = glCreateProgram();

	glAttachShader(prog, newShader(GL_VERTEX_SHADER,          read_file("shaders/water.vert")));
	glAttachShader(prog, newShader(GL_TESS_CONTROL_SHADER,    read_file("shaders/water.tesc")));
	glAttachShader(prog, newShader(GL_TESS_EVALUATION_SHADER, read_file("shaders/water.tese")));
	glAttachShader(prog, newShader(GL_FRAGMENT_SHADER,        read_file("shaders/water.frag")));

	glLinkProgram(prog);

	char buffer[1024];
	glGetProgramInfoLog(prog, sizeof(buffer), 0, buffer);
	oui::debug::println("shader linking:\n", buffer);

	return prog;
}

static std::mt19937 rng;

using ushort = unsigned short;

inline ushort cubic_midpoint(Map<ushort>::Location center, Map<ushort>::Offset incr)
{
	unsigned sum = center[-incr] + center[+incr];
	sum += sum << 3; // sum *= 9
	sum -= center[-3 * incr] + center[+3 * incr];
	return static_cast<ushort>(sum >> 4);
}
inline ushort quadratic_endpoint(Map<ushort>::Location center, Map<ushort>::Offset incr)
{
	unsigned sum = (unsigned(center[incr])<<1) + center[-incr];
	sum += sum << 1; // sum *= 3
	sum -= center[3 * incr];
	return static_cast<ushort>(sum >> 3);
}

ushort nudge(ushort x, double strength)
{
	return static_cast<ushort>(std::clamp(x + std::normal_distribution(0.0, strength)(rng), 0.0, double(0xffff)));
	//return static_cast<ushort>(std::clamp(x + std::exp(std::normal_distribution(log(strength), 0.5)(rng)), 0.0, 65000.0));
}

void recursive_mapgen(Map<ushort>& out, double strength)
{
	if (out.width() < 5)
	{
		for (auto it = out.begin(); it < out.end(); it += out.row)
			for (auto jt = it; jt < it + out.row; jt += out.col)
				*jt = nudge(0x8000, strength);
	}
	else
	{
		Map<unsigned short> sub(out.width() / 2 + 1, out.height() / 2 + 1);
		recursive_mapgen(sub, strength*2);
		for (auto it = sub.begin(), ot = out.begin(); it < sub.end(); it += sub.row, ot += 2*out.row)
			for (auto jt = it, pt = ot; jt < it + sub.row; jt += sub.col, pt += 2*out.col)
			{
				*pt = *jt;
			}

		for (auto it = out.begin(); it < out.end(); it += 2 * out.row)
		{
			it[out.col] = quadratic_endpoint(it+out.col, out.col);
			for (auto jt = it + 3 * out.col; jt < it + out.row - 2 * out.col; jt += 2 * out.col)
				*jt = cubic_midpoint(jt, out.col);
			it[out.row - 2*out.col] = quadratic_endpoint(it + out.row - 2*out.col, -out.col);
		}
		for (auto it = out.begin(); it < out.begin() + out.row; it += out.col)
			it[out.row] = quadratic_endpoint(it + out.row, out.row);
		for (auto it = out.end() - out.row; it < out.end(); it += out.col)
			it[-out.row] = quadratic_endpoint(it-out.row, -out.row);
		for (auto it = out.begin() + 3 * out.row; it < out.end() - 2 * out.row; it += 2 * out.row)
			for (auto jt = it; jt < it + out.row; jt += out.col)
				*jt = cubic_midpoint(jt, out.row);
			//it[out.col - 2 * out.row] = quadratic_endpoint(it + out.col - 2 * out.row, -out.row);
		for (auto it = out.begin(); it < out.end(); it += out.row)
			for (auto jt = it; jt < it + out.row; jt += out.col)
				*jt = nudge(*jt, strength);
	}
}

template <class T, void (*G)(GLenum, T*)>
T gl_get(GLenum key) { T result; G(key, &result); return result; }

__declspec(noinline) auto glerror() { return glGetError(); }

int main()
{
	auto window = oui::Window{ { "SIM" } };

	oui::debug::println("max texture size: ", gl_get<GLint, glGetIntegerv>(GL_MAX_TEXTURE_SIZE));

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

	const auto prog = setupShaders();
	const auto flow_prog = setupFlowShaders();
	const auto erode_prog = setupErodeShaders();
	const auto water_prog = setupWaterShaders();

	constexpr auto tex_target = GL_TEXTURE_RECTANGLE;

	short mapsize = 513;
	Map<unsigned short> map(mapsize, mapsize);
	recursive_mapgen(map, 20);

	GLuint tex; 
	glGenTextures(1, &tex);
	glBindTexture(tex_target, tex);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexParameteri(tex_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(tex_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(tex_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(tex_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(tex_target, 0, GL_R16, mapsize, mapsize, 0, GL_RED, GL_UNSIGNED_SHORT, &*map.begin());

	struct Water
	{
		ushort u;
		ushort v;
		ushort h;
		ushort b;
	};


	const short wmapsize = mapsize / 2 + 1;
	auto hramp = [](ushort iz)
	{
		float z = iz / float(0x10000);
		z -= 0.4f;
		z = 15*z * z*z;
		return static_cast<ushort>(std::clamp(int((z+0.4f) * 0x10000), 0, 0xffff));
	};
	Map<Water> water(wmapsize, wmapsize);
	auto wl = water.begin();
	for (auto gr = map.begin(); gr < map.end(); gr += 2 * map.row)
		for (auto gc = gr; gc < gr + map.row; gc += 2 * map.col, wl += water.col)
			*wl = { 0x8000,0x8000, 100, hramp(*gc) };

	//water(wmapsize / 2 + 1, wmapsize / 2 + 1)->h = 10000;

	float water_border[] = { 0.5, 0.5, 0.3, 0 };

	GLuint water_tex[2];
	glGenTextures(2, water_tex);
	glBindTexture(tex_target, water_tex[0]);
	glTexParameteri(tex_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(tex_target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
	glTexImage2D(tex_target, 0, GL_RGBA32F, wmapsize, wmapsize, 0, GL_RGBA, GL_UNSIGNED_SHORT, &*water.begin());
	glTexParameterfv(tex_target, GL_TEXTURE_BORDER_COLOR, water_border);
	glBindTexture(tex_target, water_tex[1]);
	glTexParameteri(tex_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(tex_target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
	glTexImage2D(tex_target, 0, GL_RGBA32F, wmapsize, wmapsize, 0, GL_RGBA, GL_UNSIGNED_SHORT, 0);
	glTexParameterfv(tex_target, GL_TEXTURE_BORDER_COLOR, water_border);

	GLuint water_fb;
	glGenFramebuffers(1, &water_fb);

	

	glClearColor(0, 0, 0, 0);

	auto start = oui::now();

	glEnable(GL_CULL_FACE);
	glDepthFunc(GL_LESS);

	glDisable(GL_BLEND);

	oui::input.keydown = [&](auto key, auto prevstate)
	{
		if (key == oui::Key::a || key == oui::Key::e)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, water_fb);
			glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, water_tex[1], 0);

			auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

			glViewport(0, 0, wmapsize, wmapsize);
			glDisable(GL_CULL_FACE);
			glDisable(GL_DEPTH_TEST);
			glBindTexture(tex_target, water_tex[0]);
			glBlendFunc(GL_ONE, GL_ZERO);

			auto aprog = key == oui::Key::a ? flow_prog : erode_prog;

			glUseProgram(aprog);
			glUniform1f(glGetUniformLocation(aprog, "dt"), 0.05);
			glUniform1f(glGetUniformLocation(aprog, "rdx"), 1.0);
			glUniform1f(glGetUniformLocation(aprog, "rdy"), 1.0);

			glDrawArrays(GL_QUADS, 0, 4);

			glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 0, 0);

			std::swap(water_tex[0], water_tex[1]);
			glEnable(GL_DEPTH_TEST);
			glEnable(GL_CULL_FACE);
			glViewport(0, 0, window.size.x, window.size.y);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}
	};

	glActiveTexture(GL_TEXTURE0);

	glBindVertexArray(vao);
	glPatchParameteri(GL_PATCH_VERTICES, 4);

	while (window.update())
	{
		auto time = (oui::now() - start).count()*1e-9f;
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glCullFace(GL_BACK);
		glPolygonMode(GL_FRONT, GL_FILL);

		glEnable(tex_target);
		//glEnable(GL_FRAMEBUFFER_SRGB);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluPerspective(45, window.size.x / window.size.y, 0.1f, 10.0f);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		gluLookAt(cos(1+0*time/5)*2, sin(1+0*time/5)*2, 2, 0, 0, 0, 0, 0, 1);

		glUseProgram(prog);
		glProgramUniform1i(prog, glGetUniformLocation(prog, "tex"), 0);
		glBindTexture(tex_target, tex);
		
		glDrawArrays(GL_PATCHES, 0, 4);

		glUseProgram(water_prog);
		glProgramUniform1i(water_prog, glGetUniformLocation(water_prog, "tex"), 0);
		glBindTexture(tex_target, water_tex[0]);

		glDrawArrays(GL_PATCHES, 0, 4);

		//glDisable(GL_FRAMEBUFFER_SRGB);

		window.redraw();
	}

	return 1;
}