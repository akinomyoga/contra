#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

int main() {
  // Setup X11
  Display *display = ::XOpenDisplay(NULL);
  if (display == NULL) {
    fprintf(stderr, "contra: Failed to open DISPLAY=%s\n", std::getenv("DISPLAY"));
    return 1;
  }
  int const screen = DefaultScreen(display);
  Window const root = RootWindow(display, screen);

  unsigned long const black = BlackPixel(display, screen);
  unsigned long const white = WhitePixel(display, screen);

  // Main Window
  Window main_window = XCreateSimpleWindow(display, root, 100, 100, 300, 120, 1, black, white);
  {
    // Properties
    XTextProperty win_name;
    win_name.value = (unsigned char*) "Contra/X11";
    win_name.encoding = XA_STRING;
    win_name.format = 8;
    win_name.nitems = std::strlen((char const*) win_name.value);
    ::XSetWMName(display, main_window, &win_name);

    // Events
    XSelectInput(display, main_window, ExposureMask);
  }
  XMapWindow(display, main_window);
  XFlush(display);

  GC gc = XCreateGC(display, main_window, 0, 0);
  for (;;) {
    XEvent event;
    XNextEvent(display, &event);
    switch (event.type) {
    case Expose:
      if (event.xexpose.count == 0) {
        const char* message = "Hello, world!";
        XDrawString(display, main_window, gc, 3, 15, message, std::strlen(message));
        XFlush(display);
      }
      break;
    }
  }

  return 0;
}
