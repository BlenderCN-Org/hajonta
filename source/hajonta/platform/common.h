#pragma once

#include <stddef.h>
#include <stdint.h>

#include "hajonta/platform/neutral.h"
#include "hajonta/math.h"

#ifdef HAJONTA_DEBUG
#define hassert(expression) if(!(expression)) {*(volatile int *)0 = 0;}
#else
#define hassert(expression)
#endif

#define hstrvalue(x) #x
#define hquoted(x) hstrvalue(x)

#define harray_count(array) (sizeof(array) / sizeof((array)[0]))

struct hajonta_thread_context
{
    void *opaque;
};

struct loaded_file
{
    char *contents;
    uint32_t size;

    char file_path[MAX_PATH]; // platform path for load_nearby
};

#define PLATFORM_FAIL(func_name) void func_name(hajonta_thread_context *ctx, const char *failure_reason)
typedef PLATFORM_FAIL(platform_fail_func);

#define PLATFORM_QUIT(func_name) void func_name(hajonta_thread_context *ctx)
typedef PLATFORM_QUIT(platform_quit_func);

#define PLATFORM_DEBUG_MESSAGE(func_name) void func_name(hajonta_thread_context *ctx, char *message)
typedef PLATFORM_DEBUG_MESSAGE(platform_debug_message_func);

#define PLATFORM_GLGETPROCADDRESS(func_name) void* func_name(hajonta_thread_context *ctx, char *function_name)
typedef PLATFORM_GLGETPROCADDRESS(platform_glgetprocaddress_func);

#define PLATFORM_LOAD_ASSET(func_name) bool func_name(hajonta_thread_context *ctx, const char *asset_path, uint32_t size, uint8_t *dest)
typedef PLATFORM_LOAD_ASSET(platform_load_asset_func);

#define PLATFORM_EDITOR_LOAD_FILE(func_name) bool func_name(hajonta_thread_context *ctx, loaded_file *target)
typedef PLATFORM_EDITOR_LOAD_FILE(platform_editor_load_file_func);

#define PLATFORM_EDITOR_LOAD_NEARBY_FILE(func_name) bool func_name(hajonta_thread_context *ctx, loaded_file *target, loaded_file existing_file, char *name)
typedef PLATFORM_EDITOR_LOAD_NEARBY_FILE(platform_editor_load_nearby_file_func);

// Normal means the OS is keeping track of the mouse cursor location.
// The program still needs to render the cursor, though.
//
// Unlimited means that the cursor location is given as delta from previous
// location.
enum struct platform_cursor_mode
{
    normal,
    unlimited,
    COUNT,
};

struct platform_cursor_settings
{
    bool supported_modes[(int)platform_cursor_mode::COUNT];
    platform_cursor_mode mode;
    bool position_set;
    int32_t mouse_x;
    int32_t mouse_y;
};

struct render_entry_list;

struct platform_memory
{
    bool initialized;
    uint64_t size;
    void *memory;
    bool quit;
    bool debug_keyboard;

    void *imgui_state;

    render_entry_list *render_lists[10];
    uint8_t render_lists_count;

    platform_cursor_settings cursor_settings;

    platform_fail_func *platform_fail;
    platform_glgetprocaddress_func *platform_glgetprocaddress;
    platform_debug_message_func *platform_debug_message;
    platform_load_asset_func *platform_load_asset;
    platform_editor_load_file_func *platform_editor_load_file;
    platform_editor_load_nearby_file_func *platform_editor_load_nearby_file;
};

struct game_button_state
{
    bool ended_down;
    bool repeat;
};

struct game_buttons
{
    game_button_state move_up;
    game_button_state move_down;
    game_button_state move_left;
    game_button_state move_right;
    game_button_state back;
    game_button_state start;
};

struct game_controller_state
{
    bool is_active;

    float stick_x;
    float stick_y;

    union
    {
        game_button_state _buttons[sizeof(game_buttons) / sizeof(game_button_state)];
        game_buttons buttons;
    };
};

enum struct keyboard_input_type
{
    NONE,
    ASCII,
};

enum struct keyboard_input_modifiers
{
    LEFT_SHIFT = (1<<0),
    RIGHT_SHIFT = (1<<1),
    LEFT_CTRL = (1<<2),
    RIGHT_CTRL = (1<<3),
    LEFT_ALT = (1<<4),
    RIGHT_ALT = (1<<5),
    LEFT_CMD = (1<<6),
    RIGHT_CMD = (1<<7),
};

struct keyboard_input
{
    keyboard_input_type type;
    keyboard_input_modifiers modifiers;

    union {
        char ascii;
    };
};

struct mouse_buttons
{
    game_button_state left;
    game_button_state middle;
    game_button_state right;
};

struct mouse_input
{
    bool is_active;
    int32_t x;
    int32_t y;

    int32_t vertical_wheel_delta;

    union
    {
        game_button_state _buttons[sizeof(mouse_buttons) / sizeof(game_button_state)];
        mouse_buttons buttons;
    };
};

struct window_data
{
    int width;
    int height;
};

#define MAX_KEYBOARD_INPUTS 10
#define NUM_CONTROLLERS ((uint32_t)4)
struct game_input
{
    float delta_t;

    window_data window;

    game_controller_state controllers[NUM_CONTROLLERS];
    keyboard_input keyboard_inputs[MAX_KEYBOARD_INPUTS];
    mouse_input mouse;
};

