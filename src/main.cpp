#include <oui_window.h>
#include <oui_debug.h>

#include <GL/glew.h>

#include <gsl-lite.hpp>

#include <array>
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
class Map;
template <class T>
class Location;

class Offset
{
	template <class T>
	friend class Map;
	template <class T>
	friend class Location;
	short _val;
	constexpr Offset(short val) : _val(val) { }
public:

	Offset operator+() const { return { +_val }; }
	Offset operator-() const { return { -_val }; }

	constexpr friend Offset operator+(Offset a, Offset b) { return { a._val + b._val }; }
	constexpr friend Offset operator-(Offset a, Offset b) { return { a._val - b._val }; }

	constexpr friend Offset operator*(short c, Offset o) { return { o._val * c }; }
	constexpr friend Offset operator*(Offset o, short c) { return { o._val * c }; }

	template <class T> Location<T> operator+(Location<T> l) const { return { l._it + _val }; }
};

template <class T>
class Location
{
	friend class Offset;
	friend class Map<T>;
	friend class Map<std::remove_const_t<T>>;
	using iterator = T*;
	iterator _it;

	Location(iterator it) : _it(std::move(it)) { }
public:

	T& operator[](Offset o) const { return _it[o._val]; }
	T& operator*()  const { return *_it; }
	T* operator->() const { return &*_it; }

	Location& operator+=(Offset o) { _it += o._val; return *this; }

	bool operator< (const Location& that) const { return _it < that._it; }
	bool operator<=(const Location& that) const { return _it <= that._it; }
	bool operator!=(const Location& that) const { return _it != that._it; }
	bool operator==(const Location& that) const { return _it == that._it; }
	bool operator>=(const Location& that) const { return _it >= that._it; }
	bool operator> (const Location& that) const { return _it > that._it; }

	Location operator+(Offset o) const { return { _it + o._val }; }
	Location operator-(Offset o) const { return { _it - o._val }; }
};


template <class T>
class Map
{
	std::vector<T> _data;
public:
	Offset row;

	static constexpr Offset col = { 1 };

	Map(short width, short height) : row(width) { _data.resize(int(width)*height); }

	short width() const { return row._val; }
	short height() const { return static_cast<short>(_data.size() / row._val); }

	using iterator = T*;


	Location<T>       operator()(short x, short y)       { return _data.data() + (x + row._val * y); }
	Location<const T> operator()(short x, short y) const { return _data.data() + (x + row._val * y); }


