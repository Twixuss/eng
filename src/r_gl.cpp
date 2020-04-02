#include "renderer.h"
#pragma warning(push, 0)
#include <Windows.h>
#include <gl\GL.h>
#pragma warning(pop)

#define GL_INFO_LOG_LENGTH                0x8B84
#define GL_FRAGMENT_SHADER                0x8B30
#define GL_VERTEX_SHADER                  0x8B31
#define GL_COMPILE_STATUS                 0x8B81
#define GL_LINK_STATUS                    0x8B82

namespace GL {

static HDC dc;
static HGLRC glrc;

struct Shader {
	GLuint program;
};

#define MAX_SHADERS 1024
static Shader shaders[MAX_SHADERS]{};

#include "renderer_exporter.inl"

#define ADD_ARG(...)						 (Renderer * this, __VA_ARGS__)
#define R_DECORATE(type, name, args, params) EXPORT type name ADD_ARG args

using GLchar = char;

struct GLProc {
	void** proc;
	char const* name;
};

std::vector<GLProc> glProcs;
struct GLProcAdder {
	GLProcAdder(void** proc, char const* name) { glProcs.push_back({proc, name}); }
};
#define GL_PROC(ret, name, ...) \
	ret (*name)(__VA_ARGS__);   \
	GLProcAdder CONCAT(_gpa_, name)((void**)&name, #name)

GL_PROC(GLuint, glCreateProgram);
GL_PROC(GLuint, glCreateShader, GLenum shaderType);
GL_PROC(void, glShaderSource, GLuint shader, GLsizei count, const GLchar** string, const GLint* length);
GL_PROC(void, glCompileShader, GLuint shader);
GL_PROC(void, glGetShaderiv, GLuint shader, GLenum pname, GLint* params);
GL_PROC(void, glAttachShader, GLuint program, GLuint shader);
GL_PROC(void, glLinkProgram, GLuint program);
GL_PROC(void, glGetProgramiv, GLuint program, GLenum pname, GLint* params);
GL_PROC(void, glDetachShader, GLuint program, GLuint shader);
GL_PROC(void, glGetShaderInfoLog, GLuint shader, GLsizei maxLength, GLsizei* length, GLchar* infoLog);

#define this renderer
R_INIT {
	dc = GetDC((HWND)this->window->handle);
	PIXELFORMATDESCRIPTOR pixelFormat{};
	pixelFormat.nSize = sizeof(pixelFormat);
	pixelFormat.nVersion = 1;
	pixelFormat.dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
	pixelFormat.cColorBits = 32;
	pixelFormat.cAlphaBits = 8;
	pixelFormat.iLayerType = PFD_MAIN_PLANE;
	auto pixelFormatIndex = ChoosePixelFormat(dc, &pixelFormat);
	DescribePixelFormat(dc, pixelFormatIndex, sizeof(pixelFormat), &pixelFormat);
	SetPixelFormat(dc, pixelFormatIndex, &pixelFormat);
	glrc = wglCreateContext(dc);
	ASSERT(wglMakeCurrent(dc, glrc));

	for (auto p : glProcs) {
		*p.proc = wglGetProcAddress(p.name);
	}
}
R_SHUTDOWN {}
R_RESIZE { glViewport(0, 0, (GLsizei)clientSize.x, (GLsizei)clientSize.y); }
R_PREPARE {}
R_RENDER {
	glClearColor(.3, .6, .9, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	SwapBuffers(dc);
}
R_CLEAR_TARGET {}
R_CREATE_SHADER {
	auto shader = std::find_if(std::begin(shaders), std::end(shaders), [](Shader& s) { return s.program == 0; });
	ASSERT(shader != std::end(shaders), "out of shaders");

	auto printLog = [](GLuint id, GLint length) {
		if (length) {
			GLchar* infoLog = (GLchar*)malloc((size_t)length);
			glGetShaderInfoLog(id, length, 0, infoLog);
			puts(infoLog);
			free(infoLog);
		}
	};

	GLint maxLength = 0;
	GLint success = 0;

	char buffer[256];
	sprintf(buffer, "%s.glsl", path);
	path = buffer;

	auto shaderSource = readFile(path);

	const GLchar* vertexSource[]{
		R"(
#version 130
#define COMPILE_VS
#line 0
)",
		shaderSource.data.data(),
	};
	GLint vertexSourceLengths[] {
		-1,
		(GLint)shaderSource.data.size(),
	};
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, _countof(vertexSource), vertexSource, vertexSourceLengths);
	glCompileShader(vertexShader);
	glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &maxLength);
	printLog(vertexShader, maxLength);
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
	ASSERT(success);

	const GLchar* fragmentSource[]{
		R"(
#version 130
#define COMPILE_PS
#line 0
)",
		shaderSource.data.data(),
	};
	GLint fragmentSourceLengths[] {
		-1,
		(GLint)shaderSource.data.size(),
	};
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, _countof(fragmentSource), fragmentSource, fragmentSourceLengths);
	glCompileShader(fragmentShader);
	glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &maxLength);
	printLog(fragmentShader, maxLength);
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
	ASSERT(success);

	shaderSource.release();

	auto program = glCreateProgram();

	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);

	glLinkProgram(program);

	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
	printLog(program, maxLength);
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	ASSERT(success);

	glDetachShader(program, vertexShader);
	glDetachShader(program, fragmentShader);

	shader->program = program;
	return {(u32)(shader - std::begin(shaders))};
}
R_CREATE_RT { return {}; }
R_CREATE_TEXTURE { return {}; }
R_CREATE_BUFFER { return {}; }
R_UPDATE_BUFFER {}
R_BIND_RT {}
R_BIND_RT_AS_TEXTURE {}
R_BIND_TEXTURE {}
R_BIND_BUFFER {}
R_BIND_SHADER {}
R_RELEASE_SHADER {}
R_RELEASE_RT {}
R_SET_MATRIX {}
R_SET_BLEND {}
R_DRAW {}
R_FILL_RENDER_TARGET {}
} // namespace GL