inline game_controller_state *get_controller(game_input *input, uint32_t index)
{
    hassert(index < harray_count(input->controllers));
    return &input->controllers[index];
}

struct game_sound_output
{
    int32_t samples_per_second;
    int32_t channels;
    int32_t number_of_samples;
    void *samples;
};

#define GAME_UPDATE_AND_RENDER(func_name) void func_name(hajonta_thread_context *ctx, platform_memory *memory, game_input *input, game_sound_output *sound_output)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render_func);

#define RENDERER_SETUP(func_name) bool func_name(hajonta_thread_context *ctx, platform_memory *memory, game_input *input)
typedef RENDERER_SETUP(renderer_setup_func);

#define RENDERER_RENDER(func_name) bool func_name(hajonta_thread_context *ctx, platform_memory *memory)
typedef RENDERER_RENDER(renderer_render_func);


#ifdef HJ_DIRECTGL
#include "hajonta/thirdparty/glext.h"
#define HGLD(b,a) extern PFNGL##a##PROC gl##b;
extern "C" {
#include "hajonta/platform/glextlist.txt"
}
#undef HGLD

#define HGLD(b,a)  PFNGL##a##PROC gl##b;
#include "hajonta/platform/glextlist.txt"
#undef HGLD

inline void
load_glfuncs(hajonta_thread_context *ctx, platform_glgetprocaddress_func *get_proc_address)
{
#define HGLD(b,a) gl##b = (PFNGL##a##PROC)get_proc_address(ctx, (char *)"gl"#b);
#include "hajonta/platform/glextlist.txt"
#undef HGLD
}

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
#endif

enum struct
render_entry_type
{
    clear,
    ui2d,
    quad,
    matrices,
    asset_descriptors,
};

struct
render_entry_header
{
     render_entry_type type;
};

struct
render_entry_type_clear
{
    render_entry_header header;
    v4 color;
};

struct ui2d_push_context;
struct
render_entry_type_ui2d
{
    render_entry_header header;
    ui2d_push_context *pushctx;
};

struct
render_entry_type_quad
{
    render_entry_header header;
    v3 position;
    v3 size;
    v4 color;
    int32_t matrix_id;
    int32_t asset_descriptor_id;
};

struct
render_entry_type_matrices
{
    render_entry_header header;
    uint32_t count;
    m4 *matrices;
};

struct
asset_descriptor
{
    const char *asset_name;
    uint32_t asset_id;
};

struct
render_entry_type_asset_descriptors
{
    render_entry_header header;
    uint32_t count;
    asset_descriptor *descriptors;
};

struct
render_entry_list
{
    uint32_t max_size;
    uint32_t current_size;
    uint8_t *base;
    uint32_t element_count;
};

#define RenderListBuffer(r, b) { r.max_size = sizeof(b); r.base = b; }
#define RenderListReset(r) { r.current_size = r.element_count = 0; }

#define PushRenderElement(list, type) (render_entry_type_##type *)PushRenderElement_(list, sizeof(render_entry_type_##type), render_entry_type::##type)
inline void *
PushRenderElement_(render_entry_list *list, uint32_t size, render_entry_type type)
{
    void *res = 0;

    if (list->current_size + size > list->max_size)
    {
        hassert(!"render list too small");
         return res;
    }

    render_entry_header *header = (render_entry_header *)(list->base + list->current_size);
    header->type = type;
    res = header;
    list->current_size += size;
    ++list->element_count;

    return res;
}

#define ExtractRenderElement(type, name, header) render_entry_type_##type *##name = (render_entry_type_##type *)header
#define ExtractRenderElementWithSize(type, name, header, size) ExtractRenderElement(type, name, header); size = sizeof(*##name)

inline void
PushClear(render_entry_list *list, v4 color)
{
     render_entry_type_clear *entry = PushRenderElement(list, clear);
     if (entry)
     {
         entry->color = color;
     }
}

inline void
PushUi2d(render_entry_list *list, ui2d_push_context *pushctx)
{
     render_entry_type_ui2d *entry = PushRenderElement(list, ui2d);
     if (entry)
     {
         entry->pushctx = pushctx;
     }
}

inline void
PushQuad(render_entry_list *list, v3 position, v3 size, v4 color, int32_t matrix_id, int32_t asset_descriptor_id)
{
     render_entry_type_quad *entry = PushRenderElement(list, quad);
     if (entry)
     {
         entry->position = position;
         entry->size = size;
         entry->color = color;
         entry->matrix_id = matrix_id;
         entry->asset_descriptor_id = asset_descriptor_id;
     }
}

inline void
PushMatrices(render_entry_list *list, uint32_t count, m4 *matrices)
{
     render_entry_type_matrices *entry = PushRenderElement(list, matrices);
     if (entry)
     {
         entry->count = count;
         entry->matrices = matrices;
     }
}

inline void
PushAssetDescriptors(render_entry_list *list, uint32_t count, asset_descriptor *descriptors)
{
     render_entry_type_asset_descriptors *entry = PushRenderElement(list, asset_descriptors);
     if (entry)
     {
         entry->count = count;
         entry->descriptors = descriptors;
     }
}

inline void
AddRenderList(platform_memory *memory, render_entry_list *list)
{
    hassert(memory->render_lists_count < sizeof(memory->render_lists) - 1);
    memory->render_lists[memory->render_lists_count] = list;
    ++memory->render_lists_count;
}
