#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <gl/gl.h>

#include <stdio.h>

#include <chrono>

#include "hajonta/platform/common.h"
#include "hajonta/renderer/common.h"

#include "hajonta/thirdparty/glext.h"
#define HGLD(b,a) extern PFNGL##a##PROC gl##b;
extern "C" {
#include "hajonta/platform/glextlist.txt"
}
#undef HGLD

#define HGLD(b,a)  PFNGL##a##PROC gl##b;
#include "hajonta/platform/glextlist.txt"
#undef HGLD

#include "hajonta/programs/imgui.h"
#include "hajonta/programs/ui2d.h"

#include "stb_truetype.h"
#include "hajonta/ui/ui2d.cpp"
#include "hajonta/image.cpp"
#include "hajonta/math.cpp"

inline void
load_glfuncs(hajonta_thread_context *ctx, platform_glgetprocaddress_func *get_proc_address)
{
#define HGLD(b,a) gl##b = (PFNGL##a##PROC)get_proc_address(ctx, (char *)"gl"#b);
#include "hajonta/platform/glextlist.txt"
#undef HGLD
}

inline void
glErrorAssert(bool skip = false)
{
    GLenum error = glGetError();
    if (skip)
    {
         return;
    }
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

#if defined(_MSC_VER)
#pragma warning(push, 4)
#pragma warning(disable: 4365 4312 4456 4457 4774 4577)
#endif
#include "imgui.cpp"
#include "imgui_draw.cpp"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

struct ui2d_state
{
    ui2d_program_struct ui2d_program;
    uint32_t vbo;
    uint32_t ibo;
    uint32_t vao;
    uint32_t mouse_texture;
};

struct asset_file_to_texture
{
    int32_t asset_file_id;
    int32_t texture_offset;
};

struct
asset_file
{
    char asset_file_path[1024];
};

struct asset
{
    uint32_t asset_id;
    char asset_name[100];
    int32_t asset_file_id;
    v2 st0;
    v2 st1;
};

struct renderer_state
{
    bool initialized;

    imgui_program_struct imgui_program;
    uint32_t vbo;
    uint32_t ibo;
    uint32_t QUADS_ibo;
    uint32_t vao;
    int32_t tex_id;
    uint32_t font_texture;
    uint32_t white_texture;

    ui2d_state ui_state;

    char bitmap_scratch[4096 * 4096 * 4];
    uint8_t asset_scratch[4096 * 4096 * 4];
    game_input *input;

    uint32_t textures[16];
    uint32_t texture_count;
    asset_file_to_texture texture_lookup[16];
    uint32_t texture_lookup_count;

    asset_file asset_files[16];
    uint32_t asset_file_count;
    asset assets[32];
    uint32_t asset_count;

    uint32_t generation_id;

    mouse_input original_mouse_input;

    uint32_t vertices_count;
    struct
    {
        v2 pos;
        v2 uv;
        uint32_t col;
    } vertices_scratch[4 * 2000];
    uint32_t indices_scratch[6 * 2000];
};

int32_t
lookup_asset_file_to_texture(renderer_state *state, int32_t asset_file_id)
{
    int32_t result = -1;
    for (uint32_t i = 0; i < state->texture_lookup_count; ++i)
    {
        asset_file_to_texture *lookup = state->texture_lookup + i;
        if (lookup->asset_file_id == asset_file_id)
        {
            result = lookup->texture_offset;
            break;
        }
    }
    return result;
}

void
add_asset_file_texture_lookup(renderer_state *state, int32_t asset_file_id, int32_t texture_offset)
{
    hassert(state->texture_lookup_count < harray_count(state->texture_lookup));
    asset_file_to_texture *lookup = state->texture_lookup + state->texture_lookup_count;
    ++state->texture_lookup_count;
    lookup->asset_file_id = asset_file_id;
    lookup->texture_offset = texture_offset;
}

static renderer_state _GlobalRendererState;

void
draw_ui2d(hajonta_thread_context *ctx, platform_memory *memory, renderer_state *state, ui2d_push_context *pushctx)
{
    ui2d_state *ui_state = &state->ui_state;
    ui2d_program_struct *ui2d_program = &ui_state->ui2d_program;

    glDisable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);
    glUseProgram(ui2d_program->program);
    glBindVertexArray(ui_state->vao);

    float screen_pixel_size[] =
    {
        (float)state->input->window.width,
        (float)state->input->window.height,
    };
    glUniform2fv(ui2d_program->screen_pixel_size_id, 1, (float *)&screen_pixel_size);

    glBindBuffer(GL_ARRAY_BUFFER, ui_state->vbo);
    glBufferData(GL_ARRAY_BUFFER,
            (GLsizei)(pushctx->num_vertices * sizeof(pushctx->vertices[0])),
            pushctx->vertices,
            GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ui_state->ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            (GLsizei)(pushctx->num_elements * sizeof(pushctx->elements[0])),
            pushctx->elements,
            GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray((GLuint)ui2d_program->a_pos_id);
    glEnableVertexAttribArray((GLuint)ui2d_program->a_tex_coord_id);
    glEnableVertexAttribArray((GLuint)ui2d_program->a_texid_id);
    glEnableVertexAttribArray((GLuint)ui2d_program->a_options_id);
    glEnableVertexAttribArray((GLuint)ui2d_program->a_channel_color_id);
    glVertexAttribPointer((GLuint)ui2d_program->a_pos_id, 2, GL_FLOAT, GL_FALSE, sizeof(ui2d_vertex_format), 0);
    glVertexAttribPointer((GLuint)ui2d_program->a_tex_coord_id, 2, GL_FLOAT, GL_FALSE, sizeof(ui2d_vertex_format), (void *)offsetof(ui2d_vertex_format, tex_coord));
    glVertexAttribPointer((GLuint)ui2d_program->a_texid_id, 1, GL_FLOAT, GL_FALSE, sizeof(ui2d_vertex_format), (void *)offsetof(ui2d_vertex_format, texid));
    glVertexAttribPointer((GLuint)ui2d_program->a_options_id, 1, GL_FLOAT, GL_FALSE, sizeof(ui2d_vertex_format), (void *)offsetof(ui2d_vertex_format, options));
    glVertexAttribPointer((GLuint)ui2d_program->a_channel_color_id, 3, GL_FLOAT, GL_FALSE, sizeof(ui2d_vertex_format), (void *)offsetof(ui2d_vertex_format, channel_color));

    GLint uniform_locations[10] = {};
    char msg[] = "tex[xx]";
    for (int idx = 0; idx < harray_count(uniform_locations); ++idx)
    {
        sprintf(msg, "tex[%d]", idx);
        uniform_locations[idx] = glGetUniformLocation(ui2d_program->program, msg);

    }

    for (uint32_t i = 0; i < pushctx->num_textures; ++i)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, pushctx->textures[i]);
        glUniform1i(
            uniform_locations[i],
            (GLint)i);
    }

    // TODO(nbm): Get textures working properly.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ui_state->mouse_texture);
    glUniform1i(uniform_locations[0], 0);

    glDrawElements(GL_TRIANGLES, (GLsizei)pushctx->num_elements, GL_UNSIGNED_INT, 0);
    glErrorAssert();
}

