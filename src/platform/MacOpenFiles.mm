// Peekstep - files opened from Finder on macOS.
//
// Finder sends "open documents" Apple Events to the application delegate.
// GLFW's delegate does not handle them, so we add application:openFiles:
// to its class before glfwInit() creates it. Events that launched the app
// are delivered while glfwInit() finishes launching, later ones while the
// app waits for events.
#include "platform/Platform.h"

#import <Cocoa/Cocoa.h>
#include <GLFW/glfw3.h>
#include <objc/runtime.h>

#include <mutex>
#include <utility>

namespace {

std::mutex gMutex;
std::vector<std::string> gFiles;

void openFiles(id, SEL, NSApplication* app, NSArray<NSString*>* filenames)
{
    {
        std::lock_guard lock(gMutex);
        for (NSString* name in filenames)
            gFiles.emplace_back([name fileSystemRepresentation]);
    }
    [app replyToOpenOrPrint:NSApplicationDelegateReplySuccess];
    glfwPostEmptyEvent(); // wake the main loop (harmless during glfwInit)
}

} // namespace

namespace peekstep {

void installOpenFileHandler()
{
    Class cls = objc_getClass("GLFWApplicationDelegate");
    if (cls)
        class_addMethod(cls, @selector(application:openFiles:), (IMP)openFiles, "v@:@@");
}

void showErrorDialog(const std::string& text)
{
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp activateIgnoringOtherApps:YES];
        NSAlert* alert = [[NSAlert alloc] init];
        alert.alertStyle = NSAlertStyleCritical;
        alert.messageText = @"Peekstep";
        alert.informativeText = [NSString stringWithUTF8String:text.c_str()];
        [alert runModal];
        [alert release];
    }
}

std::vector<std::string> takeOpenedFiles()
{
    std::lock_guard lock(gMutex);
    return std::exchange(gFiles, {});
}

} // namespace peekstep
