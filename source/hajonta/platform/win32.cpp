#include <stddef.h>
#include <stdio.h>

#include <windows.h>
#include <windowsx.h>
#include <gl/gl.h>
#include <xaudio2.h>
#pragma warning (push)
#pragma warning (disable: 4471)
#pragma warning (disable: 4278)
#pragma warning (disable: 4917)
#include <Shobjidl.h>
#include <Shlwapi.h>
#pragma warning (pop)

#include "hajonta/platform/common.h"
#include "hajonta/platform/win32.h"

struct win32_mouse_state
{
    bool in_window;
};

struct win32_state
{
    int32_t stopping;
    bool keyboard_mode;
    char *stop_reason;

    int window_width;
    int window_height;

    HDC device_context;
    HWND window;
    WINDOWPLACEMENT previous_window_placement;

    char binary_path[MAX_PATH];
    char asset_path[MAX_PATH];
    char arg_asset_path[MAX_PATH];

    game_input *new_input;
    game_input *old_input;

    win32_mouse_state mouse;
    platform_cursor_mode cursor_mode;
};

static bool
find_asset_path(win32_state *state)
{
    char dir[sizeof(state->binary_path)];
    strcpy(dir, state->binary_path);

    while(char *location_of_last_slash = strrchr(dir, '\\')) {
        *location_of_last_slash = 0;

        strcpy(state->asset_path, dir);
        strcat(state->asset_path, "\\data");
        WIN32_FILE_ATTRIBUTE_DATA data;
        if (!GetFileAttributesEx(state->asset_path, GetFileExInfoStandard, &data))
        {
            continue;
        }
        return true;
    }
    return false;
}

struct game_code
{
    game_update_and_render_func *game_update_and_render;

    HMODULE game_code_module;
    FILETIME last_updated;
};

struct renderer_code
{
    renderer_setup_func *renderer_setup;
    renderer_render_func *renderer_render;

    HMODULE renderer_code_module;
    FILETIME last_updated;
};

