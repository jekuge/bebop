#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_WINDOWS 100

typedef struct {
    Window win;
    int x, y, w, h;
} Client;

Display *display;
Window root;
Client *clients[MAX_WINDOWS];
int num_clients = 0;

void add_window(Window w) {
    if (num_clients >= MAX_WINDOWS) return;

    XWindowAttributes wa;
    XGetWindowAttributes(display, w, &wa);

    Client *c = malloc(sizeof(Client));
    c->win = w;
    c->x = wa.x;
    c->y = wa.y;
    c->w = wa.width;
    c->h = wa.height;

    clients[num_clients++] = c;
    XSelectInput(display, w, EnterWindowMask | FocusChangeMask);
    XMapWindow(display, w);
}

void remove_window(Window w) {
    for (int i = 0; i < num_clients; i++) {
        if (clients[i]->win == w) {
            free(clients[i]);
            for (int j = i; j < num_clients - 1; j++) {
                clients[j] = clients[j + 1];
            }
            num_clients--;
            break;
        }
   }
}

void tile_windows() {
    if (num_clients == 0) return;

    int screen_width = DisplayWidth(display, DefaultScreen(display));
    int screen_height = DisplayHeight(display, DefaultScreen(display));
    int w_per_win = screen_width / num_clients;

    for (int i =0; i < num_clients; i++) {
        clients[i]->x = i * w_per_win;
        clients[i]->y = 0;
        clients[i]->w = w_per_win;
        clients[i]->h = screen_height;
        XMoveResizeWindow(display, clients[i]->win, clients[i]->x, clients[i]->y,
            clients[i]->w, clients[i]->h);
    }
}

void spawn(const char *cmd) {
    if (fork() == 0) {
        execlp("sh","sh", "-c", cmd, (char *)NULL);
        exit(1);
    }
}

void handle_keypress(XKeyEvent *e) {
    KeySym keysym = XLookupKeysym(e, 0);
    if (keysym == XK_q && (e->state & ControlMask)) {
        XCloseDisplay(display);
        exit(0);
    } else if (keysym == XK_Return && (e->state & Mod4Mask)) {
        spawn("kitty");
    }
}

void handle_configure_request(XConfigureRequestEvent *e) {
    XWindowChanges wc;
    wc.x = e->x;
    wc.y = e->y;
    wc.width = e->width;
    wc.height = e->height;
    wc.border_width = e->border_width;
    wc.sibling = e->above;
    wc.stack_mode = e->detail;

    XConfigureWindow(display, e->window, e->value_mask, &wc);
    tile_windows();
}

int main() {
    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Cannot open display\n");
        return 1;
    }

    root = DefaultRootWindow(display);
    XSelectInput(display, root, SubstructureRedirectMask | SubstructureNotifyMask | KeyPressMask);
    XGrabKey(display, XKeysymToKeycode(display, XK_q), ControlMask, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_Return), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);

    XEvent ev;
    spawn("kitty");
    while(1) {
        XNextEvent(display, &ev);
        switch (ev.type) {
            case MapRequest:
                add_window(ev.xmaprequest.window);
                tile_windows();
                break;
            case DestroyNotify:
                remove_window(ev.xdestroywindow.window);
                tile_windows();
                break;
            case KeyPress:
                handle_keypress(&ev.xkey);
                break;
            case ConfigureRequest:
                handle_configure_request(&ev.xconfigurerequest);
                break;

        }
    }

    XCloseDisplay(display);
    return 0;
}