bool
load_texture_asset(
    hajonta_thread_context *ctx,
    platform_memory *memory,
    renderer_state *state,
    const char *filename,
    uint8_t *image_buffer,
    uint32_t image_size,
    int32_t *x,
    int32_t *y,
    GLuint *tex,
    GLenum target
)
{
    if (!memory->platform_load_asset(ctx, filename, image_size, image_buffer)) {
        return false;
    }

    uint32_t actual_size;
    load_image(image_buffer, image_size, (uint8_t *)state->bitmap_scratch, sizeof(state->bitmap_scratch),
            x, y, &actual_size);

    if (tex)
    {
        if (!*tex)
        {
            glGenTextures(1, tex);
        }
        glBindTexture(GL_TEXTURE_2D, *tex);
    }

    glTexImage2D(target, 0, GL_RGBA,
        *x, *y, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, state->bitmap_scratch);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return true;
}
void
load_texture_asset_failed(
    hajonta_thread_context *ctx,
    platform_memory *memory,
    char *filename
)
{
    char msg[1024];
    sprintf(msg, "Could not load %s\n", filename);
    memory->platform_fail(ctx, msg);
    memory->quit = true;
}

bool
ui2d_program_init(hajonta_thread_context *ctx, platform_memory *memory, renderer_state *state)
{
    ui2d_state *ui_state = &state->ui_state;
    ui2d_program_struct *program = &ui_state->ui2d_program;
    ui2d_program(program, ctx, memory);

    glGenBuffers(1, &ui_state->vbo);
    glGenBuffers(1, &ui_state->ibo);

    glGenVertexArrays(1, &ui_state->vao);
    glBindVertexArray(ui_state->vao);

    int32_t x, y;
    char filename[] = "ui/slick_arrows/slick_arrow-delta.png";
    bool loaded = load_texture_asset(ctx, memory, state, filename, state->asset_scratch, sizeof(state->asset_scratch), &x, &y, &state->ui_state.mouse_texture, GL_TEXTURE_2D);
    if (!loaded)
    {
        load_texture_asset_failed(ctx, memory, filename);
        return false;
    }
    return true;
}

