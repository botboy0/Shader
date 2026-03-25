#include "app.h"

#include <cstdio>
#include <cstring>
#include <glad.h>
#include <SDL3/SDL.h>

namespace magnetar {

App::App(const std::string& title, int width, int height)
    : m_title(title), m_width(width), m_height(height)
{
}

App::~App()
{
    shutdown();
}

bool App::init()
{
    // --- SDL init ---
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "[App] Error: SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    fprintf(stderr, "[App] SDL3 initialized\n");

    // --- GL attributes (must be set before window creation) ---
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    // --- Create window ---
    m_window = SDL_CreateWindow(
        m_title.c_str(),
        m_width, m_height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );
    if (!m_window) {
        fprintf(stderr, "[App] Error: SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }
    fprintf(stderr, "[App] Window created (%dx%d)\n", m_width, m_height);

    // --- GL context ---
    m_glCtx = SDL_GL_CreateContext(m_window);
    if (!m_glCtx) {
        fprintf(stderr, "[App] Error: SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
        SDL_Quit();
        return false;
    }
    fprintf(stderr, "[App] OpenGL context created\n");

    // --- Load GL functions via glad ---
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        fprintf(stderr, "[App] Error: gladLoadGLLoader failed\n");
        SDL_GL_DestroyContext(m_glCtx);
        m_glCtx = nullptr;
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
        SDL_Quit();
        return false;
    }

    // --- Report GL info ---
    const char* glVersion  = (const char*)glGetString(GL_VERSION);
    const char* glRenderer = (const char*)glGetString(GL_RENDERER);
    fprintf(stderr, "[App] GL Version:  %s\n", glVersion  ? glVersion  : "unknown");
    fprintf(stderr, "[App] GL Renderer: %s\n", glRenderer ? glRenderer : "unknown");

    // --- Enable vsync by default ---
    SDL_GL_SetSwapInterval(1);

    // --- Start high-precision timer ---
    m_perfFrequency = SDL_GetPerformanceFrequency();
    m_startCounter  = SDL_GetPerformanceCounter();
    fprintf(stderr, "[App] Timer started (freq=%llu)\n", (unsigned long long)m_perfFrequency);

    return true;
}

void App::run()
{
    m_running = true;
    m_lastTitleUpdate = SDL_GetPerformanceCounter();
    fprintf(stderr, "[App] Entering main loop\n");

    while (m_running) {
        m_frameControl.beginFrame();

        processEvents();

        if (m_updateCallback) {
            m_updateCallback();
        }

        SDL_GL_SwapWindow(m_window);

        m_frameControl.endFrame();

        // Update window title every ~500ms
        Uint64 now = SDL_GetPerformanceCounter();
        double elapsed = static_cast<double>(now - m_lastTitleUpdate)
                       / static_cast<double>(m_perfFrequency);
        if (elapsed >= 0.5) {
            updateWindowTitle();
            m_lastTitleUpdate = now;
        }
    }

    fprintf(stderr, "[App] Main loop exited\n");
}

float App::getElapsedTime() const
{
    Uint64 now = SDL_GetPerformanceCounter();
    return static_cast<float>(static_cast<double>(now - m_startCounter) / static_cast<double>(m_perfFrequency));
}

void App::processEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // Forward to external callback (ImGui event processing) before internal handling
        if (m_eventCallback) {
            m_eventCallback(event);
        }

        switch (event.type) {
            case SDL_EVENT_QUIT:
                m_running = false;
                break;

            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
                int w = event.window.data1;
                int h = event.window.data2;
                if (w > 0 && h > 0 && (w != m_width || h != m_height)) {
                    m_width  = w;
                    m_height = h;
                    fprintf(stderr, "[App] Window resized to %dx%d\n", m_width, m_height);
                    if (m_resizeCallback) {
                        m_resizeCallback(m_width, m_height);
                    }
                }
                break;
            }

            default:
                break;
        }
    }
}

void App::updateWindowTitle()
{
    float fps = m_frameControl.getFps();
    const char* vsyncStr = m_frameControl.isVsyncEnabled() ? "ON" : "OFF";
    int cap = m_frameControl.getFramerateCap();

    char titleBuf[256];
    if (cap > 0) {
        snprintf(titleBuf, sizeof(titleBuf),
                 "Magnetar Shader Editor | FPS: %.0f | VSync: %s | Cap: %d",
                 fps, vsyncStr, cap);
    } else {
        snprintf(titleBuf, sizeof(titleBuf),
                 "Magnetar Shader Editor | FPS: %.0f | VSync: %s | Cap: none",
                 fps, vsyncStr);
    }
    SDL_SetWindowTitle(m_window, titleBuf);
}

void App::shutdown()
{
    if (!m_glCtx && !m_window) return; // already shut down

    if (m_glCtx) {
        SDL_GL_DestroyContext(m_glCtx);
        m_glCtx = nullptr;
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    SDL_Quit();
    fprintf(stderr, "[App] Clean shutdown\n");
}

} // namespace magnetar
