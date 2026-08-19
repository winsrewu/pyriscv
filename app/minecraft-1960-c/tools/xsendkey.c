/* xsendkey -- inject key presses into the MINECRAFT 1960 window.
 * Test helper: usage  xsendkey r d w ...  (keysym names)
 */
#include <X11/Xlib.h>
#include <string.h>
#include <stdio.h>

static Window find(Display *d, Window w)
{
  Window root, parent, *kids = 0, hit = 0;
  unsigned int n = 0, i;
  char *name = 0;
  if (XFetchName(d, w, &name)) {
    int ok = name && strstr(name, "MINECRAFT") != 0;
    if (name) XFree(name);
    if (ok) return w;
  }
  if (XQueryTree(d, w, &root, &parent, &kids, &n) && kids) {
    for (i = 0; i < n && !hit; i++) hit = find(d, kids[i]);
    XFree(kids);
  }
  return hit;
}

int main(int ac, char **av)
{
  Display *d = XOpenDisplay(0);
  Window w;
  int i;
  if (!d) return 1;
  w = find(d, DefaultRootWindow(d));
  if (!w) { fprintf(stderr, "no MINECRAFT window\n"); return 1; }
  for (i = 1; i < ac; i++) {
    XKeyEvent ev;
    KeyCode kc = XKeysymToKeycode(d, XStringToKeysym(av[i]));
    memset(&ev, 0, sizeof(ev));
    ev.display = d;
    ev.window = w;
    ev.root = DefaultRootWindow(d);
    ev.subwindow = w;
    ev.same_screen = True;
    ev.keycode = kc;
    ev.type = KeyPress;
    XSendEvent(d, w, True, KeyPressMask, (XEvent *)&ev);
    ev.type = KeyRelease;
    XSendEvent(d, w, True, KeyReleaseMask, (XEvent *)&ev);
  }
  XFlush(d);
  return 0;
}