bool
program_init(hajonta_thread_context *ctx, platform_memory *memory, renderer_state *state)
{
    imgui_program_struct *program = &state->imgui_program;
    imgui_program(program, ctx, memory);

    state->tex_id = glGetUniformLocation(program->program, "tex");

    glGenBuffers(1, &state->vbo);
    glGenBuffers(1, &state->ibo);
    glGenBuffers(1, &state->QUADS_ibo);

    glGenVertexArrays(1, &state->vao);
    glBindVertexArray(state->vao);
    glBindBuffer(GL_ARRAY_BUFFER, state->vbo);
    glEnableVertexAttribArray((GLuint)program->a_position_id);
    glEnableVertexAttribArray((GLuint)program->a_uv_id);
    glEnableVertexAttribArray((GLuint)program->a_color_id);

    glVertexAttribPointer((GLuint)program->a_position_id, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (void *)offsetof(ImDrawVert, pos));
    glVertexAttribPointer((GLuint)program->a_uv_id, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (void *)offsetof(ImDrawVert, uv));
    glVertexAttribPointer((GLuint)program->a_color_id, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (void *)offsetof(ImDrawVert, col));

    ImGuiIO& io = ImGui::GetIO();
    uint8_t *pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    glGenTextures(1, &state->font_texture);
    glBindTexture(GL_TEXTURE_2D, state->font_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    io.Fonts->TexID = (void *)(intptr_t)state->font_texture;

    glGenTextures(1, &state->white_texture);
    glBindTexture(GL_TEXTURE_2D, state->white_texture);
    uint32_t color32 = 0xffffffff;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &color32);

    return true;
}

int32_t
lookup_asset_file(renderer_state *state, char *asset_file_path)
{
    int32_t result = -1;
    for (uint32_t i = 0; i < state->asset_file_count; ++i)
    {
        if (strcmp(asset_file_path, state->asset_files[i].asset_file_path) == 0)
        {
            result = (int32_t)i;
            break;
        }
    }
    return result;
}

int32_t
add_asset_file(renderer_state *state, char *asset_file_path)
{
    int32_t result = lookup_asset_file(state, asset_file_path);
    hassert(state->asset_file_count < harray_count(state->asset_files));
    if (result < 0 && state->asset_file_count < harray_count(state->asset_files))
    {
        asset_file *f = state->asset_files + state->asset_file_count;
        result = (int32_t)state->asset_file_count;
        ++state->asset_file_count;
        strcpy(f->asset_file_path, asset_file_path);
    }
    return result;
}

int32_t
add_asset_file_texture(hajonta_thread_context *ctx, platform_memory *memory, renderer_state *state, int32_t asset_file_id)
{
    int32_t result = lookup_asset_file_to_texture(state, asset_file_id);
    hassert(state->texture_count < harray_count(state->textures));
    if (result < 0)
    {
        int32_t x, y;
        asset_file *asset_file0 = state->asset_files + asset_file_id;
        bool loaded = load_texture_asset(ctx, memory, state, asset_file0->asset_file_path, state->asset_scratch, sizeof(state->asset_scratch), &x, &y, state->textures + state->texture_count, GL_TEXTURE_2D);
        if (!loaded)
        {
            load_texture_asset_failed(ctx, memory, asset_file0->asset_file_path);
            return false;
        }
        result = (int32_t)state->texture_count;
        ++state->texture_count;
        add_asset_file_texture_lookup(state, asset_file_id, (int32_t)state->textures[result]);
    }
    return result;
}

