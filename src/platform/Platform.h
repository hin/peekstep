// Peekstep - small platform-specific helpers.
#pragma once

#include <string>
#include <vector>

struct GLFWwindow;

namespace peekstep {

// Opens a URL in the user's web browser. Does nothing if that fails.
void openUrl(const std::string& url);

// macOS: files opened from Finder ("Open With", double-click) arrive as
// Apple Events, not on the command line. Call before glfwInit() so files
// that started the app are caught too. Does nothing on other platforms.
void installOpenFileHandler();

// Files that arrived through the handler above since the last call.
std::vector<std::string> takeOpenedFiles();

// Window hints that tie the window to the app's menu entry and icon
// (Wayland app id, X11 class). Call after glfwInit(), before creating the window.
void setWindowIdentityHints();

// Sets the window's title bar / task switcher icon where the app has to
// provide it itself (Linux X11). Windows uses the icon in the exe, macOS the
// one in the app bundle, Wayland the one named by the menu entry.
void setWindowIcon(GLFWwindow* window);
// macOS: shows an error in an alert window. Does nothing elsewhere (Windows
// already shows a message box, Linux prints to the terminal).
void showErrorDialog(const std::string& text);

// Short description of the platform the binary was built for, e.g. "Windows x64".
const char* platformName();

} // namespace peekstep
