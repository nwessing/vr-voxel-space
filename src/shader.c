#include "stdint.h"
#include "glad/glad.h"

static int32_t check_shader_compile_errors(uint32_t shader)
{
    int32_t success;

    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(shader, 512, NULL, info_log);
        printf("ERROR::SHADER::COMPILATION_FAILURE\n%s\n", info_log);
    }

    return success;
}

static int32_t check_program_link_errors(uint32_t program)
{
    int32_t success;
    char info_log[512];

    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, NULL, info_log);
        printf("ERROR:PROGRAM::LINK_FAILURE\n%s\n", info_log);
    }

    return success;
}

uint32_t compile_shader(int32_t shader_type, const char *shader_source)
{
    uint32_t shader = glCreateShader(shader_type);
    glShaderSource(shader, 1, &shader_source, NULL);
    glCompileShader(shader);
    if (!check_shader_compile_errors(shader)) {
        return 0;
    }

    return shader;
}


uint32_t create_shader(const char *vertex_source, const char *fragment_source)
{
    uint32_t vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_source);
    if (!vertex_shader) {
        return 0;
    }

    uint32_t fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    if (!fragment_shader) {
        return 0;
    }

    uint32_t shader_program = glCreateProgram();
    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);
    glLinkProgram(shader_program);

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    if (!check_program_link_errors(shader_program)) {
        return 0;
    }

    return shader_program;
}