int32_t
add_asset(renderer_state *state, char *asset_name, int32_t asset_file_id, v2 st0, v2 st1)
{
    int32_t result = -1;
    hassert(state->asset_count < harray_count(state->assets));
    asset *asset0 = state->assets + state->asset_count;
    asset0->asset_id = 0;
    strcpy(asset0->asset_name, asset_name);
    asset0->asset_file_id = asset_file_id;
    asset0->st0 = st0;
    asset0->st1 = st1;
    result = (int32_t)state->asset_count;
    ++state->asset_count;
    return result;
}

int32_t
add_asset(renderer_state *state, char *asset_name, char *asset_file_name, v2 st0, v2 st1)
{
    int32_t result = -1;

    int32_t asset_file_id = add_asset_file(state, asset_file_name);
    if (asset_file_id >= 0)
    {
        result = add_asset(state, asset_name, asset_file_id, st0, st1);
    }
    return result;
}

int32_t
add_tilemap_asset(renderer_state *state, char *asset_name, char *asset_file_name, uint32_t width, uint32_t height, uint32_t tile_width, uint32_t tile_height, uint32_t spacing, uint32_t tile_id)
{
    uint32_t tiles_wide = (width + spacing) / (tile_width + spacing);
    uint32_t tile_x_position = tile_id % tiles_wide;
    uint32_t tile_y_position = tile_id / tiles_wide;
    float tile_x_base = tile_x_position * (tile_width + spacing) / (float)width;
    float tile_y_base = tile_y_position * (tile_height + spacing) / (float)height;
    float tile_x_offset = (float)(tile_width - 1) / width;
    float tile_y_offset = (float)(tile_height - 1) / height;
    v2 st0 = { tile_x_base + 0.001f, tile_y_base + tile_y_offset };
    v2 st1 = { tile_x_base + tile_x_offset, tile_y_base + 0.001f};

    return add_asset(state, asset_name, asset_file_name, st0, st1);
}

