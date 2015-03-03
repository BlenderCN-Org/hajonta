#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#if defined(NEEDS_EGL)
#include <EGL/egl.h>
#elif defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif

#include "hajonta/platform/common.h"
#include "hajonta/math.cpp"
#include "hajonta/image.cpp"

#include "hajonta/programs/b.h"

inline void
glErrorAssert()
{
    GLenum error = glGetError();
    switch(error)
    {
        case GL_NO_ERROR:
        {
            return;
        } break;
        case GL_INVALID_ENUM:
        {
            hassert(!"Invalid enum");
        } break;
        case GL_INVALID_VALUE:
        {
            hassert(!"Invalid value");
        } break;
        case GL_INVALID_OPERATION:
        {
            hassert(!"Invalid operation");
        } break;
#if !defined(_WIN32)
        case GL_INVALID_FRAMEBUFFER_OPERATION:
        {
            hassert(!"Invalid framebuffer operation");
        } break;
#endif
        case GL_OUT_OF_MEMORY:
        {
            hassert(!"Out of memory");
        } break;
#if !defined(__APPLE__) && !defined(NEEDS_EGL)
        case GL_STACK_UNDERFLOW:
        {
            hassert(!"Stack underflow");
        } break;
        case GL_STACK_OVERFLOW:
        {
            hassert(!"Stack overflow");
        } break;
#endif
        default:
        {
            hassert(!"Unknown error");
        } break;
    }
}

struct vertex
{
    float position[4];
    float color[4];
};

struct vertex_with_style
{
    vertex v;
    float style[4];
};

struct face_index
{
    uint32_t vertex;
    uint32_t texture_coord;
};

struct face
{
    face_index indices[3];
    int texture_offset;
};

struct material
{
    char name[100];
    int32_t texture_offset;
};

struct game_state
{
    uint32_t vao;
    uint32_t vbo;
    uint32_t ibo;
    uint32_t line_ibo;
    int32_t sampler_ids[4];
    uint32_t texture_ids[10];
    uint32_t num_texture_ids;

    float near_;
    float far_;
    float delta_t;

    loaded_file model_file;
    loaded_file mtl_file;

    char *objfile;
    uint32_t objfile_size;

    v3 vertices[10000];
    uint32_t num_vertices;
    v3 texture_coords[10000];
    uint32_t num_texture_coords;
    face faces[10000];
    uint32_t num_faces;

    material materials[10];
    uint32_t num_materials;

    GLushort faces_array[30000];
    uint32_t num_faces_array;
    GLushort line_elements[60000];
    uint32_t num_line_elements;
    vertex_with_style vbo_vertices[30000];
    uint32_t num_vbo_vertices;

    b_program_struct program_b;

    v3 model_max;
    v3 model_min;

    char bitmap_scratch[2048 * 2048 * 4];

    bool hide_lines;
};

bool
gl_setup(hajonta_thread_context *ctx, platform_memory *memory)
{
    glErrorAssert();
    game_state *state = (game_state *)memory->memory;

#if !defined(NEEDS_EGL)
    if (glGenVertexArrays != 0)
    {
        glGenVertexArrays(1, &state->vao);
        glBindVertexArray(state->vao);
        glErrorAssert();
    }
#endif

    glErrorAssert();

    bool loaded;

    loaded = b_program(&state->program_b, ctx, memory);
    if (!loaded)
    {
        return loaded;
    }

    return true;
}

