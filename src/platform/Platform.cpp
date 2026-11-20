// Peekstep - small platform-specific helpers.
#include "platform/Platform.h"

#include <iterator>

#include <GLFW/glfw3.h>

#if !defined(_WIN32) && !defined(__APPLE__)
#include "platform/WindowIconData.h"
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#else
#include <spawn.h>
#include <sys/wait.h>
extern char** environ;
#endif

namespace peekstep {

void openUrl(const std::string& url)
{
#ifdef _WIN32
    const int len = MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, nullptr, 0);
    std::wstring w(size_t(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, w.data(), len);
    ShellExecuteW(nullptr, L"open", w.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
#ifdef __APPLE__
    const char* tool = "open";
#else
    const char* tool = "xdg-open";
#endif
    char* argv[] = {const_cast<char*>(tool), const_cast<char*>(url.c_str()), nullptr};
    pid_t pid = 0;
    if (posix_spawnp(&pid, tool, nullptr, nullptr, argv, environ) == 0) {
        int status = 0;
        waitpid(pid, &status, 0); // both tools return right away
    }
#endif
}

#ifndef __APPLE__
void installOpenFileHandler() {}
std::vector<std::string> takeOpenedFiles() { return {}; }
#endif

void setWindowIdentityHints()
{
#ifdef GLFW_WAYLAND_APP_ID
    // Matches peekstep.desktop, so Wayland compositors show its icon.
    glfwWindowHintString(GLFW_WAYLAND_APP_ID, "peekstep");
#endif
    glfwWindowHintString(GLFW_X11_CLASS_NAME, "peekstep");
    glfwWindowHintString(GLFW_X11_INSTANCE_NAME, "peekstep");
}

void setWindowIcon(GLFWwindow* window)
{
#if !defined(_WIN32) && !defined(__APPLE__)
#ifdef GLFW_PLATFORM_WAYLAND
    // Wayland has no per-window icons (GLFW would report an error).
    if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND)
        return;
#endif
    GLFWimage images[std::size(peekstep_icon::sizes)];
    for (size_t i = 0; i < std::size(images); ++i) {
        images[i].width = images[i].height = peekstep_icon::sizes[i].px;
        images[i].pixels = const_cast<unsigned char*>(peekstep_icon::sizes[i].rgba);
    }
    glfwSetWindowIcon(window, int(std::size(images)), images);
#else
    (void)window;
#endif
}

const char* platformName()
{
#if defined(_WIN32)
    return "Windows x64";
#elif defined(__APPLE__)
    return "macOS arm64";
#elif defined(__aarch64__)
    return "Linux arm64";
#else
    return "Linux x64";
#endif
}

} // namespace peekstep
