#include "EditorPaletteShell.h"
#include <cstdint>
#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#import <AppKit/AppKit.h>

// Intercept the title-bar close before Cocoa hides the modeless editor.
@interface Synth2PaletteCloseDelegate : NSObject <NSWindowDelegate> {
@public
    bool* closeRequest;
}
@end
@implementation Synth2PaletteCloseDelegate
- (BOOL)windowShouldClose:(NSWindow*)sender {
    (void)sender;
    if(closeRequest)*closeRequest=true;
    return NO;
}
@end
#else
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

namespace synth2::editor::native
{
struct EditorPaletteShell::State
{
    bool closed=false;
#ifdef _WIN32
    HWND window=nullptr;
    WNDPROC original=nullptr;
    static LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM w,LPARAM l) {
        auto* self=reinterpret_cast<State*>(GetWindowLongPtrW(window,GWLP_USERDATA));
        if(message==WM_CLOSE){self->closed=true;return 0;}
        return CallWindowProcW(self->original,window,message,w,l);
    }
#elif defined(__APPLE__)
    NSWindow* window=nil;
    Synth2PaletteCloseDelegate* delegate=nil;
#else
    Display* display=nullptr;
    Window window=0;
    Atom close=0;

#endif
};
EditorPaletteShell::EditorPaletteShell() : state(std::make_unique<State>()) {}
EditorPaletteShell::~EditorPaletteShell()
{
#ifdef _WIN32
    if(state->window)DestroyWindow(state->window);
#elif defined(__APPLE__)
    if(state->window) {
        [state->window setDelegate:nil];
        NSWindow* parent=[state->window parentWindow];
        if(parent)[parent removeChildWindow:state->window];
        [state->window close];[state->window release];
    }
    [state->delegate release];
#else
    if(state->display){if(state->window)XDestroyWindow(state->display,state->window);XCloseDisplay(state->display);}
#endif
}
bool EditorPaletteShell::open(void* owner,int width,int height)
{
#ifdef _WIN32
    const auto parent=GetAncestor(static_cast<HWND>(owner),GA_ROOT);
    RECT bounds{0,0,width,height},ownerBounds{};
    const bool positioned=parent && GetWindowRect(parent,&ownerBounds);
    constexpr DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_CLIPCHILDREN;
    AdjustWindowRectEx(&bounds,style,FALSE,WS_EX_TOOLWINDOW);
    state->window=CreateWindowExW(WS_EX_TOOLWINDOW,L"STATIC",L"Synth2 — Palette",style,
        positioned ? ownerBounds.left+40 : CW_USEDEFAULT,positioned ? ownerBounds.top+40 : CW_USEDEFAULT,
        bounds.right-bounds.left,bounds.bottom-bounds.top,parent,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!state->window)return false;
    SetWindowLongPtrW(state->window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(state.get()));
    state->original=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(state->window,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(&State::procedure)));
    ShowWindow(state->window,SW_SHOW);return true;
#elif defined(__APPLE__)
    state->window=[[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,width,height)
        styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable backing:NSBackingStoreBuffered defer:NO];
    state->delegate=[[Synth2PaletteCloseDelegate alloc] init];
    state->delegate->closeRequest=&state->closed;
    [state->window setDelegate:state->delegate];
    [state->window setReleasedWhenClosed:NO];[state->window setTitle:@"Synth2 — Palette"];
    if(owner) {
        NSWindow* parent=[static_cast<NSView*>(owner) window];
        const auto origin=[parent frame].origin;
        [state->window setFrameTopLeftPoint:NSMakePoint(origin.x+40,origin.y+[parent frame].size.height-40)];
        [parent addChildWindow:state->window ordered:NSWindowAbove];
    } else [state->window center];
    [state->window makeKeyAndOrderFront:nil];return state->window!=nil;
#else
    state->display=XOpenDisplay(nullptr);if(!state->display)return false;
    state->window=XCreateSimpleWindow(state->display,DefaultRootWindow(state->display),60,60,width,height,0,0,0x232226);
    XStoreName(state->display,state->window,"Synth2 — Palette");
    state->close=XInternAtom(state->display,"WM_DELETE_WINDOW",False);
    XSetWMProtocols(state->display,state->window,&state->close,1);
    XSizeHints size{};size.flags=PMinSize|PMaxSize;size.min_width=size.max_width=width;size.min_height=size.max_height=height;
    XSetWMNormalHints(state->display,state->window,&size);
    if(owner) {
        Window root=0,parent=0,*children=nullptr;unsigned count=0;
        Window top=static_cast<Window>(reinterpret_cast<std::uintptr_t>(owner));
        while(XQueryTree(state->display,top,&root,&parent,&children,&count)) {
            if(children){XFree(children);children=nullptr;}
            if(!parent || parent==root)break;top=parent;
        }
        XSetTransientForHint(state->display,state->window,top);
    }
    XSelectInput(state->display,state->window,StructureNotifyMask);
    XMapRaised(state->display,state->window);XFlush(state->display);return true;
#endif
}
void* EditorPaletteShell::parent() const
{
#ifdef _WIN32
    return state->window;
#elif defined(__APPLE__)
    return [state->window contentView];
#else
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(state->window));
#endif
}
void EditorPaletteShell::attach(void* view)
{
#ifdef _WIN32
    RECT client{},bounds{};
    if(GetClientRect(static_cast<HWND>(view),&client)) {
        bounds=client;AdjustWindowRectEx(&bounds,GetWindowLongW(state->window,GWL_STYLE),FALSE,WS_EX_TOOLWINDOW);
        SetWindowPos(state->window,nullptr,0,0,bounds.right-bounds.left,bounds.bottom-bounds.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    }
#elif defined(__APPLE__)
    [state->window setContentSize:[static_cast<NSView*>(view) frame].size];
#else
    const auto child=static_cast<Window>(reinterpret_cast<std::uintptr_t>(view));
    XWindowAttributes attributes{};
    if(XGetWindowAttributes(state->display,child,&attributes)) {
        // The APP SDK offsets all embedded views for its menu. This owner has
        // no menu, and its fixed client size follows the child's actual DPI.
        XMoveWindow(state->display,child,0,0);
        XSizeHints size{};size.flags=PMinSize|PMaxSize;
        size.min_width=size.max_width=attributes.width;size.min_height=size.max_height=attributes.height;
        XSetWMNormalHints(state->display,state->window,&size);
        XResizeWindow(state->display,state->window,attributes.width,attributes.height);XFlush(state->display);
    }
#endif
}
bool EditorPaletteShell::takeCloseRequest()
{
#if !defined(_WIN32) && !defined(__APPLE__)
    while(state->display && XPending(state->display)) {
        XEvent event;XNextEvent(state->display,&event);
        if(event.type==ClientMessage && static_cast<Atom>(event.xclient.data.l[0])==state->close)state->closed=true;
        if(event.type==DestroyNotify){state->closed=true;state->window=0;}
    }
#endif
    const bool requested=state->closed;state->closed=false;
    return requested;
}
void EditorPaletteShell::raise()
{
#ifdef _WIN32
    SetForegroundWindow(state->window);
#elif defined(__APPLE__)
    [state->window makeKeyAndOrderFront:nil];
#else
    if(state->display){XMapRaised(state->display,state->window);XFlush(state->display);}
#endif
}
}
