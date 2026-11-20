// Peekstep - small platform-specific helpers.
#pragma once

#include <string>
#include <vector>

namespace peekstep {

// Opens a URL in the user's web browser. Does nothing if that fails.
void openUrl(const std::string& url);

// macOS: files opened from Finder ("Open With", double-click) arrive as
// Apple Events, not on the command line. Call before glfwInit() so files
// that started the app are caught too. Does nothing on other platforms.
void installOpenFileHandler();

// Files that arrived through the handler above since the last call.
std::vector<std::string> takeOpenedFiles();

// macOS: shows an error in an alert window. Does nothing elsewhere (Windows
// already shows a message box, Linux prints to the terminal).
void showErrorDialog(const std::string& text);

// Short description of the platform the binary was built for, e.g. "Windows x64".
const char* platformName();

} // namespace peekstep