LRESULT CALLBACK
main_window_callback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    win32_state *state;
    if (uMsg == WM_NCCREATE)
    {
        state = (win32_state *)(((LPCREATESTRUCT)lParam)->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
    }
    else
    {
        state = (win32_state *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (!state)
        {
            // This message is allowed to arrive before WM_NCCREATE (!)
            if (uMsg != WM_GETMINMAXINFO)
            {
                char debug_string[1024] = {};
                _snprintf(debug_string, 1023, "Non-WM_NCCREATE without any state set: %d\n", uMsg);
                OutputDebugStringA(debug_string);
            }
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }
    }

    LRESULT lResult = 0;

    switch (uMsg)
    {
        case WM_CREATE:
        {
            PIXELFORMATDESCRIPTOR pixel_format_descriptor = {
                sizeof(PIXELFORMATDESCRIPTOR),
                1,
                PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
                PFD_TYPE_RGBA,
                32,
                0, 0, 0, 0, 0, 0,
                0,
                0,
                0,
                0, 0, 0, 0,
                24,
                8,
                0,
                PFD_MAIN_PLANE,
                0,
                0, 0, 0
            };

            state->device_context = GetDC(hwnd);
            if (!state->device_context)
            {
                MessageBoxA(0, "Unable to acquire device context", 0, MB_OK | MB_ICONSTOP);
                state->stopping = 1;
                return 1;
            }

            int pixel_format = ChoosePixelFormat(state->device_context, &pixel_format_descriptor);
            if (!pixel_format)
            {
                MessageBoxA(0, "Unable to choose requested pixel format", 0, MB_OK | MB_ICONSTOP);
                state->stopping = 1;
                return 1;
            }

            if(!SetPixelFormat(state->device_context, pixel_format, &pixel_format_descriptor))
            {
                MessageBoxA(0, "Unable to set pixel format", 0, MB_OK | MB_ICONSTOP);
                state->stopping = 1;
                return 1;
            }

            HGLRC wgl_context = wglCreateContext(state->device_context);
            if(!wgl_context)
            {
                MessageBoxA(0, "Unable to create wgl context", 0, MB_OK | MB_ICONSTOP);
                state->stopping = 1;
                return 1;
            }

            if(!wglMakeCurrent(state->device_context, wgl_context))
            {
                MessageBoxA(0, "Unable to make wgl context current", 0, MB_OK | MB_ICONSTOP);
                state->stopping = 1;
                return 1;
            }

            wglCreateContextAttribsARB =
                (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");

            if(!wglCreateContextAttribsARB)
            {
                MessageBoxA(0, "Unable to get function pointer to wglCreateContextAttribsARB", 0, MB_OK | MB_ICONSTOP);
                state->stopping = 1;
                return 1;
            }

            int context_attribs[] =
            {
                WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
                WGL_CONTEXT_MINOR_VERSION_ARB, 2,
                WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
                WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
                0
            };

            HGLRC final_context = wglCreateContextAttribsARB(state->device_context, 0, context_attribs);

            if(!final_context)
            {
                MessageBoxA(0, "Unable to upgrade to OpenGL 3.2", 0, MB_OK | MB_ICONSTOP);
                state->stopping = 1;
                return 1;
            }

            wglMakeCurrent(0, 0);
            wglDeleteContext(wgl_context);
            wglMakeCurrent(state->device_context, final_context);

            glViewport(0, 0, 960, 540);
        } break;
        case WM_SIZE:
        {
            OutputDebugStringA("WM_SIZE\n");
            int32_t window_width = LOWORD(lParam);
            int32_t window_height = HIWORD(lParam);
            state->window_height = window_height;
            state->window_width = window_width;
            glViewport(0, 0, window_width, window_height);
            if (state->cursor_mode == platform_cursor_mode::unlimited)
            {
                RECT window_rect = {};
                GetClientRect(hwnd, &window_rect);
                ClientToScreen(hwnd, (LPPOINT)&window_rect.left);
                ClientToScreen(hwnd, (LPPOINT)&window_rect.right);
                ClipCursor(&window_rect);
            }
        } break;
        case WM_CLOSE:
        {
            PostQuitMessage(0);
        } break;
        case WM_SETFOCUS:
        {
            OutputDebugStringA("WM_SETFOCUS\n");
            if (state->cursor_mode == platform_cursor_mode::unlimited)
            {
                RECT window_rect = {};
                GetClientRect(hwnd, &window_rect);
                ClientToScreen(hwnd, (LPPOINT)&window_rect.left);
                ClientToScreen(hwnd, (LPPOINT)&window_rect.right);
                ClipCursor(&window_rect);
            }
        } break;
        case WM_KILLFOCUS:
        {
            OutputDebugStringA("WM_KILLFOCUS\n");
            ClipCursor(0);
        } break;
        case WM_ACTIVATE:
        {
            RECT window_rect = {};
            GetClientRect(hwnd, &window_rect);
            ClientToScreen(hwnd, (LPPOINT)&window_rect.left);
            ClientToScreen(hwnd, (LPPOINT)&window_rect.right);

            switch (LOWORD(wParam))
            {
                case WA_INACTIVE:
                {
                    OutputDebugStringA("WM_ACTIVATE with WA_INACTIVE\n");
                    // ClipCursor(0);
                } break;
                case WA_ACTIVE:
                {
                    OutputDebugStringA("WM_ACTIVATE with WA_ACTIVE\n");
                    // ClipCursor(&window_rect);
                } break;
                case WA_CLICKACTIVE:
                {
                    OutputDebugStringA("WM_ACTIVATE with WA_CLICKACTIVE\n");
                    //ClipCursor(&window_rect);
                } break;
            }
        } break;
        default:
        {
            lResult = DefWindowProc(hwnd, uMsg, wParam, lParam);
        } break;
    }
    return lResult;
}

static void
win32_process_keypress(game_button_state *new_button_state, bool was_down, bool is_down)
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
handle_win32_messages(win32_state *state)
{
    game_controller_state *new_keyboard_controller = get_controller(state->new_input, 0);

    MSG message;
    while(!state->stopping && PeekMessage(&message, 0, 0, 0, PM_REMOVE))
    {
        switch(message.message)
        {
            case WM_QUIT:
            {
                state->stopping = true;
            } break;

            case WM_CHAR:
            {
                char c = 0;
                switch (message.wParam)
                {
                    /*
                    case 0x08: // backspace
                    {
                    } break;
                    case 0x0A: // linefeed
                    {
                    } break;
                    case 0x1B: // escape
                    {
                    } break;
                    case 0x09: // tab
                    {
                    } break;
                    case 0x0D: // carriage return
                    {
                    } break;
                    */
                    case '`':
                    {
                        state->keyboard_mode = false;
                    } break;
                    default:
                    {
                        c = (char)message.wParam;
                    } break;
                }
                if (!c)
                {
                    break;
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
            } break;

            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
                WPARAM vkcode = message.wParam;
                /*
                    30 The previous key state. The value is 1 if the key is
                       down before the message is sent, or it is zero if the
                       key is up.
                    31 The transition state. The value is always 0 for a WM_KEYDOWN message.
                */
                bool was_down = ((message.lParam & (1 << 30)) != 0);
                bool is_down = ((message.lParam & (1 << 31)) == 0);
                bool alt_was_down = ((message.lParam & (1 << 29)) != 0);
                if (!state->keyboard_mode)
                {
                    switch(vkcode)
                    {
                        case VK_RETURN:
                        {
                            if (alt_was_down)
                            {
                                if (!is_down)
                                {
                                    break;
                                }
                                LONG dwStyle = GetWindowLong(state->window, GWL_STYLE);
                                if (dwStyle & WS_OVERLAPPEDWINDOW) {
                                    MONITORINFO mi = { sizeof(mi) };
                                    if (GetWindowPlacement(state->window, &state->previous_window_placement) &&
                                       GetMonitorInfo(MonitorFromWindow(state->window,
                                                      MONITOR_DEFAULTTOPRIMARY), &mi)) {
                                     SetWindowLong(state->window, GWL_STYLE,
                                                   dwStyle & ~WS_OVERLAPPEDWINDOW);
                                     SetWindowPos(state->window, HWND_TOP,
                                                  mi.rcMonitor.left, mi.rcMonitor.top,
                                                  mi.rcMonitor.right - mi.rcMonitor.left,
                                                  mi.rcMonitor.bottom - mi.rcMonitor.top,
                                                  SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
                                   }
                                 } else {
                                   SetWindowLong(state->window, GWL_STYLE,
                                                 dwStyle | WS_OVERLAPPEDWINDOW);
                                   SetWindowPlacement(state->window, &state->previous_window_placement);
                                   SetWindowPos(state->window, NULL, 0, 0, 0, 0,
                                                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                                                SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
                                }
                            }
                            else
                            {
                                win32_process_keypress(&new_keyboard_controller->buttons.start, was_down, is_down);
                            }
                        } break;
                        case VK_ESCAPE:
                        {
                            win32_process_keypress(&new_keyboard_controller->buttons.back, was_down, is_down);
                        } break;
                        case 'W':
                        {
                            win32_process_keypress(&new_keyboard_controller->buttons.move_up, was_down, is_down);
                        } break;
                        case 'A':
                        {
                            win32_process_keypress(&new_keyboard_controller->buttons.move_left, was_down, is_down);
                        } break;
                        case 'S':
                        {
                            win32_process_keypress(&new_keyboard_controller->buttons.move_down, was_down, is_down);
                        } break;
                        case 'D':
                        {
                            win32_process_keypress(&new_keyboard_controller->buttons.move_right, was_down, is_down);
                        } break;
                        case VK_OEM_3:
                        {
                            if (is_down)
                            {
                                state->keyboard_mode = true;
                            }
                        } break;
                        default:
                        {
                        } break;
                    }
                }
                else
                {
                    TranslateMessage(&message);
                    DispatchMessageA(&message);
                }
            } break;
            case WM_LBUTTONUP:
            {
                int x_pos = GET_X_LPARAM(message.lParam);
                int y_pos = GET_Y_LPARAM(message.lParam);
                char dbg[1024] = {};
                sprintf(dbg, "WM_LBUTTONUP at %dx%d\n", x_pos, y_pos);
                OutputDebugStringA(dbg);
                //ReleaseCapture();
                TranslateMessage(&message);
                DispatchMessageA(&message);
                win32_process_keypress(&state->new_input->mouse.buttons.left, true, false);
            } break;
            case WM_LBUTTONDOWN:
            {
                int x_pos = GET_X_LPARAM(message.lParam);
                int y_pos = GET_Y_LPARAM(message.lParam);
                char dbg[1024] = {};
                sprintf(dbg, "WM_LBUTTONDOWN at %dx%d\n", x_pos, y_pos);
                OutputDebugStringA(dbg);
                //SetCapture(state->window);
                TranslateMessage(&message);
                DispatchMessageA(&message);
                win32_process_keypress(&state->new_input->mouse.buttons.left, false, true);
            } break;
            case WM_RBUTTONUP:
            {
                int x_pos = GET_X_LPARAM(message.lParam);
                int y_pos = GET_Y_LPARAM(message.lParam);
                char dbg[1024] = {};
                sprintf(dbg, "WM_RBUTTONUP at %dx%d\n", x_pos, y_pos);
                OutputDebugStringA(dbg);
                TranslateMessage(&message);
                DispatchMessageA(&message);
                win32_process_keypress(&state->new_input->mouse.buttons.right, true, false);
            } break;
            case WM_RBUTTONDOWN:
            {
                int x_pos = GET_X_LPARAM(message.lParam);
                int y_pos = GET_Y_LPARAM(message.lParam);
                char dbg[1024] = {};
                sprintf(dbg, "WM_RBUTTONDOWN at %dx%d\n", x_pos, y_pos);
                OutputDebugStringA(dbg);
                //SetCapture(state->window);
                TranslateMessage(&message);
                DispatchMessageA(&message);
                win32_process_keypress(&state->new_input->mouse.buttons.right, false, true);
            } break;
            case WM_MBUTTONUP:
            {
                int x_pos = GET_X_LPARAM(message.lParam);
                int y_pos = GET_Y_LPARAM(message.lParam);
                char dbg[1024] = {};
                sprintf(dbg, "WM_MBUTTONUP at %dx%d\n", x_pos, y_pos);
                OutputDebugStringA(dbg);
                TranslateMessage(&message);
                DispatchMessageA(&message);
                win32_process_keypress(&state->new_input->mouse.buttons.middle, true, false);
            } break;
            case WM_MBUTTONDOWN:
            {
                int x_pos = GET_X_LPARAM(message.lParam);
                int y_pos = GET_Y_LPARAM(message.lParam);
                char dbg[1024] = {};
                sprintf(dbg, "WM_MBUTTONDOWN at %dx%d\n", x_pos, y_pos);
                OutputDebugStringA(dbg);
                //SetCapture(state->window);
                TranslateMessage(&message);
                DispatchMessageA(&message);
                win32_process_keypress(&state->new_input->mouse.buttons.middle, false, true);
            } break;
            case WM_MOUSEWHEEL:
            {
                int16_t zDelta = GET_WHEEL_DELTA_WPARAM(message.wParam);
                char dbg[1024] = {};
                state->new_input->mouse.vertical_wheel_delta += zDelta;
                sprintf(dbg, "WM_MOUSEWHEEL of %d, total of %d\n", zDelta, state->new_input->mouse.vertical_wheel_delta);
                OutputDebugStringA(dbg);
                //SetCapture(state->window);
                TranslateMessage(&message);
                DispatchMessageA(&message);
                win32_process_keypress(&state->new_input->mouse.buttons.middle, false, true);
            } break;
            case WM_MOUSELEAVE:
            {
                int x_pos = GET_X_LPARAM(message.lParam);
                int y_pos = GET_Y_LPARAM(message.lParam);
                char dbg[1024] = {};
                sprintf(dbg, "WM_MOUSELEAVE at %dx%d\n", x_pos, y_pos);
                OutputDebugStringA(dbg);
                state->mouse.in_window = false;
            } break;
            case WM_MOUSEMOVE:
            {
                int x_pos = GET_X_LPARAM(message.lParam);
                int y_pos = GET_Y_LPARAM(message.lParam);
                state->new_input->mouse.x = x_pos;
                state->new_input->mouse.y = y_pos;

                if (!state->mouse.in_window)
                {
                    state->mouse.in_window = true;
                    TRACKMOUSEEVENT tme = { sizeof(tme) };
                    tme.dwFlags = TME_LEAVE;
                    tme.hwndTrack = state->window;
                    TrackMouseEvent(&tme);
                    char dbg[1024] = {};
                    sprintf(dbg, "WM_MOUSEMOVE at %dx%d\n", x_pos, y_pos);
                    OutputDebugStringA(dbg);
                }
            } break;
            default:
            {
                TranslateMessage(&message);
                DispatchMessageA(&message);
            }
        }
    }

    if (state->cursor_mode == platform_cursor_mode::unlimited) {
        state->new_input->mouse.x -= (state->window_width / 2);
        state->new_input->mouse.y -= (state->window_height / 2);
        POINT pos = { state->window_width / 2, state->window_height  / 2};
        ClientToScreen(state->window, &pos);
        SetCursorPos(pos.x, pos.y);
    }
}

PLATFORM_FAIL(platform_fail)
{
    win32_state *state = (win32_state *)ctx;
    state->stopping = 1;
    state->stop_reason = strdup(failure_reason);
}

PLATFORM_DEBUG_MESSAGE(win32_debug_message)
{
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
}

PLATFORM_GLGETPROCADDRESS(platform_glgetprocaddress)
{
    return wglGetProcAddress(function_name);
}

PLATFORM_LOAD_ASSET(win32_load_asset)
{
    win32_state *state = (win32_state *)ctx;

    char *asset_folder_paths[] = {
        state->asset_path,
        state->arg_asset_path,
    };

    HANDLE handle = INVALID_HANDLE_VALUE;
    for (uint32_t i = 0; i < harray_count(asset_folder_paths); ++i)
    {
        char *asset_folder_path = asset_folder_paths[i];
        char full_pathname[MAX_PATH];
        _snprintf(full_pathname, sizeof(full_pathname), "%s\\%s", asset_folder_path, asset_path);
        handle = CreateFile(full_pathname, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        if (handle != INVALID_HANDLE_VALUE)
        {
            break;
        }
    }
    if (handle == INVALID_HANDLE_VALUE)
    {
        MessageBoxA(0, "Failed", 0, MB_OK | MB_ICONSTOP);
        return false;
    }
    LARGE_INTEGER _size;
    GetFileSizeEx(handle, &_size);
    if (_size.QuadPart > size)
    {
        char msg[1024];
        sprintf(msg, "File size mismatch: Got %lld, max %d", _size.QuadPart, size);
        MessageBoxA(0, msg, 0, MB_OK | MB_ICONSTOP);
        return false;
    }
    DWORD bytes_read;
    if (!ReadFile(handle, dest, size, &bytes_read, 0))
    {
        return false;
    }
    return true;
}

PLATFORM_EDITOR_LOAD_FILE(win32_editor_load_file)
{
    bool result = false;
    IFileOpenDialog *pFileOpen;

    // Create the FileOpenDialog object.
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
            IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

    if (SUCCEEDED(hr))
    {
        // Show the Open dialog box.
        hr = pFileOpen->Show(NULL);

        // Get the file name from the dialog box.
        if (SUCCEEDED(hr))
        {
            IShellItem *pItem;
            hr = pFileOpen->GetResult(&pItem);
            if (SUCCEEDED(hr))
            {
                PWSTR pszFilePath;
                hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

                // Display the file name to the user.
                if (SUCCEEDED(hr))
                {
                    HANDLE handle = CreateFileW(pszFilePath, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
                    if (handle != INVALID_HANDLE_VALUE)
                    {
                        LARGE_INTEGER _size;
                        GetFileSizeEx(handle, &_size);
                        void *buffer = malloc((size_t)_size.QuadPart);
                        DWORD bytes_read;
                        if (ReadFile(handle, buffer, (DWORD)_size.QuadPart, &bytes_read, 0))
                        {
                            target->contents = (char *)buffer;
                            target->size = (uint32_t)_size.QuadPart;
                            wcstombs(target->file_path, pszFilePath, sizeof(target->file_path));
                            result = true;
                        }
                        CloseHandle(handle);
                    }
                    else
                    {
                        MessageBoxA(0, "Failed to open file", 0, MB_OK | MB_ICONSTOP);
                    }
                    CoTaskMemFree(pszFilePath);
                }
                pItem->Release();
            }
        }
        pFileOpen->Release();
    }
    return result;
}

PLATFORM_EDITOR_LOAD_NEARBY_FILE(win32_editor_load_nearby_file)
{
    bool result = false;
    char new_path[MAX_PATH];
    if (PathIsRelative(name))
    {
        char existing_path[MAX_PATH];
        strcpy(existing_path, existing_file.file_path);
        PathRemoveFileSpec(existing_path);
        auto combine_result = PathCombine(new_path, existing_path, name);
        if (!combine_result)
        {
            return false;
        }
    }
    else
    {
        strcpy(new_path, name);
    }

    HANDLE handle = CreateFile(new_path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (handle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER _size;
        GetFileSizeEx(handle, &_size);
        void *buffer = malloc((size_t)_size.QuadPart);
        DWORD bytes_read;
        if (ReadFile(handle, buffer, (DWORD)_size.QuadPart, &bytes_read, 0))
        {
            target->contents = (char *)buffer;
            target->size = (uint32_t)_size.QuadPart;
            strncpy(target->file_path, new_path, sizeof(target->file_path));
            result = true;
        }
        CloseHandle(handle);
    }
    return result;
}

bool win32_load_game_code(game_code *code, char *filename)
{
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesEx(filename, GetFileExInfoStandard, &data))
    {
        return false;
    }
    FILETIME last_updated = data.ftLastWriteTime;
    if (CompareFileTime(&last_updated, &code->last_updated) != 1)
    {
        return true;
    }

    char locksuffix[] = ".lck";
    char lockfilename[MAX_PATH];
    strcpy(lockfilename, filename);
    strcat(lockfilename, locksuffix);
    if (GetFileAttributesEx(lockfilename, GetFileExInfoStandard, &data))
    {
        return false;
    }

    char tempsuffix[] = ".tmp";
    char tempfilename[MAX_PATH];
    strcpy(tempfilename, filename);
    strcat(tempfilename, tempsuffix);
    if (code->game_code_module)
    {
        FreeLibrary(code->game_code_module);
    }
    CopyFile(filename, tempfilename, 0);
    HMODULE new_module = LoadLibraryA(tempfilename);
    if (!new_module)
    {
        return false;
    }

    game_update_and_render_func *game_update_and_render = (game_update_and_render_func *)GetProcAddress(new_module, "game_update_and_render");
    if (!game_update_and_render)
    {
        return false;
    }

    code->game_code_module = new_module;
    code->last_updated = last_updated;
    code->game_update_and_render = game_update_and_render;
    return true;
}

bool win32_load_renderer_code(renderer_code *code, char *filename)
{
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesEx(filename, GetFileExInfoStandard, &data))
    {
        return false;
    }
    FILETIME last_updated = data.ftLastWriteTime;
    if (CompareFileTime(&last_updated, &code->last_updated) != 1)
    {
        return true;
    }

    char locksuffix[] = ".lck";
    char lockfilename[MAX_PATH];
    strcpy(lockfilename, filename);
    strcat(lockfilename, locksuffix);
    if (GetFileAttributesEx(lockfilename, GetFileExInfoStandard, &data))
    {
        return false;
    }

    char tempsuffix[] = ".tmp";
    char tempfilename[MAX_PATH];
    strcpy(tempfilename, filename);
    strcat(tempfilename, tempsuffix);
    if (code->renderer_code_module)
    {
        FreeLibrary(code->renderer_code_module);
    }
    CopyFile(filename, tempfilename, 0);
    HMODULE new_module = LoadLibraryA(tempfilename);
    if (!new_module)
    {
        return false;
    }

    renderer_setup_func *renderer_setup = (renderer_setup_func *)GetProcAddress(new_module, "renderer_setup");
    if (!renderer_setup)
    {
        return false;
    }

    renderer_render_func *renderer_render = (renderer_render_func *)GetProcAddress(new_module, "renderer_render");
    if (!renderer_render)
    {
        return false;
    }

    code->renderer_code_module = new_module;
    code->last_updated = last_updated;
    code->renderer_setup = renderer_setup;
    code->renderer_render = renderer_render;
    return true;
}

int CALLBACK
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    win32_state state = {};

    WNDCLASSEXA window_class = {};
    window_class.cbSize = sizeof(WNDCLASSEXA);
    window_class.lpfnWndProc = main_window_callback;
    window_class.hInstance = hInstance;
    window_class.lpszClassName = "HajontaMainWindow";
    window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;

    if (!RegisterClassExA(&window_class))
    {
        MessageBoxA(0, "Unable to register window class", 0, MB_OK | MB_ICONSTOP);
        return 1;
    }

    DWORD style;
    style = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

    state.window_width = 960;
    state.window_height = 540;
    RECT create_window_rect = {};
    create_window_rect.left = 0;
    create_window_rect.top = 0;
    create_window_rect.right = create_window_rect.left + state.window_width;
    create_window_rect.bottom = create_window_rect.top + state.window_height;
    BOOL result = AdjustWindowRect(&create_window_rect, style, 0);
    if (!result)
    {
        MessageBoxA(0, "Unable to determine window size", 0, MB_OK | MB_ICONSTOP);
        return 1;
    }

    HWND window = CreateWindowExA(
        0,
        "HajontaMainWindow",
        "Hajonta",
        style,
        0,
        0,
        create_window_rect.right - create_window_rect.left,
        create_window_rect.bottom - create_window_rect.top,
        0,
        0,
        hInstance,
        (LPVOID)&state
    );

    if (!window)
    {
        MessageBoxA(0, "Unable to create window", 0, MB_OK | MB_ICONSTOP);
        return 1;
    }

    state.window = window;
    state.previous_window_placement = { sizeof(state.previous_window_placement) };

    ShowWindow(window, nCmdShow);
    ShowCursor(0);

    wglSwapIntervalEXT =
        (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
    wglSwapIntervalEXT(1);

    platform_memory memory = {};
    memory.size = 256 * 1024 * 1024;
    memory.memory = malloc(memory.size);
    memory.cursor_settings.supported_modes[(uint32_t)platform_cursor_mode::normal] = true;
    memory.cursor_settings.supported_modes[(uint32_t)platform_cursor_mode::unlimited] = true;
    memory.cursor_settings.mode = platform_cursor_mode::normal;

    memory.platform_fail = platform_fail;
    memory.platform_glgetprocaddress = platform_glgetprocaddress;
    memory.platform_load_asset = win32_load_asset;
    memory.platform_debug_message = win32_debug_message;
    memory.platform_editor_load_file = win32_editor_load_file;
    memory.platform_editor_load_nearby_file = win32_editor_load_nearby_file;

    GetModuleFileNameA(0, state.binary_path, sizeof(state.binary_path));
    char game_library_path[sizeof(state.binary_path)];
    char *location_of_last_slash = strrchr(state.binary_path, '\\');
    strncpy(game_library_path, state.binary_path, (size_t)(location_of_last_slash - state.binary_path));
    strcat(game_library_path, "\\");
#if defined(HAJONTA_LIBRARY_NAME)
    strcat(game_library_path, hquoted(HAJONTA_LIBRARY_NAME));
#else
    strcat(game_library_path, "game.dll");
#endif

    char renderer_library_path[sizeof(state.binary_path)];
    strncpy(renderer_library_path, state.binary_path, (size_t)(location_of_last_slash - state.binary_path));
    strcat(renderer_library_path, "\\");
#if defined(HAJONTA_RENDERER_LIBRARY_NAME)
    strcat(renderer_library_path, hquoted(HAJONTA_RENDERER_LIBRARY_NAME));
#else
    strcat(renderer_library_path, "renderer.dll");
#endif

    GetEnvironmentVariable("HAJONTA_ASSET_PATH", state.arg_asset_path, sizeof(state.arg_asset_path));

    if (!find_asset_path(&state))
    {
        MessageBoxA(0, "Failed to locate asset path", 0, MB_OK | MB_ICONSTOP);
        return 1;
    }

    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    IXAudio2 *xaudio = {};
    if(FAILED(XAudio2Create(&xaudio, 0))) {
        MessageBoxA(0, "Unable to create XAudio2 instance", 0, MB_OK | MB_ICONSTOP);
        return 1;
    }

    IXAudio2MasteringVoice *master_voice = {};
    if(FAILED(xaudio->CreateMasteringVoice(&master_voice, 2, 48000))) {
        MessageBoxA(0, "Unable to create XAudio2 master voice", 0, MB_OK | MB_ICONSTOP);
        return 1;
    }
    WAVEFORMATEX wave_format = {};
    wave_format.wFormatTag = WAVE_FORMAT_PCM;
    wave_format.nChannels = 2;
    wave_format.nSamplesPerSec = 48000;
    wave_format.nAvgBytesPerSec = 48000 * 2 * (16 / 8);
    wave_format.nBlockAlign = 2 * (16 / 8);
    wave_format.wBitsPerSample = 16;

    IXAudio2SourceVoice *source_voice = {};
    xaudio->CreateSourceVoice(&source_voice, &wave_format);

    int audio_offset = 0;

    source_voice->Start(0);

    XAUDIO2_BUFFER audio_buffer = {};
    audio_buffer.Flags |= XAUDIO2_END_OF_STREAM;
    uint8_t intro[48000 * 2 * (16 / 8) / 250] = {};
    audio_buffer.AudioBytes = sizeof(intro);
    audio_buffer.pAudioData = (uint8_t *)intro;
    source_voice->SubmitSourceBuffer(&audio_buffer);

    XAUDIO2_VOICE_STATE voice_state;

    struct foo {
        uint32_t BuffersQueued;
        uint64_t SamplesPlayed;
    };

    foo audio_history[60] = {};

    game_input input[2] = {};
    state.new_input = &input[0];
    state.old_input = &input[1];

    game_code gamecode = {};
    renderer_code renderercode = {};
    while(!state.stopping)
    {
        bool updated = win32_load_game_code(&gamecode, game_library_path);
        if (!gamecode.game_code_module)
        {
            state.stopping = 1;
            state.stop_reason = "Unable to load game code";
        }
        if (!updated)
        {
            state.stopping = 1;
            state.stop_reason = "Unable to reload game code";
        }

        updated = win32_load_renderer_code(&renderercode, renderer_library_path);
        if (!renderercode.renderer_code_module)
        {
            state.stopping = 1;
            state.stop_reason = "Unable to load renderer code";
            break;
        }
        if (!updated)
        {
            state.stopping = 1;
            state.stop_reason = "Unable to reload renderer code";
        }

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

        bool switched_to_unlimited = false;

        if (state.cursor_mode != memory.cursor_settings.mode)
        {
            switch(memory.cursor_settings.mode)
            {
                case platform_cursor_mode::normal:
                {
                    ClipCursor(0);
                } break;
                case platform_cursor_mode::unlimited:
                {
                    switched_to_unlimited = true;
                    RECT window_rect = {};
                    GetClientRect(state.window, &window_rect);
                    ClientToScreen(state.window, (LPPOINT)&window_rect.left);
                    ClientToScreen(state.window, (LPPOINT)&window_rect.right);
                    ClipCursor(&window_rect);
                } break;
                case platform_cursor_mode::COUNT:
                default:
                {
                    hassert(!"Unknown cursor mode");
                } break;
            }
            state.cursor_mode = memory.cursor_settings.mode;
        }
        if (memory.cursor_settings.position_set)
        {
            POINT pos = {
                memory.cursor_settings.mouse_x,
                memory.cursor_settings.mouse_y,
            };
            memory.cursor_settings.position_set = false;
            ClientToScreen(state.window, &pos);
            SetCursorPos(pos.x, pos.y);
        }
        handle_win32_messages(&state);
        if (switched_to_unlimited)
        {
            state.new_input->mouse.x = 0;
            state.new_input->mouse.y = 0;
        }

        state.new_input->window.width = state.window_width;
        state.new_input->window.height = state.window_height;

        renderercode.renderer_setup((hajonta_thread_context *)&state, &memory, state.new_input);

        game_sound_output sound_output;
        sound_output.samples_per_second = 48000;
        sound_output.channels = 2;
        sound_output.number_of_samples = 48000 / 60;
        gamecode.game_update_and_render((hajonta_thread_context *)&state, &memory, state.new_input, &sound_output);

        source_voice->GetState(&voice_state);
        audio_buffer.AudioBytes = 48000 * 2 * (16 / 8) / 60;
        audio_buffer.pAudioData = (uint8_t *)sound_output.samples;

        audio_history[audio_offset].BuffersQueued = voice_state.BuffersQueued;
        audio_history[audio_offset].SamplesPlayed = voice_state.SamplesPlayed;

        audio_offset = (audio_offset + 1) % 60;
        if (audio_offset == 0 && 0)
        {
            char longmsg[16384];
#define BQ(x, y, z) audio_history[(x*y)+z].BuffersQueued
            sprintf(longmsg, "BuffersQueued: "
"%4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d "
"%4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d "
"%4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d "
"%4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d"
"%4d %4d %4d %4d %4d %4d %4d %4d %4d %4d\n",
            BQ(0, 6, 0), BQ(0, 6, 1), BQ(0, 6, 2), BQ(0, 6, 3), BQ(0, 6, 4), BQ(0, 6, 5),
            BQ(1, 6, 0), BQ(1, 6, 1), BQ(1, 6, 2), BQ(1, 6, 3), BQ(1, 6, 4), BQ(1, 6, 5),
            BQ(2, 6, 0), BQ(2, 6, 1), BQ(2, 6, 2), BQ(2, 6, 3), BQ(2, 6, 4), BQ(2, 6, 5),
            BQ(3, 6, 0), BQ(3, 6, 1), BQ(3, 6, 2), BQ(3, 6, 3), BQ(3, 6, 4), BQ(3, 6, 5),
            BQ(4, 6, 0), BQ(4, 6, 1), BQ(4, 6, 2), BQ(4, 6, 3), BQ(4, 6, 4), BQ(4, 6, 5),
            BQ(5, 6, 0), BQ(5, 6, 1), BQ(5, 6, 2), BQ(5, 6, 3), BQ(5, 6, 4), BQ(5, 6, 5),
            BQ(6, 6, 0), BQ(6, 6, 1), BQ(6, 6, 2), BQ(6, 6, 3), BQ(6, 6, 4), BQ(6, 6, 5),
            BQ(7, 6, 0), BQ(7, 6, 1), BQ(7, 6, 2), BQ(7, 6, 3), BQ(7, 6, 4), BQ(7, 6, 5),
            BQ(8, 6, 0), BQ(8, 6, 1), BQ(8, 6, 2), BQ(8, 6, 3), BQ(8, 6, 4), BQ(8, 6, 5),
            BQ(9, 6, 0), BQ(9, 6, 1), BQ(9, 6, 2), BQ(9, 6, 3), BQ(9, 6, 4), BQ(9, 6, 5),
            BQ(10, 6, 0), BQ(10, 6, 1), BQ(10, 6, 2), BQ(10, 6, 3), BQ(10, 6, 4), BQ(10, 6, 5),
            BQ(11, 6, 0), BQ(11, 6, 1), BQ(11, 6, 2), BQ(11, 6, 3), BQ(11, 6, 4), BQ(11, 6, 5),
            BQ(12, 6, 0), BQ(12, 6, 1), BQ(12, 6, 2), BQ(12, 6, 3), BQ(12, 6, 4), BQ(12, 6, 5),
            BQ(13, 6, 0), BQ(13, 6, 1), BQ(13, 6, 2), BQ(13, 6, 3), BQ(13, 6, 4), BQ(13, 6, 5),
            BQ(14, 6, 0), BQ(14, 6, 1), BQ(14, 6, 2), BQ(14, 6, 3), BQ(14, 6, 4), BQ(14, 6, 5));
            OutputDebugStringA(longmsg);
            audio_offset = 0;
#define SP(x, y, z) (int32_t)audio_history[(x*y)+z].SamplesPlayed
            sprintf(longmsg, "SamplesPlayed: "
"%4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d "
"%4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d "
"%4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d"
"%4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d"
"%4d %4d %4d %4d %4d %4d %4d %4d %4d %4d\n",
            SP(0, 6, 0), SP(0, 6, 1), SP(0, 6, 2), SP(0, 6, 3), SP(0, 6, 4), SP(0, 6, 5),
            SP(1, 6, 0), SP(1, 6, 1), SP(1, 6, 2), SP(1, 6, 3), SP(1, 6, 4), SP(1, 6, 5),
            SP(2, 6, 0), SP(2, 6, 1), SP(2, 6, 2), SP(2, 6, 3), SP(2, 6, 4), SP(2, 6, 5),
            SP(3, 6, 0), SP(3, 6, 1), SP(3, 6, 2), SP(3, 6, 3), SP(3, 6, 4), SP(3, 6, 5),
            SP(4, 6, 0), SP(4, 6, 1), SP(4, 6, 2), SP(4, 6, 3), SP(4, 6, 4), SP(4, 6, 5),
            SP(5, 6, 0), SP(5, 6, 1), SP(5, 6, 2), SP(5, 6, 3), SP(5, 6, 4), SP(5, 6, 5),
            SP(6, 6, 0), SP(6, 6, 1), SP(6, 6, 2), SP(6, 6, 3), SP(6, 6, 4), SP(6, 6, 5),
            SP(7, 6, 0), SP(7, 6, 1), SP(7, 6, 2), SP(7, 6, 3), SP(7, 6, 4), SP(7, 6, 5),
            SP(8, 6, 0), SP(8, 6, 1), SP(8, 6, 2), SP(8, 6, 3), SP(8, 6, 4), SP(8, 6, 5),
            SP(9, 6, 0), SP(9, 6, 1), SP(9, 6, 2), SP(9, 6, 3), SP(9, 6, 4), SP(9, 6, 5),
            SP(10, 6, 0), SP(10, 6, 1), SP(10, 6, 2), SP(10, 6, 3), SP(10, 6, 4), SP(10, 6, 5),
            SP(11, 6, 0), SP(11, 6, 1), SP(11, 6, 2), SP(11, 6, 3), SP(11, 6, 4), SP(11, 6, 5),
            SP(12, 6, 0), SP(12, 6, 1), SP(12, 6, 2), SP(12, 6, 3), SP(12, 6, 4), SP(12, 6, 5),
            SP(13, 6, 0), SP(13, 6, 1), SP(13, 6, 2), SP(13, 6, 3), SP(13, 6, 4), SP(13, 6, 5),
            SP(14, 6, 0), SP(14, 6, 1), SP(14, 6, 2), SP(14, 6, 3), SP(14, 6, 4), SP(14, 6, 5));
            OutputDebugStringA(longmsg);
            audio_offset = 0;
        }

        source_voice->SubmitSourceBuffer(&audio_buffer);

        renderercode.renderer_render((hajonta_thread_context *)&state, &memory, state.new_input);
        SwapBuffers(state.device_context);

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
        state.keyboard_mode = memory.debug_keyboard;
    }

    if (state.stop_reason)
    {
        MessageBoxA(0, state.stop_reason, 0, MB_OK | MB_ICONSTOP);
        return 1;
    }

    return 0;
}
