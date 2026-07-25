#ifndef APP_H
#define APP_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>

#include "camera.h"

// Owns the window, the GL context and all per-frame input state.
//
// GLFW callbacks are plain C function pointers, so they reach this object through the window's
// user pointer rather than through globals. Construct this before anything that creates GL
// objects, so those are destroyed while the context is still alive.
class App
{
public:
    Camera camera;

    App(int width, int height, const char* title)
        : camera(glm::vec3(0.0f, 0.0f, 3.0f)), fbWidth(width), fbHeight(height)
    {
        if (!glfwInit()) {
            std::cout << "Failed to initialize GLFW" << "\n";
            return;
        }
        glfwInitialized = true;

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        window = glfwCreateWindow(width, height, title, nullptr, nullptr);
        if (window == nullptr) {
            std::cout << "Failed to create window" << "\n";
            return;
        }

        glfwMakeContextCurrent(window);
        glfwSwapInterval(1); // cap to vsync so the compute shader doesn't peg the GPU at 100% uncapped
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, onFramebufferSize);
        glfwSetCursorPosCallback(window, onCursorPos);
        glfwSetScrollCallback(window, onScroll);
        glfwSetWindowFocusCallback(window, onFocus);

        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            std::cout << "Failed to initialize GLAD" << "\n";
            return;
        }

        if (!GLAD_GL_ARB_bindless_texture) {
            std::cout << "GL_ARB_bindless_texture not supported on this GPU/driver" << "\n";
            return;
        }

        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        ready = true;
    }

    ~App()
    {
        if (glfwInitialized) {
            glfwTerminate();
        }
    }

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    bool ok() const { return ready; }
    bool shouldClose() const { return glfwWindowShouldClose(window); }

    int width() const  { return fbWidth; }
    int height() const { return fbHeight; }

    // Seconds since startup, sampled once at the top of the current frame.
    float time() const { return lastFrame; }

    // Seconds elapsed during the previous frame, for rate-independent adjustments.
    float delta() const { return deltaTime; }

    // Raw key state, so callers can bind their own controls without App having to know
    // what they do.
    bool isKeyDown(int key) const { return glfwGetKey(window, key) == GLFW_PRESS; }

    // True exactly once after the framebuffer changes size, so the caller can reallocate targets.
    bool consumeResize()
    {
        bool r = resized;
        resized = false;
        return r;
    }

    // Advances the frame clock and applies keyboard input to the camera.
    void beginFrame()
    {
        float now = static_cast<float>(glfwGetTime());
        deltaTime = now - lastFrame;
        lastFrame = now;
        processKeyboard();
    }

    void endFrame()
    {
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

private:
    GLFWwindow* window = nullptr;
    bool glfwInitialized = false;
    bool ready = false;

    int fbWidth = 0;
    int fbHeight = 0;
    bool resized = false;

    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    float lastX = 0.0f;
    float lastY = 0.0f;
    bool initialMouse = true;

    static App& from(GLFWwindow* w)
    {
        return *static_cast<App*>(glfwGetWindowUserPointer(w));
    }

    void processKeyboard()
    {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            camera.ProcessKeyboard(FORWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            camera.ProcessKeyboard(BACKWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            camera.ProcessKeyboard(LEFT, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            camera.ProcessKeyboard(RIGHT, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            camera.ProcessKeyboard(DOWN, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            camera.ProcessKeyboard(UP, deltaTime);
    }

    static void onFramebufferSize(GLFWwindow* w, int width, int height)
    {
        App& app = from(w);
        app.fbWidth = width;
        app.fbHeight = height;
        app.resized = true;
    }

    static void onCursorPos(GLFWwindow* w, double xposIn, double yposIn)
    {
        App& app = from(w);
        float xpos = static_cast<float>(xposIn);
        float ypos = static_cast<float>(yposIn);

        if (app.initialMouse) {
            app.lastX = xpos;
            app.lastY = ypos;
            app.initialMouse = false;
        }

        float xoffset = xpos - app.lastX;
        float yoffset = app.lastY - ypos; // reversed since y-coordinates go from bottom to top

        app.lastX = xpos;
        app.lastY = ypos;

        app.camera.ProcessMouseMovement(xoffset, yoffset);
    }

    static void onScroll(GLFWwindow* w, double /*xoffset*/, double yoffset)
    {
        from(w).camera.ProcessMouseScroll(static_cast<float>(yoffset));
    }

    // With the cursor disabled, GLFW recentres it when the window gains focus. Without this the
    // next cursor event reports the jump as real mouse movement and swings the camera, which
    // makes a scene's authored viewpoint impossible to reproduce.
    static void onFocus(GLFWwindow* w, int focused)
    {
        if (focused) {
            from(w).initialMouse = true;
        }
    }
};

#endif
