#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <libproc.h>
#include <dlfcn.h>

#include <sys/syslimits.h>

#include <OpenGL/gl.h>
#include "hajonta/platform/common.h"
#include "hajonta/platform/osx.h"

void
get_binary_name(osx_state *state)
{
    pid_t pid = getpid();
    proc_pidpath(pid, state->binary_name, sizeof(state->binary_name));
    for (char *c = state->binary_name; *c; ++c)
    {
        if (*c == '/')
        {
            state->last_slash = c + 1;
        }
    }
}

void
cat_strings(
    size_t src_a_count, char *src_a,
    size_t src_b_count, char *src_b,
    size_t dest_count, char *dest
    )
{
    size_t counter = 0;
    for (
            size_t i = 0;
            i < src_a_count && counter++ < dest_count;
            ++i)
    {
        *dest++ = *src_a++;
    }
    for (
            size_t i = 0;
            i < src_b_count && counter++ < dest_count;
            ++i)
    {
        *dest++ = *src_b++;
    }

    *dest++ = 0;
}

void
build_full_filename(osx_state *state, char *filename, int dest_count, char *dest)
{
    cat_strings(state->last_slash -
            state->binary_name, state->binary_name, strlen(filename), filename,
            dest_count, dest);
}

void
process_keyboard_message(game_button_state *new_state, bool is_down)
{
    if (new_state->ended_down != is_down)
    {
        new_state->ended_down = is_down;
        new_state->repeat = false;
    }
}

void
load_game(osx_game_code *game_code, char *path)
{
    struct stat statbuf = {};
    uint32_t stat_result = stat(path, &statbuf);
    if (stat_result != 0)
    {
        printf("Failed to stat game code at %s\n", path);
        return;
    }
    game_code->last_updated = statbuf.st_mtime;

    game_code->library_handle = dlopen(path, RTLD_LAZY);
    if (game_code->library_handle == 0)
    {
        char *error = dlerror();
        printf("Unable to load library at path %s: %s\n", path, error);
        return;
    }
    game_code->game_update_and_render =
        (game_update_and_render_func *)dlsym(game_code->library_handle, "game_update_and_render");
    if (game_code->game_update_and_render == 0)
    {
        char *error = dlerror();
        printf("Unable to load symbol game_update_and_render: %s\n", error);
        free(error);
        _exit(1);
    }
}

PLATFORM_LOAD_ASSET(osx_load_asset)
{
    osx_state *state = (osx_state *)ctx;
    char full_pathname[PATH_MAX];
    snprintf(full_pathname, sizeof(full_pathname), "%s/%s", state->asset_path, asset_path);

    FILE *asset = fopen(full_pathname, "r");
    fseek(asset, 0, SEEK_END);
    uint32_t file_size = ftell(asset);
    fseek(asset, 0, SEEK_SET);
    int size_read = fread(dest, 1, size, asset);
    return true;
}

PLATFORM_FAIL(osx_fail)
{
    printf("%s\n", failure_reason);
    _exit(1);
}

PLATFORM_DEBUG_MESSAGE(osx_debug_message)
{
    printf("%s\n", message);
}

static bool
find_asset_path(osx_state *state)
{
    char dir[sizeof(state->binary_name)];
    strcpy(dir, state->binary_name);

    while(char *location_of_last_slash = strrchr(dir, '/')) {
        *location_of_last_slash = 0;

        strcpy(state->asset_path, dir);
        strcat(state->asset_path, "/data");
        struct stat statbuf = {};
        uint32_t stat_result = stat(state->asset_path, &statbuf);
        if (stat_result != 0)
        {
            continue;
        }
        return true;
    }
    return false;
}

void
osx_init(osx_state *state)
{
    *state = {};
    get_binary_name(state);
    find_asset_path(state);

    char *game_code_filename = (char *)"libgame.dylib";
    build_full_filename(state, game_code_filename,
            sizeof(state->library_path),
            state->library_path);

    load_game(&state->game_code, state->library_path);

    state->memory.size = 64 * 1024 * 1024;
    state->memory.memory = calloc(state->memory.size, sizeof(uint8_t));
    state->memory.platform_fail = osx_fail;
    state->memory.platform_load_asset = osx_load_asset;
    state->memory.platform_debug_message = osx_debug_message;

    state->new_input = &state->inputs[0];
    state->old_input = &state->inputs[1];
}

void
loop_cycle(osx_state *state)
{
    state->new_input->delta_t = 1.0/60.0;

    if (state->stopping)
    {
        return;
    }

    game_sound_output sound_output;
    sound_output.samples_per_second = 48000;
    sound_output.channels = 2;
    sound_output.number_of_samples = 48000 / 60;

    state->game_code.game_update_and_render((hajonta_thread_context *)state, &state->memory, state->new_input, &sound_output);

    game_input *temp_input = state->new_input;
    state->new_input = state->old_input;
    state->old_input = temp_input;

    game_controller_state *old_keyboard_controller = get_controller(state->old_input, 0);
    game_controller_state *new_keyboard_controller = get_controller(state->new_input, 0);
    *new_keyboard_controller = {};
    new_keyboard_controller->is_active = true;
    for (
            uint button_index = 0;
            button_index < harray_count(new_keyboard_controller->_buttons);
            ++button_index)
    {
        new_keyboard_controller->_buttons[button_index].ended_down =
            old_keyboard_controller->_buttons[button_index].ended_down;
        new_keyboard_controller->_buttons[button_index].repeat = true;
    }

    for (uint32_t input_idx = 0;
            input_idx < harray_count(state->new_input->keyboard_inputs);
            ++input_idx)
    {
        *(state->new_input->keyboard_inputs + input_idx) = {};
    }

    if (state->memory.quit)
    {
        state->stopping = true;
    }
}

void
osx_process_character(osx_state *state, char c)
{
    if (c == '`')
    {
        state->keyboard_mode = false;
        return;
    }
    keyboard_input *k = state->new_input->keyboard_inputs;
    for (uint32_t idx = 0;
            idx < harray_count(state->new_input->keyboard_inputs);
            ++idx)
    {
        keyboard_input *ki = k + idx;
        if (ki->type == keyboard_input_type::NONE)
        {
            ki->type = keyboard_input_type::ASCII;
            ki->ascii = c;
            break;
        }
    }
}

void
osx_process_key_event(osx_state *state, int key_code, bool is_down)
{
    game_controller_state *keyboard = get_controller(state->new_input, 0);
    switch (key_code)
    {
        case 13:
        {
            process_keyboard_message(&keyboard->buttons.move_up, is_down);
        } break;
        case 0:
        {
            process_keyboard_message(&keyboard->buttons.move_left, is_down);
        } break;
        case 1:
        {
            process_keyboard_message(&keyboard->buttons.move_down, is_down);
        } break;
        case 2:
        {
            process_keyboard_message(&keyboard->buttons.move_right, is_down);
        } break;
        case 53:
        {
            process_keyboard_message(&keyboard->buttons.back, is_down);
        } break;
        case 36:
        {
            process_keyboard_message(&keyboard->buttons.start, is_down);
        } break;
        case 50:
        {
            if (!is_down)
            {
                state->keyboard_mode = true;
            }
        } break;
        default:
        {
            printf("Unrecognized key: %d\n", key_code);
        } break;
    }
}
