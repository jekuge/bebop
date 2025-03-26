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
int focused = -1;

void add_window(Window w) {
    if (num_clients >= MAX_WINDOWS) return;

    XWindowAttributes wa;
    XGetWindowAttributes(display, w, &wa);

    Client *c = malloc(sizeof(Client));
    if (!c) {
        fprintf(stderr, "Failed to allocate client\n");
        return;
    }

    c->win = w;
    c->x = wa.x;
    c->y = wa.y;
    c->w = wa.width;
    c->h = wa.height;


    clients[num_clients++] = c;
    XSelectInput(display, w, EnterWindowMask | FocusChangeMask);
    XMapWindow(display, w);

    if (focused == -1 && num_clients == 1) {
        focused = 0;
        XSetInputFocus(display, w, RevertToPointerRoot, CurrentTime);
        XRaiseWindow(display, w);
    }
}

void remove_window(Window w) {
    for (int i = 0; i < num_clients; i++) {
        if (clients[i]->win == w) {
            free(clients[i]);
            for (int j = i; j < num_clients - 1; j++) {
                clients[j] = clients[j + 1];
            }
            num_clients--;
            if (focused == i) {
                focused = (num_clients > 0) ? (i > 0 ? i - 1 : 0) : -1;
                if (focused >= 0) {
                    XSetInputFocus(display, clients[focused]->win, RevertToPointerRoot, CurrentTime);
                    XRaiseWindow(display, clients[focused]->win);
                }
            } else if (focused > 1) {
                focused--;
            }
            break;
        }
   }
}

void cleanup() {
    for (int i = 0; i < num_clients; i++) {
        free(clients[i]);
        XCloseDisplay(display);
    }
}


void tile_windows() {
    if (num_clients == 0) return;

    int screen_width = DisplayWidth(display, DefaultScreen(display));
    int screen_height = DisplayHeight(display, DefaultScreen(display));

    if (num_clients == 1) {
        XMoveResizeWindow(display, clients[0]->win, 0, 0, screen_width, screen_height);
        return;
    }

    int master_width = screen_width / 2;
    int stack_height = screen_height / (num_clients - 1);
    XMoveResizeWindow(display, clients[0]->win, 0, 0, master_width, screen_height);

    for (int i = 1; i < num_clients; i++) {
        XMoveResizeWindow(display, clients[i]->win, master_width, (i - 1) * stack_height,
            master_width, stack_height);
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
    } else if (keysym == XK_q && (e->state & Mod4Mask)) {
        spawn("kitty");
    } else if (keysym == XK_Return && (e->state & Mod4Mask)) {
        spawn("firefox");
    } else if(keysym == XK_Tab && (e->state & Mod4Mask) && !(e->state & ShiftMask)) {
        if (num_clients > 1) {
            focused = (focused + 1) % num_clients;
            XSetInputFocus(display, clients[focused]->win, RevertToPointerRoot, CurrentTime);
            XRaiseWindow(display, clients[focused]->win);
        }
    } else if(keysym == XK_Tab && (e->state & Mod4Mask) && (e->state & ShiftMask)) {
        if (num_clients > 1) {
            focused = (focused - 1 < 0) ? num_clients - 1: focused - 1;
            XSetInputFocus(display, clients[focused]->win, RevertToPointerRoot, CurrentTime);
            XRaiseWindow(display, clients[focused]->win);
        }
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
    XGrabKey(display, XKeysymToKeycode(display, XK_q), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_Tab), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_Tab), Mod4Mask | ShiftMask, root, True, GrabModeAsync, GrabModeAsync);
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