bool
load_mtl(hajonta_thread_context *ctx, platform_memory *memory)
{
    game_state *state = (game_state *)memory->memory;


    char *position = (char *)state->mtl_file.contents;
    uint32_t max_lines = 100000;
    uint32_t counter = 0;
    material *current_material = 0;
    for (;;)
    {
        char *newline = strchr(position, '\n');
        char *returnnewline = strchr(position, '\r');
        char *nul = strchr(position, '\0');
        char *eol = newline;
        if (returnnewline && eol > returnnewline)
        {
            eol = returnnewline;
        }
        if (nul && eol > nul)
        {
            eol = nul;
        }
        if (!eol)
        {
            break;
        }
        if (eol > (state->mtl_file.contents + state->mtl_file.size))
        {
            eol = state->mtl_file.contents + state->mtl_file.size;
        }
        char line[1024];
        strncpy(line, position, (size_t)(eol - position));
        line[eol - position] = '\0';
        /*
        char msg[1024];
        sprintf(msg, "position: %d; eol: %d; line: %s\n", position - state->mtl_file.contents, eol - position, line);
        memory->platform_debug_message(ctx, msg);
        */

        if (line[0] == '\0')
        {

        }
        else if (line[0] == '#')
        {

        }
        else if (strncmp(line, "newmtl", 6) == 0)
        {
            current_material = state->materials + state->num_materials++;
            strncpy(current_material->name, line + 7, (size_t)(eol - position - 7));
            current_material->texture_offset = -1;
        }
        else if (strncmp(line, "Ns", 2) == 0)
        {
        }
        else if (strncmp(line, "Ka", 2) == 0)
        {
        }
        else if (strncmp(line, "Kd", 2) == 0)
        {
        }
        else if (strncmp(line, "Ks", 2) == 0)
        {
        }
        else if (strncmp(line, "Ni", 2) == 0)
        {
        }
        else if (strncmp(line, "d ", 2) == 0)
        {
        }
        else if (strncmp(line, "illum ", sizeof("illum ") - 1) == 0)
        {
        }
        else if (strncmp(line, "map_Bump ", sizeof("map_Bump ") - 1) == 0)
        {
        }
        else if (strncmp(line, "map_Kd ", sizeof("map_Kd ") - 1) == 0)
        {
            char *filename = line + sizeof("map_Kd ") - 1;
            loaded_file texture;
            bool loaded = memory->platform_editor_load_nearby_file(ctx, &texture, state->mtl_file, filename);
            hassert(loaded);
            int32_t x, y, size;
            load_image((uint8_t *)texture.contents, texture.size, (uint8_t *)state->bitmap_scratch, sizeof(state->bitmap_scratch), &x, &y, &size, false);
            /*
            char msg[1024];
            sprintf(msg, "name = %s; x = %i, y = %i, size = %i", filename, x, y, size);
            memory->platform_debug_message(ctx, msg);
            */

            current_material->texture_offset = (int32_t)(state->num_texture_ids++);
            glBindTexture(GL_TEXTURE_2D, state->texture_ids[current_material->texture_offset]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                x, y, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, state->bitmap_scratch);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        }
        else
        {
            hassert(!"Invalid code path");
        }
        if (counter++ > max_lines)
        {
            hassert(!"Too many lines in mtl file");
        }

        if (*eol == '\0')
        {
            break;
        }
        position = eol + 1;
        while((*position == '\r') && (*position == '\n'))
        {
            position++;
        }
    }
    return true;
}