RENDERER_SETUP(renderer_setup)
{
    static std::chrono::steady_clock::time_point last_frame_start_time = std::chrono::steady_clock::now();

#if !defined(NEEDS_EGL) && !defined(__APPLE__)
    if (!glCreateProgram)
    {
        load_glfuncs(ctx, memory->platform_glgetprocaddress);
    }
#endif
    memory->render_lists_count = 0;
    renderer_state *state = &_GlobalRendererState;

    memory->imgui_state = ImGui::GetInternalState();
    if (!_GlobalRendererState.initialized)
    {
        program_init(ctx, memory, &_GlobalRendererState);
        hassert(ui2d_program_init(ctx, memory, &_GlobalRendererState));
        _GlobalRendererState.initialized = true;
        state->generation_id = 1;
        add_asset(state, "mouse_cursor_old", "ui/slick_arrows/slick_arrow-delta.png", {0.0f, 0.0f}, {1.0f, 1.0f});
        add_asset(state, "mouse_cursor", "testing/kenney/cursorSword_silver.png", {0.0f, 0.0f}, {1.0f, 1.0f});
        add_tilemap_asset(state, "sea_0", "testing/kenney/RPGpack_sheet_2X.png", 2560, 1664, 128, 128, 0, 31);
        add_tilemap_asset(state, "ground_0", "testing/kenney/RPGpack_sheet_2X.png", 2560, 1664, 128, 128, 0, 21);
        add_tilemap_asset(state, "sea_ground_br", "testing/kenney/RPGpack_sheet_2X.png", 2560, 1664, 128, 128, 0, 13);
        add_tilemap_asset(state, "sea_ground_bl", "testing/kenney/RPGpack_sheet_2X.png", 2560, 1664, 128, 128, 0, 14);
        add_tilemap_asset(state, "sea_ground_tr", "testing/kenney/RPGpack_sheet_2X.png", 2560, 1664, 128, 128, 0, 33);
        add_tilemap_asset(state, "sea_ground_tl", "testing/kenney/RPGpack_sheet_2X.png", 2560, 1664, 128, 128, 0, 34);
        add_tilemap_asset(state, "sea_ground_r", "testing/kenney/RPGpack_sheet_2X.png", 2560, 1664, 128, 128, 0, 32);
        add_tilemap_asset(state, "sea_ground_l", "testing/kenney/RPGpack_sheet_2X.png", 2560, 1664, 128, 128, 0, 30);
        add_tilemap_asset(state, "sea_ground_t", "testing/kenney/RPGpack_sheet_2X.png", 2560, 1664, 128, 128, 0, 11);
        add_tilemap_asset(state, "sea_ground_b", "testing/kenney/RPGpack_sheet_2X.png", 2560, 1664, 128, 128, 0, 51);
        add_tilemap_asset(state, "sea_ground_t_l", "testing/kenney/RPGpack_sheet_2X.png", 2560, 1664, 128, 128, 0, 10);
        add_tilemap_asset(state, "sea_ground_t_r", "testing/kenney/RPGpack_sheet_2X.png", 2560, 1664, 128, 128, 0, 12);
        add_tilemap_asset(state, "sea_ground_b_l", "testing/kenney/RPGpack_sheet_2X.png", 2560, 1664, 128, 128, 0, 50);
        add_tilemap_asset(state, "sea_ground_b_r", "testing/kenney/RPGpack_sheet_2X.png", 2560, 1664, 128, 128, 0, 52);
        add_tilemap_asset(state, "bottom_wall", "testing/kenney/RPGpack_sheet_2X.png", 2560, 1664, 128, 128, 0, 69);
        add_asset(state, "player", "testing/kenney/alienPink_stand.png", {0.0f, 1.0f}, {1.0f, 0.0f});
        add_asset(state, "familiar_ship", "testing/kenney/shipBlue.png", {0.0f, 1.0f}, {1.0f, 0.0f});
        add_asset(state, "familiar", "testing/kenney/alienBlue_stand.png", {0.0f, 1.0f}, {1.0f, 0.0f});

        uint32_t scratch_pos = 0;
        for (uint32_t i = 0; i < harray_count(state->indices_scratch) / 6; ++i)
        {
            uint32_t start = i * 4;
            state->indices_scratch[scratch_pos++] = start + 0;
            state->indices_scratch[scratch_pos++] = start + 1;
            state->indices_scratch[scratch_pos++] = start + 2;
            state->indices_scratch[scratch_pos++] = start + 2;
            state->indices_scratch[scratch_pos++] = start + 3;
            state->indices_scratch[scratch_pos++] = start + 0;
        }
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->QUADS_ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            sizeof(state->indices_scratch),
            state->indices_scratch,
            GL_STATIC_DRAW);
    }

    _GlobalRendererState.input = input;

    ImGuiIO& io = ImGui::GetIO();

    io.DisplaySize = ImVec2((float)input->window.width, (float)input->window.height);
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

    std::chrono::steady_clock::time_point this_frame_start_time = std::chrono::steady_clock::now();
    auto diff = this_frame_start_time - last_frame_start_time;
    using microseconds = std::chrono::microseconds;
    int64_t duration_us = std::chrono::duration_cast<microseconds>(diff).count();
    io.DeltaTime = (float)duration_us / 1000 / 1000;
    last_frame_start_time = this_frame_start_time;

    io.MousePos = ImVec2((float)input->mouse.x, (float)input->mouse.y);
    io.MouseDown[0] = input->mouse.buttons.left.ended_down;
    io.MouseDown[1] = input->mouse.buttons.middle.ended_down;
    io.MouseDown[2] = input->mouse.buttons.right.ended_down;
    io.MouseWheel = 0.0f;

    io.KeysDown[8] = false;
    io.KeyMap[ImGuiKey_Backspace] = 8;

    if (memory->debug_keyboard)
    {
        keyboard_input *k = input->keyboard_inputs;
        for (uint32_t idx = 0;
                idx < harray_count(input->keyboard_inputs);
                ++idx)
        {
            keyboard_input *ki = k + idx;
            if (ki->type == keyboard_input_type::ASCII)
            {
                if (ki->ascii == 8)
                {
                     io.KeysDown[ki->ascii] = true;
                }
                else
                {
                    io.AddInputCharacter((ImWchar)ki->ascii);
                }
            }
        }
    }

    state->original_mouse_input = input->mouse;
    bool wanted_capture_mouse = io.WantCaptureMouse;

    ImGui::NewFrame();

    if (io.WantCaptureKeyboard)
    {
        memory->debug_keyboard = true;
    } else
    {
        memory->debug_keyboard = false;
    }

    if (!io.WantCaptureMouse && wanted_capture_mouse)
    {
        input->mouse.buttons.left.repeat = true;
        state->original_mouse_input.buttons.left.repeat = true;
    }

    if (io.WantCaptureMouse)
    {
        input->mouse.buttons.left.repeat = !BUTTON_WENT_UP(input->mouse.buttons.left) || wanted_capture_mouse;
        input->mouse.buttons.left.ended_down = false;
        input->mouse.buttons.right.repeat = !BUTTON_WENT_UP(input->mouse.buttons.right) || wanted_capture_mouse;
        input->mouse.buttons.right.ended_down = false;
        input->mouse.buttons.middle.repeat = !BUTTON_WENT_UP(input->mouse.buttons.middle) || wanted_capture_mouse;
        input->mouse.buttons.middle.ended_down = false;
    }

    io.MouseDrawCursor = io.WantCaptureMouse;

    return true;
}

