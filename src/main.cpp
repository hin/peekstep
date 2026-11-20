// Peekstep - a fast-starting STEP file viewer.
// SPDX-License-Identifier: BSD-2-Clause

#include "app/Viewer.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

// Command line arguments as UTF-8 (on Windows, argv is in the ANSI code page).
std::vector<std::string> utf8Args(int argc, char** argv)
{
    std::vector<std::string> args;
#ifdef _WIN32
    (void)argc;
    (void)argv;
    int n = 0;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &n);
    for (int i = 0; i < n; ++i) {
        const int len = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, nullptr, 0, nullptr, nullptr);
        std::string s(size_t(len > 0 ? len - 1 : 0), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, s.data(), len, nullptr, nullptr);
        args.push_back(std::move(s));
    }
    LocalFree(wargv);
#else
    args.assign(argv, argv + argc);
#endif
    return args;
}

void showMessage(const std::string& text, bool error)
{
#ifdef _WIN32
    // A GUI app on Windows has no console to print to.
    const int len = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    std::wstring w(size_t(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, w.data(), len);
    MessageBoxW(nullptr, w.c_str(), L"Peekstep", MB_OK | (error ? MB_ICONERROR : MB_ICONINFORMATION));
#else
    std::fprintf(error ? stderr : stdout, "%s\n", text.c_str());
#endif
}

int printInfo(const std::string& file)
{
    try {
        const peekstep::Model m = peekstep::loadStep(file);
        std::printf("File:      %s\n", file.c_str());
        std::printf("Parts:     %zu\nFaces:     %zu\nTriangles: %zu\nVertices:  %zu\n", m.parts, m.faces,
                    m.indices.size() / 3, m.vertices.size());
        std::printf("Size:      %.3f x %.3f x %.3f mm\n", m.bboxMax[0] - m.bboxMin[0], m.bboxMax[1] - m.bboxMin[1],
                    m.bboxMax[2] - m.bboxMin[2]);
        std::printf("Read:      %.3f s\nMesh:      %.3f s\n", m.readSeconds, m.meshSeconds);
        std::printf("Colors:\n");
        for (const peekstep::ColorInfo& c : m.colors)
            std::printf("  #%02X%02X%02X  %zu faces\n", int(c.rgba[0] * 255 + 0.5f), int(c.rgba[1] * 255 + 0.5f),
                        int(c.rgba[2] * 255 + 0.5f), c.faces);
        std::printf("Materials:\n");
        for (const peekstep::MaterialInfo& mat : m.materials)
            std::printf("  %s%s\n", mat.name.c_str(), mat.visual ? " (appearance)" : "");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "peekstep: %s\n", e.what());
        return 1;
    }
}

const char* kUsage = "Usage: peekstep [file.step]\n"
                     "\n"
                     "Opens a STEP file for a quick look. Files can also be dropped on the window.\n"
                     "\n"
                     "  --info         load the file, print what is in it and exit (no window)\n"
                     "  -h, --help     show this help\n"
                     "  -v, --version  show the version";

} // namespace

int main(int argc, char** argv)
{
    const std::vector<std::string> args = utf8Args(argc, argv);
    std::string file;
    bool infoOnly = false;
    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "-h" || a == "--help") {
            showMessage(kUsage, false);
            return 0;
        }
        if (a == "-v" || a == "--version") {
            showMessage("Peekstep " PEEKSTEP_VERSION, false);
            return 0;
        }
        if (a == "--info") {
            infoOnly = true;
            continue;
        }
        if (!a.empty() && a[0] == '-' && a != "-") {
            showMessage("Unknown option: " + a + "\n\n" + kUsage, true);
            return 2;
        }
        file = a;
    }

    if (infoOnly) {
        if (file.empty()) {
            showMessage(kUsage, true);
            return 2;
        }
        return printInfo(file);
    }

    try {
        peekstep::Viewer viewer;
        if (!file.empty())
            viewer.open(file); // loads on a worker thread while the window comes up
        viewer.run();
        if (viewer.loading())
            std::_Exit(0); // don't wait for a load nobody wants any more
    } catch (const std::exception& e) {
        showMessage(std::string("Peekstep could not start: ") + e.what(), true);
        return 1;
    }
    return 0;
}