extern "C" GAME_UPDATE_AND_RENDER(game_update_and_render)
{
    game_state *state = (game_state *)memory->memory;

    static uint32_t last_active_demo = UINT32_MAX;

#if !defined(NEEDS_EGL) && !defined(__APPLE__)
    if (!glCreateProgram)
    {
        load_glfuncs(ctx, memory->platform_glgetprocaddress);
    }
#endif

    if (!memory->initialized)
    {
        if(!gl_setup(ctx, memory))
        {
            return;
        }
        memory->initialized = 1;

        state->near_ = {5.0f};
        state->far_ = {50.0f};


        glErrorAssert();
        glGenBuffers(1, &state->vbo);
        glGenBuffers(1, &state->ibo);
        glGenBuffers(1, &state->line_ibo);
        glErrorAssert();
        glGenTextures(harray_count(state->texture_ids), state->texture_ids);
        glErrorAssert();
        state->sampler_ids[0] = glGetUniformLocation(state->program_b.program, "tex");
        state->sampler_ids[1] = glGetUniformLocation(state->program_b.program, "tex1");
        state->sampler_ids[2] = glGetUniformLocation(state->program_b.program, "tex2");
        state->sampler_ids[3] = glGetUniformLocation(state->program_b.program, "tex3");
        glErrorAssert();

        while (!memory->platform_editor_load_file(ctx, &state->model_file))
        {
        }

        char *position = (char *)state->model_file.contents;
        uint32_t max_lines = 100000;
        uint32_t counter = 0;
        material *current_material = 0;
        for (;;)
        {
            char *newline = strchr(position, '\n');
            char *returnnewline = strchr(position, '\r');
            char *nul = strchr(position, '\0');
            char *eol = newline;
            if (returnnewline && eol > returnnewline)
            {
                eol = returnnewline;
            }
            if (nul && eol > nul)
            {
                eol = nul;
            }
            if (!eol)
            {
                break;
            }
            if (eol > (state->model_file.contents + state->model_file.size))
            {
                eol = state->model_file.contents + state->model_file.size;
            }
            char line[1024];
            strncpy(line, position, (size_t)(eol - position));
            line[eol - position] = '\0';
            /*
            char msg[1024];
            sprintf(msg, "position: %d; eol: %d; line: %s\n", position - state->model_file.contents, eol - position, line);
            memory->platform_debug_message(ctx, msg);
            */

            if (line[0] == '\0')
            {
            }
            else if (line[0] == 'v')
            {
                if (line[1] == 't')
                {
                    float a, b, c;
                    int num_found = sscanf(position + 2, "%f %f %f", &a, &b, &c);
                    if (num_found == 3)
                    {
                        v3 texture_coord = {a,b,c};
                        state->texture_coords[state->num_texture_coords++] = texture_coord;
                    }
                    else if (num_found == 2)
                    {
                        v3 texture_coord = {a, b, 0.0f};
                        state->texture_coords[state->num_texture_coords++] = texture_coord;
                    }
                    else
                    {
                        hassert(!"Invalid code path");
                    }
                }
                else if (line[1] == 'n')
                {

                }
                else if (line[1] == ' ')
                {
                    float a, b, c;
                    if (sscanf(line + 2, "%f %f %f", &a, &b, &c) == 3)
                    {
                        if (state->num_vertices == 0)
                        {
                            state->model_max = {a, b, c};
                            state->model_min = {a, b, c};
                        }
                        else
                        {
                            if (a > state->model_max.x)
                            {
                                state->model_max.x = a;
                            }
                            if (b > state->model_max.y)
                            {
                                state->model_max.y = b;
                            }
                            if (c > state->model_max.z)
                            {
                                state->model_max.z = c;
                            }
                            if (a < state->model_min.x)
                            {
                                state->model_min.x = a;
                            }
                            if (b < state->model_min.y)
                            {
                                state->model_min.y = b;
                            }
                            if (c < state->model_min.z)
                            {
                                state->model_min.z = c;
                            }
                        }
                        v3 vertex = {a,b,c};
                        state->vertices[state->num_vertices++] = vertex;
                    }
                }
                else
                {
                    hassert(!"Invalid code path");
                }
            }
            else if (line[0] == 'f')
            {
                char a[100], b[100], c[100], d[100];
                int num_found = sscanf(line + 2, "%s %s %s %s", &a, &b, &c, &d);
                hassert((num_found == 4) || (num_found == 3));
                if (num_found == 4)
                {
                    uint32_t t1, t2;
                    int num_found2 = sscanf(a, "%d/%d", &t1, &t2);
                    hassert(num_found2 == 2);
                    if (num_found2 == 2)
                    {
                        uint32_t a_vertex_id, a_texture_coord_id;
                        uint32_t b_vertex_id, b_texture_coord_id;
                        uint32_t c_vertex_id, c_texture_coord_id;
                        uint32_t d_vertex_id, d_texture_coord_id;
                        int num_found2;
                        num_found2 = sscanf(a, "%d/%d", &a_vertex_id, &a_texture_coord_id);
                        hassert(num_found2 == 2);
                        num_found2 = sscanf(b, "%d/%d", &b_vertex_id, &b_texture_coord_id);
                        hassert(num_found2 == 2);
                        num_found2 = sscanf(c, "%d/%d", &c_vertex_id, &c_texture_coord_id);
                        hassert(num_found2 == 2);
                        num_found2 = sscanf(d, "%d/%d", &d_vertex_id, &d_texture_coord_id);
                        hassert(num_found2 == 2);
                        face face1 = {
                            {
                                {a_vertex_id, a_texture_coord_id},
                                {b_vertex_id, b_texture_coord_id},
                                {c_vertex_id, c_texture_coord_id},
                            },
                            current_material->texture_offset,
                        };
                        face face2 = {
                            {
                                {c_vertex_id, c_texture_coord_id},
                                {d_vertex_id, d_texture_coord_id},
                                {a_vertex_id, a_texture_coord_id},
                            },
                            current_material->texture_offset,
                        };
                        state->faces[state->num_faces++] = face1;
                        state->faces[state->num_faces++] = face2;
                    }
                }
                else if (num_found == 3)
                {
                    uint32_t t1, t2;
                    int num_found2 = sscanf(a, "%d/%d", &t1, &t2);
                    hassert(num_found2 == 2);
                    if (num_found2 == 2)
                    {
                        uint32_t a_vertex_id, a_texture_coord_id;
                        uint32_t b_vertex_id, b_texture_coord_id;
                        uint32_t c_vertex_id, c_texture_coord_id;
                        int num_found2;
                        num_found2 = sscanf(a, "%d/%d", &a_vertex_id, &a_texture_coord_id);
                        hassert(num_found2 == 2);
                        num_found2 = sscanf(b, "%d/%d", &b_vertex_id, &b_texture_coord_id);
                        hassert(num_found2 == 2);
                        num_found2 = sscanf(c, "%d/%d", &c_vertex_id, &c_texture_coord_id);
                        hassert(num_found2 == 2);
                        face face1 = {
                            {
                                {a_vertex_id, a_texture_coord_id},
                                {b_vertex_id, b_texture_coord_id},
                                {c_vertex_id, c_texture_coord_id},
                            },
                            current_material->texture_offset,
                        };
                        state->faces[state->num_faces++] = face1;
                    }
                }
                /*
                char msg[100];
                sprintf(msg, "Found %d face positions\n", num_found);
                memory->platform_debug_message(ctx, msg);
                */
            }
            else if (line[0] == '#')
            {
            }
            else if (line[0] == 'g')
            {
            }
            else if (line[0] == 'o')
            {
            }
            else if (line[0] == 's')
            {
            }
            else if (strncmp(line, "mtllib", 6) == 0)
            {
                char *filename = line + 7;
                bool loaded = memory->platform_editor_load_nearby_file(ctx, &state->mtl_file, state->model_file, filename);
                hassert(loaded);
                load_mtl(ctx, memory);
            }
            else if (strncmp(line, "usemtl", 6) == 0)
            {
                material *tm;
                char material_name[100];
                auto material_name_length = eol - position - 7;
                strncpy(material_name, line + 7, (size_t)material_name_length + 1);

                current_material = 0;
                for (tm = state->materials;
                        tm < state->materials + state->num_materials;
                        tm++)
                {
                    if (strcmp(tm->name, material_name) == 0)
                    {
                        current_material = tm;
                        break;
                    }
                }
                hassert(current_material);
            }
            else
            {
                hassert(!"Invalid code path");
            }

            if (*eol == '\0')
            {
                break;
            }
            position = eol + 1;
            while((*position == '\r') && (*position == '\n'))
            {
                position++;
            }
            if (counter++ >= max_lines)
            {
                break;
            }
        }

        for (uint32_t face_idx = 0;
                face_idx < state->num_faces * 3;
                ++face_idx)
        {
            state->num_faces_array++;
            state->faces_array[face_idx] = (GLushort)face_idx;
        }


        for (GLushort face_array_idx = 0;
                face_array_idx < state->num_faces_array;
                face_array_idx += 3)
        {
            state->line_elements[state->num_line_elements++] = face_array_idx;
            state->line_elements[state->num_line_elements++] = (GLushort)(face_array_idx + 1);
            state->line_elements[state->num_line_elements++] = (GLushort)(face_array_idx + 1);
            state->line_elements[state->num_line_elements++] = (GLushort)(face_array_idx + 2);
            state->line_elements[state->num_line_elements++] = (GLushort)(face_array_idx + 2);
            state->line_elements[state->num_line_elements++] = face_array_idx;
        }

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->ibo);
        glErrorAssert();
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                (GLsizeiptr)(state->num_faces_array * sizeof(state->faces_array[0])),
                state->faces_array,
                GL_STATIC_DRAW);
        glErrorAssert();

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->line_ibo);
        glErrorAssert();
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                (GLsizeiptr)(state->num_line_elements * sizeof(state->line_elements[0])),
                state->line_elements,
                GL_STATIC_DRAW);
        glErrorAssert();

        for (uint32_t face_idx = 0;
                face_idx < state->num_faces;
                ++face_idx)
        {
            state->num_vbo_vertices += 3;
            vertex_with_style *vbo_v1 = state->vbo_vertices + (3 * face_idx);
            vertex_with_style *vbo_v2 = vbo_v1 + 1;
            vertex_with_style *vbo_v3 = vbo_v2 + 1;
            face *f = state->faces + face_idx;
            v3 *face_v1 = state->vertices + (f->indices[0].vertex - 1);
            v3 *face_v2 = state->vertices + (f->indices[1].vertex - 1);
            v3 *face_v3 = state->vertices + (f->indices[2].vertex - 1);

            v3 *face_vt1 = state->texture_coords + (f->indices[0].texture_coord - 1);
            v3 *face_vt2 = state->texture_coords + (f->indices[1].texture_coord - 1);
            v3 *face_vt3 = state->texture_coords + (f->indices[2].texture_coord - 1);

            vbo_v1->v.position[0] = face_v1->x;
            vbo_v1->v.position[1] = face_v1->y;
            vbo_v1->v.position[2] = face_v1->z;
            vbo_v1->v.position[3] = 1.0f;

            vbo_v2->v.position[0] = face_v2->x;
            vbo_v2->v.position[1] = face_v2->y;
            vbo_v2->v.position[2] = face_v2->z;
            vbo_v2->v.position[3] = 1.0f;

            vbo_v3->v.position[0] = face_v3->x;
            vbo_v3->v.position[1] = face_v3->y;
            vbo_v3->v.position[2] = face_v3->z;
            vbo_v3->v.position[3] = 1.0f;

            vbo_v1->v.color[0] = face_vt1->x;
            vbo_v1->v.color[1] = 1 - face_vt1->y;
            vbo_v1->v.color[2] = 0.0f;
            vbo_v1->v.color[3] = 1.0f;

            vbo_v2->v.color[0] = face_vt2->x;
            vbo_v2->v.color[1] = 1 - face_vt2->y;
            vbo_v2->v.color[2] = 0.0f;
            vbo_v2->v.color[3] = 1.0f;

            vbo_v3->v.color[0] = face_vt3->x;
            vbo_v3->v.color[1] = 1 - face_vt3->y;
            vbo_v3->v.color[2] = 0.0f;
            vbo_v3->v.color[3] = 1.0f;

            vbo_v1->style[0] = 3.0f;
            vbo_v1->style[1] = (float)f->texture_offset;
            vbo_v2->style[0] = 3.0f;
            vbo_v2->style[1] = (float)f->texture_offset;
            vbo_v3->style[0] = 3.0f;
            vbo_v3->style[1] = (float)f->texture_offset;
        }

        glBindBuffer(GL_ARRAY_BUFFER, state->vbo);
        glErrorAssert();

        glBufferData(GL_ARRAY_BUFFER,
                (GLsizeiptr)(sizeof(state->vbo_vertices[0]) * state->num_faces * 3),
                state->vbo_vertices,
                GL_STATIC_DRAW);
        glErrorAssert();
    }

    for (uint32_t i = 0;
            i < harray_count(input->controllers);
            ++i)
    {
        if (!input->controllers[i].is_active)
        {
            continue;
        }
        game_controller_state *controller = &input->controllers[i];
        if (controller->buttons.back.ended_down && !controller->buttons.back.repeat)
        {
            memory->quit = true;
        }
        if (controller->buttons.start.ended_down && !controller->buttons.start.repeat)
        {
            state->hide_lines ^= true;
        }
    }

    glBindVertexArray(state->vao);

    glErrorAssert();

    // Revert to something resembling defaults
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthFunc(GL_ALWAYS);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);

    glErrorAssert();

    glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glErrorAssert();

    state->delta_t += input->delta_t;

    glUseProgram(state->program_b.program);
    glBindBuffer(GL_ARRAY_BUFFER, state->vbo);

    glEnableVertexAttribArray((GLuint)state->program_b.a_pos_id);
    glEnableVertexAttribArray((GLuint)state->program_b.a_color_id);
    glEnableVertexAttribArray((GLuint)state->program_b.a_style_id);
    glVertexAttribPointer((GLuint)state->program_b.a_pos_id, 4, GL_FLOAT, GL_FALSE, sizeof(vertex_with_style), 0);
    glVertexAttribPointer((GLuint)state->program_b.a_color_id, 4, GL_FLOAT, GL_FALSE, sizeof(vertex_with_style), (void *)offsetof(vertex, color));
    glVertexAttribPointer((GLuint)state->program_b.a_style_id, 4, GL_FLOAT, GL_FALSE, sizeof(vertex_with_style), (void *)offsetof(vertex_with_style, style));

    for (uint32_t idx = 0;
            idx < state->num_texture_ids;
            ++idx)
    {
        glUniform1i(state->sampler_ids[idx], idx);
        glActiveTexture(GL_TEXTURE0 + idx);
        glBindTexture(GL_TEXTURE_2D, state->texture_ids[idx]);
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->ibo);

    v3 axis = {0.0f, 1.0f, 0.0f};
    v3 center = v3div(v3add(state->model_max, state->model_min), 2.0f);
    m4 center_translate = m4identity();
    center_translate.cols[3] = {-center.x, -center.y, -center.z, 1.0f};

    v3 dimension = v3sub(state->model_max, state->model_min);
    float max_dimension = dimension.x;
    if (dimension.y > max_dimension)
    {
        max_dimension = dimension.y;
    }
    if (dimension.z > max_dimension)
    {
        max_dimension = dimension.z;
    }
    m4 rotate = m4rotation(axis, state->delta_t);
    m4 scale = m4identity();
    scale.cols[0].E[0] = 2.0f / max_dimension;
    scale.cols[1].E[1] = 2.0f / max_dimension;
    scale.cols[2].E[2] = 2.0f / max_dimension;
    m4 translate = m4identity();
    translate.cols[3] = v4{0.0f, 0.0f, -10.0f, 1.0f};

    m4 a = center_translate;
    m4 b = scale;
    m4 c = rotate;
    m4 d = translate;

    m4 u_model = m4mul(d, m4mul(c, m4mul(b, a)));

    v4 u_mvp_enabled = {1.0f, 0.0f, 0.0f, 0.0f};
    glUniform4fv(state->program_b.u_mvp_enabled_id, 1, (float *)&u_mvp_enabled);
    glUniformMatrix4fv(state->program_b.u_model_id, 1, false, (float *)&u_model);
    m4 u_view = m4identity();
    glUniformMatrix4fv(state->program_b.u_view_id, 1, false, (float *)&u_view);
    float ratio = 960.0f / 540.0f;
    m4 u_perspective = m4frustumprojection(state->near_, state->far_, {-ratio, -1.0f}, {ratio, 1.0f});
    //m4 u_perspective = m4identity();
    glUniformMatrix4fv(state->program_b.u_perspective_id, 1, false, (float *)&u_perspective);

    glDrawElements(GL_TRIANGLES, (GLsizei)state->num_faces_array, GL_UNSIGNED_SHORT, 0);
    glErrorAssert();

    if (!state->hide_lines)
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->line_ibo);
        glDepthFunc(GL_LEQUAL);
        u_mvp_enabled = {1.0f, 0.0f, 0.0f, 1.0f};
        glUniform4fv(state->program_b.u_mvp_enabled_id, 1, (float *)&u_mvp_enabled);
        glDrawElements(GL_LINES, (GLsizei)state->num_line_elements, GL_UNSIGNED_SHORT, 0);
    }
}