void render_draw_lists(ImDrawData* draw_data, renderer_state *state)
{
    // Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glActiveTexture(GL_TEXTURE0);

    // Handle cases of screen coordinates != from framebuffer coordinates (e.g. retina displays)
    ImGuiIO& io = ImGui::GetIO();
    int fb_width = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
    int fb_height = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
    draw_data->ScaleClipRects(io.DisplayFramebufferScale);

    // Setup viewport, orthographic projection matrix
    glViewport(0, 0, (GLsizei)fb_width, (GLsizei)fb_height);
    const float ortho_projection[4][4] =
    {
        { 2.0f/io.DisplaySize.x, 0.0f,                   0.0f, 0.0f },
        { 0.0f,                  2.0f/-io.DisplaySize.y, 0.0f, 0.0f },
        { 0.0f,                  0.0f,                  -1.0f, 0.0f },
        {-1.0f,                  1.0f,                   0.0f, 1.0f },
    };
    glUniformMatrix4fv(state->imgui_program.u_projection_id, 1, GL_FALSE, &ortho_projection[0][0]);
    glBindVertexArray(state->vao);

    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        const ImDrawIdx* idx_buffer_offset = 0;

        glBindBuffer(GL_ARRAY_BUFFER, state->vbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(cmd_list->VtxBuffer.size() * sizeof(ImDrawVert)), (GLvoid*)&cmd_list->VtxBuffer.front(), GL_STREAM_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(cmd_list->IdxBuffer.size() * sizeof(ImDrawIdx)), (GLvoid*)&cmd_list->IdxBuffer.front(), GL_STREAM_DRAW);

        for (const ImDrawCmd* pcmd = cmd_list->CmdBuffer.begin(); pcmd != cmd_list->CmdBuffer.end(); pcmd++)
        {
            if (pcmd->UserCallback)
            {
                pcmd->UserCallback(cmd_list, pcmd);
            }
            else
            {
                glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)pcmd->TextureId);
                glScissor((int)pcmd->ClipRect.x, (int)(fb_height - pcmd->ClipRect.w), (int)(pcmd->ClipRect.z - pcmd->ClipRect.x), (int)(pcmd->ClipRect.w - pcmd->ClipRect.y));
                glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, idx_buffer_offset);
            }
            idx_buffer_offset += pcmd->ElemCount;
        }
    }
    glBindVertexArray(0);
}

static uint32_t generations_updated_this_frame = 0;

void
update_asset_descriptor_asset_id(renderer_state *state, asset_descriptor *descriptor)
{
    if (descriptor->generation_id != state->generation_id)
    {
        ++generations_updated_this_frame;
        int32_t result = -1;
        for (uint32_t i = 0; i < state->asset_count; ++i)
        {
            if (strcmp(descriptor->asset_name, state->assets[i].asset_name) == 0)
            {
                result = (int32_t)i;
                break;
            }
        }
        descriptor->asset_id = result;
        descriptor->generation_id = state->generation_id;
    }
}

