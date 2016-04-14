#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(SDL_WITH_SUBDIR)
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#else
#include <SDL.h>
#include <SDL_opengl.h>
#endif
#include "hajonta/platform/common.h"

struct sdl2_audio_buffer
{
    uint8_t bytes[48000*2*2]; // 48000Hz, two samples, 16 bit
    uint32_t play_position;
    uint32_t write_position;
};

struct sdl2_state
{
    bool stopping;
    char *stop_reason;

    bool sdl_inited;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    SDL_GLContext context;

    SDL_AudioDeviceID audio_device;
    SDL_AudioSpec audio_spec;
    bool audio_started;

#if !SDL_VERSION_ATLEAST(2,0,4)
    sdl2_audio_buffer audio_buffer;
#endif

    char *base_path;
    char asset_path[MAX_PATH];
    char arg_asset_path[MAX_PATH];

    game_input *new_input;
    game_input *old_input;

    int32_t window_width;
    int32_t window_height;
};

struct game_code
{
    game_update_and_render_func *game_update_and_render;

    void *handle;
};

struct renderer_code
{
    renderer_setup_func *renderer_setup;
    renderer_render_func *renderer_render;

    void *handle;
};

static bool
_fail(sdl2_state *state, const char *failure_reason)
{
    state->stopping = true;
    state->stop_reason = strdup(failure_reason);
    return false;
}

static void
sdl_cleanup(sdl2_state *state)
{
    if (state->texture)
    {
        SDL_DestroyTexture(state->texture);
        state->texture = 0;
    }
    if (state->renderer)
    {
        SDL_DestroyRenderer(state->renderer);
        state->renderer = 0;
    }
    if (state->window)
    {
        SDL_DestroyWindow(state->window);
        state->window = 0;
    }
    if (state->audio_device)
    {
         SDL_CloseAudioDevice(state->audio_device);
         state->audio_device = 0;
    }
    if (state->sdl_inited)
    {
        SDL_Quit();
        state->sdl_inited = false;
    }
}

void
sdl_audio_callback(void *userdata, uint8_t *stream, int32_t len)
{
    sdl2_state *state = (sdl2_state *)userdata;
    sdl2_audio_buffer *buffer = &state->audio_buffer;
    hassert(len > 0);

    uint32_t bytes_from_play_to_end = sizeof(buffer->bytes) - buffer->play_position;

    uint32_t bytes_to_play = bytes_from_play_to_end < (uint32_t)len ? bytes_from_play_to_end : (uint32_t)len;
    memcpy(stream, buffer->bytes + buffer->play_position, bytes_to_play);
    buffer->play_position += bytes_to_play;
    buffer->play_position %= sizeof(buffer->bytes);
    len -= bytes_to_play;

    if (len > 0)
    {
        memcpy(stream, buffer->bytes + buffer->play_position, (uint32_t)len);
        buffer->play_position += len;
        buffer->play_position %= sizeof(buffer->bytes);
    }
}

static bool
find_asset_path(sdl2_state *state)
{
    _snprintf(state->asset_path, sizeof(state->asset_path), "%s%s", state->base_path, "../data");
    return true;
}