	Location<T> begin() { return _data.data(); }
	Location<T> end() { return _data.data() + _data.size(); }
	Location<const T> begin() const { return _data.data(); }
	Location<const T> end()   const { return _data.data() + _data.size(); }
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

inline ushort cubic_midpoint(Location<ushort> center, Offset incr)
{
	unsigned sum = center[-incr] + center[+incr];
	sum += sum << 3; // sum *= 9
	sum -= center[-3 * incr] + center[+3 * incr];
	return static_cast<ushort>(sum >> 4);
}
inline ushort quadratic_endpoint(Location<ushort> center, Offset incr)
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

Map<ushort> double_map(const Map<ushort>& src, double strength)
{
	Map<ushort> out(src.width() * 2 - 1, src.height() * 2 - 1);
	auto ot = out.begin();
	for (auto it = src.begin(); it < src.end(); it += src.row, ot += 2 * out.row)
	{
		auto pt = ot;
		for (auto jt = it; jt < it + src.row; jt += src.col, pt += 2 * out.col)
		{
			*pt = *jt;
		}
	}

	for (auto it = out.begin(); it < out.end(); it += 2 * out.row)
	{
		it[out.col] = quadratic_endpoint(it + out.col, out.col);
		for (auto jt = it + 3 * out.col; jt < it + out.row - 2 * out.col; jt += 2 * out.col)
			*jt = cubic_midpoint(jt, out.col);
		it[out.row - 2 * out.col] = quadratic_endpoint(it + out.row - 2 * out.col, -out.col);
	}
	for (auto it = out.begin(); it < out.begin() + out.row; it += out.col)
		it[out.row] = quadratic_endpoint(it + out.row, out.row);
	for (auto it = out.end() - out.row; it < out.end(); it += out.col)
		it[-out.row] = quadratic_endpoint(it - out.row, -out.row);
	for (auto it = out.begin() + 3 * out.row; it < out.end() - 2 * out.row; it += 2 * out.row)
		for (auto jt = it; jt < it + out.row; jt += out.col)
			*jt = cubic_midpoint(jt, out.row);
	//it[out.col - 2 * out.row] = quadratic_endpoint(it + out.col - 2 * out.row, -out.row);
	for (auto it = out.begin(); it < out.end(); it += out.row)
		for (auto jt = it; jt < it + out.row; jt += out.col)
			*jt = nudge(*jt, strength);
	return out;
}

constexpr auto absdiff(ushort a, ushort b) 
{ 
	const int diff = int((12 + a - b) & 7) - 4;
	return diff < 0 ? -diff : diff;
};
void graph_mapgen(Map<ushort>& out, double strength)
{
	static constexpr ushort sink = 8;
	static constexpr ushort no_neighbor = 16;
	struct GridNode
	{
		ushort heigth;
		ushort neighbor = no_neighbor;
	};
	Map<GridNode> graph(out.width(), out.height());

	struct Coord
	{
		short x;
		short y;

		Coord operator+(Coord b) const { return { x + b.x, y + b.y }; }

		[[nodiscard]] constexpr bool operator==(Coord b) const { return x == b.x && y == b.y; }
		[[nodiscard]] constexpr bool operator!=(Coord b) const { return x != b.x || y != b.y; }
	};

	constexpr Coord neighbors[8]
	{
	{ -1, -1 },	{ -1, +0 },	{ -1, +1 }, { +0, +1 },
	{ +1, +1 }, { +1, +0 }, { +1, -1 }, { +0, -1 }
	};

	double next_height = 0;
	strength *= 0.0005;

	std::vector<Coord> agenda;

	Coord cur;

	const auto add_open_neighbors_to_agenda = [&](Coord co)
	{
		size_t added = 0;
		auto lo = graph(co.x, co.y);
		for (int i = 0; i < 8; ++i)
		{
			const auto nco = co + neighbors[i];
			if (nco.x < 0 || nco.y < 0 || nco.x >= graph.width() || nco.y >= graph.height())
				continue;
			const auto nlo = graph(nco.x, nco.y);
			if (nlo->neighbor == no_neighbor)
			{
				nlo->neighbor = (i + 4) & 7;
				nlo->heigth = ushort(next_height);
				agenda.push_back(nco);
				++added;
			}
		}
		return added;
	};

	for (int i = 0; i < 1; ++i)
	{
		cur =
		{
			std::uniform_int<short>(0, graph.width())(rng),
			std::uniform_int<short>(0, graph.height())(rng)
		};
		*graph(cur.x, cur.y) = { 0, sink };
		add_open_neighbors_to_agenda(cur);
	}

	std::vector<size_t> roulette;
	while (!agenda.empty())
	{
		auto select = std::uniform_int<size_t>(0, agenda.size()-1)(rng);
		cur = agenda[select];
		agenda[select] = agenda.back();
		agenda.pop_back();
		add_open_neighbors_to_agenda(cur);
		//while (auto added = add_open_neighbors_to_agenda(cur))
		//{
		//	auto clo = graph(cur.x, cur.y);
		//	for (auto i = added; i > 0; --i)
		//	{
		//		const auto ai = agenda.size() - i;
		//		auto ico = agenda[ai];
		//		auto ilo = graph(ico.x, ico.y);
		//		for (auto k = 3 - absdiff(clo->neighbor, ilo->neighbor); k > 0; --k)
		//			roulette.push_back(ai);
		//	}
		//	if (roulette.empty())
		//		break;
		//	const auto clear_roulette = gsl::finally([&] { roulette.clear(); });
		//	const auto roulette_i = std::uniform_int<size_t>(0, roulette.size()-1)(rng);
		//	if (roulette_i == roulette.size())
		//		break;
		//	const auto new_select = roulette[roulette_i];
		//	strength += 0.0002;
		//	cur = agenda[new_select];
		//	agenda[new_select] = agenda.back();
		//	agenda.pop_back();
		//}

		next_height += strength;
		strength += 0.0000005;
	}

	for (short y = 0; y < graph.height(); ++y)
		for (short x = 0; x < graph.width(); ++x)
			*out(x, y) = graph(x, y)->heigth;
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

	Map<ushort> map(3, 3);

	for (auto it = map.begin(); it != map.end(); it += map.col)
		*it = 14000+std::uniform_int<ushort>(0, 12000)(rng);

	for (int i = 0; i < 9; ++i)
		map = double_map(map,3500.0*pow(0.5, i));

	const short mapsize = map.width();
	//Map<unsigned short> map(mapsize, mapsize);
	//recursive_mapgen(map, 20);
	//graph_mapgen(map, 20);

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


	const short wmapsize = mapsize;// / 2 + 1;
	auto hramp = [](ushort iz)
	{
		float z = iz / float(0x10000);
		z -= 0.2f;
		z = 15*z * z*z;
		return static_cast<ushort>(std::clamp(int((z+0.4f) * 0x10000), 0, 0xffff));
	};
	Map<Water> water(wmapsize, wmapsize);
	auto wl = water.begin();
	for (auto gr = map.begin(); gr < map.end(); gr += 1 * map.row)
		for (auto gc = gr; gc < gr + map.row; gc += 1 * map.col, wl += water.col)
			*wl = { 0x8000*0,0x8000*0, 200, hramp(*gc) };//hramp(*gc) };

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
	GLuint erode_tex[2];
	glGenTextures(2, erode_tex);
	glBindTexture(tex_target, erode_tex[0]);
	glTexParameteri(tex_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(tex_target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
	glTexImage2D(tex_target, 0, GL_RG32F, wmapsize, wmapsize, 0, GL_RGBA, GL_UNSIGNED_SHORT, 0);
	glTexParameterfv(tex_target, GL_TEXTURE_BORDER_COLOR, water_border);
	glBindTexture(tex_target, erode_tex[1]);
	glTexParameteri(tex_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(tex_target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
	glTexImage2D(tex_target, 0, GL_RG32F, wmapsize, wmapsize, 0, GL_RGBA, GL_UNSIGNED_SHORT, 0);
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
			GLenum bufs[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };

			auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

			glViewport(0, 0, wmapsize, wmapsize);
			glDisable(GL_CULL_FACE);
			glDisable(GL_DEPTH_TEST);
			glBlendFunc(GL_ONE, GL_ZERO);

			auto aprog = key == oui::Key::a ? flow_prog : erode_prog;

			glUseProgram(aprog);
			glUniform1f(glGetUniformLocation(aprog, "dt"), 0.01);
			glUniform1f(glGetUniformLocation(aprog, "rdx"), 10.0);
			glUniform1f(glGetUniformLocation(aprog, "rdy"), 10.0);

			for (auto i = 0; i < (aprog == flow_prog ? 16 : 1); ++i)
			{
				glBindFramebuffer(GL_FRAMEBUFFER, water_fb);
				glDrawBuffers(2, bufs);
				glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, water_tex[1], 0);
				glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, erode_tex[1], 0);

				glActiveTexture(GL_TEXTURE1);
				glBindTexture(tex_target, erode_tex[0]);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(tex_target, water_tex[0]);

				glDrawArrays(GL_QUADS, 0, 4);
				glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 0, 0);
				glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, 0, 0);
				std::swap(water_tex[0], water_tex[1]);
				std::swap(erode_tex[0], erode_tex[1]);
			}

			glDrawBuffer(GL_COLOR_ATTACHMENT1);
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
		glEnable(GL_DEPTH_TEST);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluPerspective(45, window.size.x / window.size.y, 0.1f, 10.0f);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		gluLookAt(cos(1+0*time/10)*2, sin(1+0*time/10)*2, 2, 0, 0, 0, 0, 0, 1);

		glUseProgram(prog);
		glProgramUniform1i(prog, glGetUniformLocation(prog, "tex"), 0);
		glBindTexture(tex_target, tex);
		
		glDrawArrays(GL_PATCHES, 0, 4);

		glUseProgram(water_prog);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(tex_target, erode_tex[0]);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(tex_target, water_tex[0]);


		glDrawArrays(GL_PATCHES, 0, 4);

		window.redraw();
	}

	return 1;
}