void
draw_quad(hajonta_thread_context *ctx, platform_memory *memory, renderer_state *state, m4 *matrices, asset_descriptor *descriptors, render_entry_type_quad *quad)
{
    m4 projection = matrices[quad->matrix_id];

    uint32_t texture = state->white_texture;
    v2 st0 = {0, 0};
    v2 st1 = {1, 1};
    if (quad->asset_descriptor_id != -1)
    {
        asset_descriptor *descriptor = descriptors + quad->asset_descriptor_id;
        update_asset_descriptor_asset_id(state, descriptor);
        if (descriptor->asset_id >= 0)
        {
            asset *descriptor_asset = state->assets + descriptor->asset_id;
            st0 = descriptor_asset->st0;
            st1 = descriptor_asset->st1;
            int32_t asset_file_id = descriptor_asset->asset_file_id;
            int32_t texture_lookup = lookup_asset_file_to_texture(state, asset_file_id);
            if (texture_lookup >= 0)
            {
                texture = (uint32_t)texture_lookup;
            }
            else
            {
                int32_t texture_id = add_asset_file_texture(ctx, memory, state, asset_file_id);
                if (texture_id >= 0)
                {
                    texture = (uint32_t)texture_id;
                }
            }
        }
    }

    glUniformMatrix4fv(state->imgui_program.u_projection_id, 1, GL_FALSE, (float *)&projection);
    glBindVertexArray(state->vao);

    uint32_t col = 0x00000000;
    col |= (uint32_t)(quad->color.w * 255.0f) << 24;
    col |= (uint32_t)(quad->color.z * 255.0f) << 16;
    col |= (uint32_t)(quad->color.y * 255.0f) << 8;
    col |= (uint32_t)(quad->color.x * 255.0f) << 0;
    glBindBuffer(GL_ARRAY_BUFFER, state->vbo);
    struct
    {
        v2 pos;
        v2 uv;
        uint32_t col;
    } vertices[] = {
        {
            { quad->position.x, quad->position.y, },
            { st0.x, st0.y },
            col,
        },
        {
            { quad->position.x + quad->size.x, quad->position.y, },
            { st1.x, st0.y },
            col,
        },
        {
            { quad->position.x + quad->size.x, quad->position.y + quad->size.y, },
            { st1.x, st1.y },
            col,
        },
        {
            { quad->position.x, quad->position.y + quad->size.y, },
            { st0.x, st1.y },
            col,
        },
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);

    uint32_t indices[] = {
        0, 1, 2,
        2, 3, 0,
    };
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), (GLvoid*)indices, GL_STREAM_DRAW);

    glBindTexture(GL_TEXTURE_2D, texture);
    glDrawElements(GL_TRIANGLES, (GLsizei)harray_count(indices), GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
}

void
draw_quads(hajonta_thread_context *ctx, platform_memory *memory, renderer_state *state, m4 *matrices, asset_descriptor *descriptors, render_entry_type_QUADS *quads)
{
    m4 projection = matrices[quads->matrix_id];

    uint32_t texture = state->white_texture;
    v2 st0 = {0, 0};
    v2 st1 = {1, 1};

    if (quads->asset_descriptor_id != -1)
    {
        asset_descriptor *descriptor = descriptors + quads->asset_descriptor_id;
        update_asset_descriptor_asset_id(state, descriptor);
        if (descriptor->asset_id >= 0)
        {
            asset *descriptor_asset = state->assets + descriptor->asset_id;
            st0 = descriptor_asset->st0;
            st1 = descriptor_asset->st1;
            int32_t asset_file_id = descriptor_asset->asset_file_id;
            int32_t texture_lookup = lookup_asset_file_to_texture(state, asset_file_id);
            if (texture_lookup >= 0)
            {
                texture = (uint32_t)texture_lookup;
            }
            else
            {
                int32_t texture_id = add_asset_file_texture(ctx, memory, state, asset_file_id);
                if (texture_id >= 0)
                {
                    texture = (uint32_t)texture_id;
                }
            }
        }
    }

    glUniformMatrix4fv(state->imgui_program.u_projection_id, 1, GL_FALSE, (float *)&projection);
    glBindVertexArray(state->vao);

    hassert(harray_count(state->vertices_scratch) >= quads->entry_count * 4);

    uint32_t scratch_pos = 0;
    for (uint32_t i = 0; i < quads->entry_count; ++i)
    {
        auto &entry = quads->entries[i];
        uint32_t col = 0x00000000;
        col |= (uint32_t)(entry.color.w * 255.0f) << 24;
        col |= (uint32_t)(entry.color.z * 255.0f) << 16;
        col |= (uint32_t)(entry.color.y * 255.0f) << 8;
        col |= (uint32_t)(entry.color.x * 255.0f) << 0;
        state->vertices_scratch[scratch_pos++] = {
            { entry.position.x, entry.position.y, },
            { st0.x, st0.y },
            col,
        };
        state->vertices_scratch[scratch_pos++] = {
            { entry.position.x + entry.size.x, entry.position.y, },
            { st1.x, st0.y },
            col,
        };

        state->vertices_scratch[scratch_pos++] = {
            { entry.position.x + entry.size.x, entry.position.y + entry.size.y, },
            { st1.x, st1.y },
            col,
        };

        state->vertices_scratch[scratch_pos++] = {
            { entry.position.x, entry.position.y + entry.size.y, },
            { st0.x, st1.y },
            col,
        };
    }

    glBindBuffer(GL_ARRAY_BUFFER, state->vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(state->vertices_scratch[0])) * 4 * quads->entry_count, state->vertices_scratch, GL_STREAM_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->QUADS_ibo);
    glBindTexture(GL_TEXTURE_2D, texture);
    glDrawElements(GL_TRIANGLES, (GLsizei)quads->entry_count * 6, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
}