static bool
sdl_init(sdl2_state *state)
{
    state->base_path = SDL_GetBasePath();
    find_asset_path(state);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
    {
        const char *error = SDL_GetError();
        hassert(error);
        sdl_cleanup(state);
        return _fail(state, "SDL_Init failed");
    }
    state->sdl_inited = true;

    state->window = SDL_CreateWindow("Hajonta",
            0, 0,
            state->window_width, state->window_height,
            SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL); if (!state->window)
    {
        const char *error = SDL_GetError();
        hassert(error);
        sdl_cleanup(state);
        return _fail(state, "SDL_CreateWindow failed");
    }

    state->renderer = SDL_CreateRenderer(state->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!state->renderer)
    {
        const char *error = SDL_GetError();
        hassert(error);
        sdl_cleanup(state);
        return _fail(state, "SDL_CreateRenderer failed");
    }

    SDL_Surface *surface = SDL_CreateRGBSurface(0, 100, 100, 32, 0, 0, 0, 0);
    if (!surface)
    {
        const char *error = SDL_GetError();
        hassert(error);
        sdl_cleanup(state);
        return _fail(state, "SDL_CreateRGBSurface failed");
    }

    SDL_FillRect(surface, 0, SDL_MapRGB(surface->format, 255, 0, 0));

    state->texture = SDL_CreateTextureFromSurface(state->renderer, surface);
    SDL_FreeSurface(surface);
    if (!state->texture)
    {
        const char *error = SDL_GetError();
        hassert(error);
        sdl_cleanup(state);
        return _fail(state, "SDL_CreateTextureFromSurface failed");
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
    state->context = SDL_GL_CreateContext(state->window);

    SDL_AudioSpec wanted = {};
    wanted.freq = 48000;
    wanted.format = AUDIO_S16;
    wanted.channels = 2;
    wanted.samples = 4096;
    wanted.userdata = (void *)state;
#if !SDL_VERSION_ATLEAST(2,0,4)
    wanted.callback = sdl_audio_callback;
#endif
    state->audio_device = SDL_OpenAudioDevice(0, 0, &wanted, &state->audio_spec, 0);
    if (!state->audio_device)
    {
        const char *error = SDL_GetError();
        hassert(!error);
    }
    SDL_ShowCursor(SDL_DISABLE);

    return true;
}

PLATFORM_FAIL(platform_fail)
{
    _fail((sdl2_state *)ctx, failure_reason);
}

PLATFORM_DEBUG_MESSAGE(platform_debug_message)
{
    SDL_Log("%s\n", message);
}

PLATFORM_GLGETPROCADDRESS(platform_glgetprocaddress)
{
    void *result = SDL_GL_GetProcAddress(function_name);
    return result;
}

PLATFORM_LOAD_ASSET(platform_load_asset)
{
    sdl2_state *state = (sdl2_state *)ctx;

    char *asset_folder_paths[] = {
        state->asset_path,
        state->arg_asset_path,
    };

    SDL_RWops *sdlrw = 0;
    for (uint32_t i = 0; i < harray_count(asset_folder_paths); ++i)
    {
        char *asset_folder_path = asset_folder_paths[i];
        char full_pathname[MAX_PATH];
        if (!asset_folder_path)
        {
            break;
        }
        _snprintf(full_pathname, sizeof(full_pathname), "%s/%s", asset_folder_path, asset_path);
        sdlrw = SDL_RWFromFile(full_pathname, "rb");
        if (sdlrw)
        {
            break;
        }
    }

    if (!sdlrw)
    {
        const char *error = SDL_GetError();
        hassert(!error);
        return false;
    }

    SDL_RWread(sdlrw, dest, 1, size);
    SDL_RWclose(sdlrw);
    return true;
}

bool sdl_load_game_code(sdl2_state *state, game_code *code, const char *filename)
{
    char libfile[1024];
    _snprintf(libfile, sizeof(libfile), "%s%s", state->base_path, filename);
    if (code->handle)
    {
        SDL_UnloadObject(code->handle);
    }
    code->handle = SDL_LoadObject(libfile);
    code->game_update_and_render = (game_update_and_render_func *)SDL_LoadFunction(code->handle, "game_update_and_render");
    hassert(code->game_update_and_render);
    return true;
}

bool
sdl_load_renderer_code(sdl2_state *state, renderer_code *code, const char *filename)
{
    char libfile[1024];
    _snprintf(libfile, sizeof(libfile), "%s%s", state->base_path, filename);
    if (code->handle)
    {
        SDL_UnloadObject(code->handle);
    }
    code->handle = SDL_LoadObject(libfile);
    code->renderer_setup = (renderer_setup_func *)SDL_LoadFunction(code->handle, "renderer_setup");
    code->renderer_render = (renderer_render_func *)SDL_LoadFunction(code->handle, "renderer_render");
    hassert(code->renderer_setup);
    hassert(code->renderer_render);
    return true;
}

static void
sdl2_process_keypress(game_button_state *new_button_state, bool was_down, bool is_down)
{
    if (was_down == is_down)
    {
        return;
    }

    if(new_button_state->ended_down != is_down)
    {
        new_button_state->ended_down = is_down;
        new_button_state->repeat = false;
    }
}

static void
handle_sdl2_events(sdl2_state *state)
{
    SDL_Event sdl_event;
    game_controller_state *new_keyboard_controller = get_controller(state->new_input, 0);

    while(SDL_PollEvent(&sdl_event))
    {
        switch (sdl_event.type)
        {
            case SDL_QUIT:
            {
                state->stopping = true;
            } break;
            case SDL_KEYDOWN:
            case SDL_KEYUP:
            {
                bool was_down = (sdl_event.type == SDL_KEYUP);
                bool is_down = (sdl_event.type != SDL_KEYUP);

                game_button_state *button_state = 0;
                switch(sdl_event.key.keysym.sym)
                {
                    case SDLK_w:
                    {
                        button_state = &new_keyboard_controller->buttons.move_up;
                    } break;
                    case SDLK_s:
                    {
                        button_state = &new_keyboard_controller->buttons.move_down;
                    } break;
                    case SDLK_a:
                    {
                        button_state = &new_keyboard_controller->buttons.move_left;
                    } break;
                    case SDLK_d:
                    {
                        button_state = &new_keyboard_controller->buttons.move_right;
                    } break;
                    case SDLK_ESCAPE:
                    {
                        button_state = &new_keyboard_controller->buttons.back;
                    } break;
                    case SDLK_RETURN:
                    case SDLK_RETURN2:
                    {
                        button_state = &new_keyboard_controller->buttons.start;
                    } break;
                }
                if (button_state)
                {
                    sdl2_process_keypress(button_state, was_down, is_down);
                }
            } break;
            case SDL_MOUSEMOTION:
            {
                state->new_input->mouse.x = sdl_event.motion.x;
                state->new_input->mouse.y = sdl_event.motion.y;
            } break;
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            {
                bool was_down = (sdl_event.type == SDL_MOUSEBUTTONUP);
                bool is_down = (sdl_event.type != SDL_MOUSEBUTTONUP);
                game_button_state *button_state = 0;
                switch (sdl_event.button.button)
                {
                    case SDL_BUTTON_LEFT:
                    {
                        button_state = &state->new_input->mouse.buttons.left;
                    } break;
                    case SDL_BUTTON_RIGHT:
                    {
                        button_state = &state->new_input->mouse.buttons.right;
                    } break;
                    case SDL_BUTTON_MIDDLE:
                    {
                        button_state = &state->new_input->mouse.buttons.middle;
                    } break;
                }
                if (button_state)
                {
                    sdl2_process_keypress(button_state, was_down, is_down);
                }
            } break;
        }
    }
}

void
sdl_queue_sound(sdl2_state *state, game_sound_output *sound_output)
{
    bool free_samples = false;
    if (state->audio_device)
    {
        uint32_t sample_size = (16 / 8);
        uint32_t len = sound_output->number_of_samples * sound_output->channels * sample_size;
        void *samples = sound_output->samples;
        if (!samples)
        {
            samples = calloc(1, len);
            free_samples = true;
        }
#if SDL_VERSION_ATLEAST(2,0,4)
        SDL_QueueAudio(state->audio_device, samples, len);
#else
        sdl2_audio_buffer *buffer = &state->audio_buffer;
        while (len)
        {
            uint32_t bytes_to_write = len;
            if ((sizeof(buffer->bytes) - buffer->write_position) < len)
            {
                bytes_to_write = sizeof(buffer->bytes) - buffer->write_position;
            }
            memcpy(buffer->bytes + buffer->write_position, samples, bytes_to_write);
            len -= bytes_to_write;
            buffer->write_position += bytes_to_write;
            buffer->write_position %= sizeof(buffer->bytes);
        }
#endif
        if (!state->audio_started)
        {
            SDL_PauseAudioDevice(state->audio_device, 0);
            state->audio_started = true;
        }
        if (free_samples)
        {
             free(samples);
        }
    }
}

int
#ifdef _WIN32
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
#else
main(int argc, char *argv[])
#endif
{
    sdl2_state state = {};
    state.window_width = 1920;
    state.window_height = 1080;
    sdl_init(&state);

    platform_memory memory = {};
    memory.size = 256 * 1024 * 1024;
    memory.memory = malloc(memory.size);
    memory.cursor_settings.supported_modes[(uint32_t)platform_cursor_mode::normal] = true;
    memory.cursor_settings.supported_modes[(uint32_t)platform_cursor_mode::unlimited] = false;
    memory.cursor_settings.mode = platform_cursor_mode::normal;

    memory.platform_fail = platform_fail;
    memory.platform_glgetprocaddress = platform_glgetprocaddress;
    memory.platform_load_asset = platform_load_asset;
    memory.platform_debug_message = platform_debug_message;

    game_input input[2] = {};
    state.new_input = &input[0];
    state.old_input = &input[1];

    game_code code = {};
    renderer_code renderercode = {};

    sdl_load_game_code(&state, &code, hquoted(HAJONTA_LIBRARY_NAME));
    sdl_load_renderer_code(&state, &renderercode, hquoted(HAJONTA_RENDERER_LIBRARY_NAME));

    char *arg_asset = getenv("HAJONTA_ASSET_PATH");
    if (arg_asset)
    {
        strncpy(state.arg_asset_path, arg_asset, sizeof(state.arg_asset_path));
    }

    while (!state.stopping)
    {
        state.new_input->delta_t = 1.0f / 60.0f;

        game_controller_state *old_keyboard_controller = get_controller(state.old_input, 0);
        game_controller_state *new_keyboard_controller = get_controller(state.new_input, 0);
        *new_keyboard_controller = {};
        new_keyboard_controller->is_active = true;
        for (
                uint32_t button_index = 0;
                button_index < harray_count(new_keyboard_controller->_buttons);
                ++button_index)
        {
            new_keyboard_controller->_buttons[button_index].ended_down =
                old_keyboard_controller->_buttons[button_index].ended_down;
            new_keyboard_controller->_buttons[button_index].repeat = true;
        }

        state.new_input->mouse.is_active = true;
        for (
                uint32_t button_index = 0;
                button_index < harray_count(state.new_input->mouse._buttons);
                ++button_index)
        {
            state.new_input->mouse._buttons[button_index].ended_down = state.old_input->mouse._buttons[button_index].ended_down;
            state.new_input->mouse._buttons[button_index].repeat = true;
        }

        state.new_input->mouse.x = state.old_input->mouse.x;
        state.new_input->mouse.y = state.old_input->mouse.y;
        state.new_input->mouse.vertical_wheel_delta = 0;

        handle_sdl2_events(&state);

        state.new_input->window.width = state.window_width;
        state.new_input->window.height = state.window_height;

        renderercode.renderer_setup((hajonta_thread_context *)&state, &memory, state.new_input);

        game_sound_output sound_output;
        sound_output.samples_per_second = 48000;
        sound_output.channels = 2;
        sound_output.number_of_samples = 48000 / 60;

        code.game_update_and_render((hajonta_thread_context *)&state, &memory, state.new_input, &sound_output);

        sdl_queue_sound(&state, &sound_output);

        renderercode.renderer_render((hajonta_thread_context *)&state, &memory, state.new_input);

        SDL_GL_SwapWindow(state.window);

        game_input *temp_input = state.new_input;
        state.new_input = state.old_input;
        state.old_input = temp_input;

        for (uint32_t input_idx = 0;
                input_idx < harray_count(state.new_input->keyboard_inputs);
                ++input_idx)
        {
            *(state.new_input->keyboard_inputs + input_idx) = {};
        }
        state.new_input->mouse = {};

        if (memory.quit)
        {
            state.stopping = 1;
        }
    }

    hassert(!state.stop_reason);

    sdl_cleanup(&state);
    return 0;
}