RENDERER_RENDER(renderer_render)
{
    ImGuiIO& io = ImGui::GetIO();
    generations_updated_this_frame = 0;
    glErrorAssert(true);

    renderer_state *state = &_GlobalRendererState;
    hassert(memory->render_lists_count > 0);

    window_data *window = &state->input->window;

    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glScissor(0, 0, window->width, window->height);
    glViewport(0, 0, (GLsizei)window->width, (GLsizei)window->height);
    glUseProgram(state->imgui_program.program);
    glUniform1i(state->tex_id, 0);

    uint32_t quads_drawn = 0;
    for (uint32_t i = 0; i < memory->render_lists_count; ++i)
    {
        render_entry_list *render_list = memory->render_lists[i];
        uint32_t offset = 0;
        hassert(render_list->element_count > 0);

        m4 *matrices = {};
        asset_descriptor *asset_descriptors = {};
        uint32_t matrix_count = 0;
        uint32_t asset_descriptor_count = 0;

        for (uint32_t elements = 0; elements < render_list->element_count; ++elements)
        {
            render_entry_header *header = (render_entry_header *)(render_list->base + offset);
            uint32_t element_size = 0;

            switch (header->type)
            {
                case render_entry_type::clear:
                {
                    ExtractRenderElementWithSize(clear, item, header, element_size);
                    v4 *color = &item->color;
                    glScissor(0, 0, state->input->window.width, state->input->window.height);
                    glClearColor(color->r, color->g, color->b, color->a);
                    glClear(GL_COLOR_BUFFER_BIT);
                } break;
                case render_entry_type::ui2d:
                {
                    ExtractRenderElementWithSize(ui2d, item, header, element_size);
                    draw_ui2d(ctx, memory, state, item->pushctx);
                } break;
                case render_entry_type::quad:
                {
                    ExtractRenderElementWithSize(quad, item, header, element_size);
                    hassert(matrix_count > 0 || item->matrix_id == -1);
                    hassert(asset_descriptor_count > 0 || item->asset_descriptor_id == -1);
                    draw_quad(ctx, memory, state, matrices, asset_descriptors, item);
                    ++quads_drawn;
                } break;
                case render_entry_type::matrices:
                {
                    ExtractRenderElementWithSize(matrices, item, header, element_size);
                    matrices = item->matrices;
                    matrix_count = item->count;
                } break;
                case render_entry_type::asset_descriptors:
                {
                    ExtractRenderElementWithSize(asset_descriptors, item, header, element_size);
                    asset_descriptors = item->descriptors;
                    asset_descriptor_count = item->count;
                } break;
                case render_entry_type::QUADS:
                {
                    ExtractRenderElementWithSize(QUADS, item, header, element_size);
                    draw_quads(ctx, memory, state, matrices, asset_descriptors, item);
                } break;
                case render_entry_type::QUADS_lookup:
                {
                    ExtractRenderElementSizeOnly(QUADS_lookup, element_size);
                } break;
                default:
                {
                    hassert(!"Unhandled render entry type")
                };
            }
            hassert(element_size > 0);
            offset += element_size;
        }
    }
    ImGui::Text("Quads drawn: %d", quads_drawn);
    ImGui::Text("Generations updated: %d", generations_updated_this_frame);
    ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
    ImGui::Text("This frame: %.3f ms", 1000.0f * io.DeltaTime);

    ImGui::Render();
    ImDrawData *draw_data = ImGui::GetDrawData();
    render_draw_lists(draw_data, state);

    glErrorAssert();

    input->mouse = state->original_mouse_input;

    return true;
};
