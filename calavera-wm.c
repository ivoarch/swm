/* See LICENSE file for copyright and license details. */

/* headers */
#include <limits.h>
#include <errno.h>
#include <locale.h>
#include <stdarg.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>

/* for multimedia keys, etc. */
#include <X11/XF86keysym.h>

/* windows manager name */
#define WMNAME "calavera-wm"

#define BUFSIZE 256

/* macros */
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define ISVISIBLE(C)            ((C->tags & themon->tagset[themon->seltags]))
#define LENGTH(X)               (sizeof X / sizeof X[0])
#define MAX(A, B)               ((A) > (B) ? (A) : (B))
#define MIN(A, B)               ((A) < (B) ? (A) : (B))
#define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
#define WIDTH(X)                ((X)->w + 2 * (X)->bw)
#define HEIGHT(X)               ((X)->h + 2 * (X)->bw)
#define N_WORKSPACES 10
#define TAGMASK                 ((1 << N_WORKSPACES) - 1)
#define RESIZE_MASK             (CWX|CWY|CWWidth|CWHeight|CWBorderWidth)
#define EVENT_MASK              (EnterWindowMask | FocusChangeMask | PropertyChangeMask | StructureNotifyMask)
#define ROOT                    RootWindow(display, DefaultScreen(display))

/* enums */
enum { PrefixKey, CmdKey };                              /* prefix key */
enum { CurNormal, CurResize, CurMove, CurCmd, CurLast }; /* cursor */

/* EWMH atoms */
enum {
    NetActiveWindow,
    NetClientList,
    NetClientListStacking,
    NetCurrentDesktop,
    NetNumberOfDesktops,
    NetSupported,
    NetSupportingCheck,
    NetWMDesktop,
    NetWMName,
    NetWMState,
    NetWMFullscreen,
    NetWMWindowType,
    NetWMWindowTypeNotification,
    NetWMWindowTypeSplash,
    NetWMWindowTypeDock,
    NetWMWindowTypeDialog,
    NetLast
};

/* default atoms */
enum {
    WMProtocols,
    WMDelete,
    WMState,
    WMTakeFocus,
    WMLast,
    Utf8String,
};

typedef union {
    int i;
    unsigned int ui;
    float f;
    const void *v;
} Arg; /* argument structure by conf.h */

typedef struct {
    unsigned int mask;
    unsigned int button;
    void (*func)(const Arg *arg);
    const Arg arg;
} Button;

typedef struct Monitor Monitor;
typedef struct Client Client;
struct Client {
    char name[BUFSIZE];
    float mina, maxa;
    int x, y, w, h;  /* current position and size */
    int oldx, oldy, oldw, oldh;
    int basew, baseh, incw, inch, maxw, maxh, minw, minh;
    int bw, oldbw;
    unsigned int tags;
    Bool isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen, needresize; // isdock;
    Client *next;
    Client *snext;
    Window win; /* The window */
};

/* key struct */
typedef struct {
    unsigned int mod;
    KeySym keysym;
    void (*func)(const Arg *);
    const Arg arg;
} Key;

struct Monitor {
    int num;
    int mx, my, mw, mh;   /* screen size */
    int wx, wy, ww, wh;   /* window area  */
    unsigned int seltags;
    unsigned int tagset[2];
    Client *clients;
    Client *thesel;
    Client *thestack;
};

typedef struct {
    const char *class;
    unsigned int tags;
    Bool isfloating;
} Rule;

/* DATA */

// atoms - ewmh
static void ewmh_init(void);
static long ewmh_getstate(Window w);
static void ewmh_setclientstate(Client *c, long state);
static Bool sendevent(Client *c, Atom proto);
static void ewmh_setnumbdesktops(void);
static void ewmh_updatecurrenddesktop(void);
static void ewmh_updateclientdesktop(Client *c);
static void ewmh_updateclientlist(void);
static void ewmh_updateclientlist_stacking(void);
static void ewmh_updatewindowtype(Client *c);

// bar
static void set_padding(void);

// colors
static unsigned long getcolor(const char *colstr);

// clients
static Bool applysizehints(Client *c, int *x, int *y, int *w, int *h, Bool interact);
static void attach(Client *c);
static void attachstack(Client *c);
static void attachend(Client *c);
static void attachstackend(Client *c);
static void clearurgent(Client *c);
static void configure(Client *c);
static void detach(Client *c);
static void detachstack(Client *c);
static void focus(Client *c);
static void killclient(Client *c);
static void grabbuttons(Client *c, Bool focused);
static void pop(Client *);
static void resize(Client *c, int x, int y, int w, int h, Bool interact);
static void resizeclient(Client *c, int x, int y, int w, int h);
static void setfocus(Client *c);
static void setfullscreen(Client *c, Bool fullscreen);
static void showhide(Client *c);
static void unfocus(Client *c, Bool setfocus);
static void unmanage(Client *c, Bool destroyed);
static void updatesizehints(Client *c);
static void updatewmhints(Client *c);
static Client *wintoclient(Window w);

// events
static void buttonpress(XEvent *e);
static void clientmessage(XEvent *e);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static void destroynotify(XEvent *e);
static void focusin(XEvent *e);
static void keypress(XEvent *e);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void propertynotify(XEvent *e);
static void unmapnotify(XEvent *e);

// manage
static void grabkeys(int keytype);
static void manage(Window w, XWindowAttributes *wa);
static void grab_pointer(void);
static void updatenumlockmask(void);

// main
static void autorun(void);
static void checkotherwm(void);
static Bool checkdock(Window *w);
static void cleanup(void);
static void eprint(const char *errstr, ...);
static Bool getrootptr(int *x, int *y);
static void handle_events(void);
static void scan(void);
static void setup(void);
static void sigchld(int unused);
static void sync_display(void);
static int xerror(Display *display, XErrorEvent *ee);
static int xerrordummy(Display *display, XErrorEvent *ee);
static int xerrorstart(Display *display, XErrorEvent *ee);

// monitor
static void arrange_windows(void);
static Monitor *createmon(void);
static void restack(void);
static Bool updategeom(void);

// actions
static void banish(const Arg *arg);
static void center(const Arg *arg);
static void focusstack(const Arg *arg);
static void killfocused(const Arg *arg);
static void exec(const Arg *arg);
static void maximize(const Arg *arg);
static void horizontalmax(const Arg *arg);
static void verticalmax(const Arg *arg);
static void movemouse(const Arg *arg);
static void moveto_workspace(const Arg *arg);
static void moveresize(const Arg *arg);
static void quit(const Arg *arg);
static void reload(const Arg *arg);
static void resizemouse(const Arg *arg);
static void spawn(const Arg *arg);
static void fullscreen(const Arg *arg);
static void change_workspace(const Arg *arg);

/* variables */
static unsigned int win_focus;                                                                                                
static unsigned int win_unfocus;
static char *wm_name = WMNAME;
static char **cargv;
static int screen, screen_w, screen_h;  /* X display screen geometry width, height */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0; /* dynamic key lock mask */
/* Events array */
static void (*handler[LASTEvent]) (XEvent *) = {
    [ButtonPress] = buttonpress,
    [ClientMessage] = clientmessage,
    [ConfigureRequest] = configurerequest,
    [ConfigureNotify] = configurenotify,
    [DestroyNotify] = destroynotify,
    [FocusIn] = focusin,
    [KeyPress] = keypress,
    [MappingNotify] = mappingnotify,
    [MapRequest] = maprequest,
    [PropertyNotify] = propertynotify,
    [UnmapNotify] = unmapnotify
};
static Atom wmatom[WMLast], netatom[NetLast];
static Bool running = True;
static Cursor cursor[CurLast];
static Display *display; /* The connection to the X server. */
static Monitor *themon = NULL;
static Window root;

/* configuration, allows nested code to access above variables */
#include "conf.h"

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags { char limitexceeded[N_WORKSPACES > 31 ? -1 : 1]; };

/* function implementations */
Bool applysizehints(Client *c, int *x, int *y, int *w, int *h, Bool interact) {
    Bool baseismin;

    /* set minimum possible */
    *w = MAX(1, *w);
    *h = MAX(1, *h);
    if(interact) {
	if(*x > screen_w)
	    *x = screen_w - WIDTH(c);
	if(*y > screen_h)
	    *y = screen_h - HEIGHT(c);
	if(*x + *w + 2 * c->bw < 0)
	    *x = 0;
	if(*y + *h + 2 * c->bw < 0)
	    *y = 0;
    }
    else {
	if(*x >= themon->wx + themon->ww)
	    *x = themon->wx + themon->ww - WIDTH(c);
	if(*y >= themon->wy + themon->wh)
	    *y = themon->wy + themon->wh - HEIGHT(c);
	if(*x + *w + 2 * c->bw <= themon->wx)
	    *x = themon->wx;
	if(*y + *h + 2 * c->bw <= themon->wy)
	    *y = themon->wy;
    }
    if(*h < DOCK_SIZE)
      *h = DOCK_SIZE;
    if(*w < DOCK_SIZE)
      *w = DOCK_SIZE;
    if(c->isfloating) {
	/* see last two sentences in ICCCM 4.1.2.3 */
	baseismin = c->basew == c->minw && c->baseh == c->minh;
	if(!baseismin) { /* temporarily remove base dimensions */
	    *w -= c->basew;
	    *h -= c->baseh;
	}
	/* adjust for aspect limits */
	if(c->mina > 0 && c->maxa > 0) {
	    if(c->maxa < (float)*w / *h)
		*w = *h * c->maxa + 0.5;
	    else if(c->mina < (float)*h / *w)
		*h = *w * c->mina + 0.5;
	}
	if(baseismin) { /* increment calculation requires this */
	    *w -= c->basew;
	    *h -= c->baseh;
	}
	/* adjust for increment value */
	if(c->incw)
	    *w -= *w % c->incw;
	if(c->inch)
	    *h -= *h % c->inch;
	/* restore base dimensions */
	*w = MAX(*w + c->basew, c->minw);
	*h = MAX(*h + c->baseh, c->minh);
	if(c->maxw)
	    *w = MIN(*w, c->maxw);
	if(c->maxh)
	    *h = MIN(*h, c->maxh);
    }
    return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

void arrange_windows() {
    if(themon)
      showhide(themon->thestack);
    restack();
}

void attachend(Client *c) {
    Client *p = themon->clients;

    if(p) {
	for(; p->next; p = p->next);
        p->next = c;
    }  else {
	attach(c);
    }
}

void attach(Client *c) {
    c->next = themon->clients;
    themon->clients = c;
}

void attachstackend(Client *c) {
    Client *p = themon->thestack;

    if(p) {
	for(; p->snext; p = p->snext);
	p->snext = c;
    } else {
	attachstack(c);
    }
}

void attachstack(Client *c) {
    c->snext = themon->thestack;
    themon->thestack = c;
}

void autorun(){
    struct stat st;
    char path[PATH_MAX];
    char *home;

    /* execute autostart script */
    if (!(home = getenv("HOME")))
      return;

    snprintf(path, sizeof(path), "%s/calavera-wm/autostart", home);

    if (stat(path, &st) != 0)
      return;

    const char* autostartcmd[] = { path, NULL };
    Arg a = {.v = autostartcmd };

    /* Check if file is executable */
    if (S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR))
    spawn(&a);
}

void buttonpress(XEvent *e) {
    unsigned int i, click = 0;
    Client *c;
    XButtonPressedEvent *ev = &e->xbutton;

    if((c = wintoclient(ev->window))) {
	focus(c);
	click = 1;
    }
    for(i = 0; i < LENGTH(buttons); i++)
      if(click && buttons[i].func && buttons[i].button == ev->button
         && CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
        buttons[i].func(&buttons[i].arg);
}

void banish(const Arg *arg) {
  XWarpPointer(display, None, root, 0, 0, 0, 0, screen_w, screen_h);
}

void center(const Arg *arg) {
    if(!themon->thesel || themon->thesel->isfullscreen || !(themon->thesel->isfloating))
	return;
    resize(themon->thesel, themon->wx + 0.5 * (themon->ww - themon->thesel->w), themon->wy + 0.5 *  
           (themon->wh - themon->thesel->h), themon->thesel->w, themon->thesel->h, False);
    arrange_windows();
}

/* FIXME */
Bool checkdock(Window *w) {
    int format;
    unsigned char *p = NULL;
    unsigned long n, extra;
    Atom real, result = None;
  
    if(XGetWindowProperty(display, *w, netatom[NetWMWindowType], 0L, 0xffffffff, False, AnyPropertyType,
                          &real, &format, &n, &extra, &p) == Success) {
      if (n != 0)
        result = * (Atom *) p;
    }
    XFree(p);
    XMapWindow(display, *w);
    return result == netatom[NetWMWindowTypeDock]  
      || result == netatom[NetWMWindowTypeNotification]  
      || result == netatom[NetWMWindowTypeSplash]? True : False;
}

void checkotherwm(void) {
    xerrorxlib = XSetErrorHandler(xerrorstart);
    /* this causes an error if some other window manager is running */
    XSelectInput(display, DefaultRootWindow(display), SubstructureRedirectMask);
    sync_display();
    XSetErrorHandler(xerror);
    sync_display();
}

void cleanup(void) {
    Arg a = {.ui = ~0};

    moveto_workspace(&a);
    while(themon->thestack)
      unmanage(themon->thestack, False);
    XUngrabKey(display, AnyKey, AnyModifier, root);
    XFreeCursor(display, cursor[CurNormal]);
    XFreeCursor(display, cursor[CurResize]);
    XFreeCursor(display, cursor[CurMove]);
    XFreeCursor(display, cursor[CurCmd]); 
    sync_display();
    XSetInputFocus(display, PointerRoot, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(display, root, netatom[NetActiveWindow]); 
    free(themon);
}

void clearurgent(Client *c) {
    XWMHints *wmh;

    c->isurgent = False;
    if(!(wmh = XGetWMHints(display, c->win)))
	return;
    wmh->flags &= ~XUrgencyHint;
    XSetWMHints(display, c->win, wmh);
    XFree(wmh);
}

void clientmessage(XEvent *e) {
    XClientMessageEvent *cme = &e->xclient;
    Client *c = wintoclient(cme->window);

    if(!c)
	return;
    if(cme->message_type == netatom[NetWMState]) {
	if(cme->data.l[1] == netatom[NetWMFullscreen] || cme->data.l[2] == netatom[NetWMFullscreen])
	    setfullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
			      || (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ && !c->isfullscreen)));
    }
    else if(cme->message_type == netatom[NetActiveWindow]) {
	if(!ISVISIBLE(c)) {
	    themon->seltags ^= 1;
	    themon->tagset[themon->seltags] = c->tags;
	}
	pop(c);
    }
}

void configure(Client *c) {
    XConfigureEvent ce;

    ce.type = ConfigureNotify;
    ce.display = display;
    ce.event = c->win;
    ce.window = c->win;
    ce.x = c->x;
    ce.y = c->y;
    ce.width = c->w;
    ce.height = c->h;
    ce.border_width = c->bw;
    ce.above = None;
    ce.override_redirect = False;
    XSendEvent(display, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void configurenotify(XEvent *e) {
    XConfigureEvent *ev = &e->xconfigure;
    Bool dirty;

    // TODO: updategeom handling sucks, needs to be simplified
    if(ev->window == root) {
	dirty = (screen_w != ev->width || screen_h != ev->height);
	screen_w = ev->width;
	screen_h = ev->height;
	if(updategeom() || dirty) {
	    focus(NULL);
	    arrange_windows();
	}
    }
}

void configurerequest(XEvent *e) {
    Client *c;
    XConfigureRequestEvent *ev = &e->xconfigurerequest;
    XWindowChanges wc;

    if((c = wintoclient(ev->window))) {
	if(ev->value_mask & CWBorderWidth)
	    c->bw = ev->border_width;
	else if(c->isfloating) {
	    if(ev->value_mask & CWX) {
		c->oldx = c->x;
		c->x = themon->mx + ev->x;
	    }
	    if(ev->value_mask & CWY) {
		c->oldy = c->y;
		c->y = themon->my + ev->y;
	    }
	    if(ev->value_mask & CWWidth) {
		c->oldw = c->w;
		c->w = ev->width;
	    }
	    if(ev->value_mask & CWHeight) {
		c->oldh = c->h;
		c->h = ev->height;
	    }
	    if((c->x + c->w) > themon->mx + themon->mw && c->isfloating)
		c->x = themon->mx + (themon->mw / 2 - WIDTH(c) / 2); /* center in x direction */
	    if((c->y + c->h) > themon->my + themon->mh && c->isfloating)
		c->y = themon->my + (themon->mh / 2 - HEIGHT(c) / 2); /* center in y direction */
	    if((ev->value_mask & (CWX|CWY)) && !(ev->value_mask & (CWWidth|CWHeight)))
		configure(c);
	    if(ISVISIBLE(c))
		XMoveResizeWindow(display, c->win, c->x, c->y, c->w, c->h);
	    else
		c->needresize = True;
	}
	else
	    configure(c);
    }
    else {
	wc.x = ev->x;
	wc.y = ev->y;
	wc.width = ev->width;
	wc.height = ev->height;
	wc.border_width = ev->border_width;
	wc.sibling = ev->above;
	wc.stack_mode = ev->detail;
	XConfigureWindow(display, ev->window, ev->value_mask, &wc);
    }
    sync_display();
}

Monitor *createmon(void) {
    Monitor *m;

    if(!(m = (Monitor *)calloc(1, sizeof(Monitor))))
	eprint("fatal: could not malloc() %u bytes\n", sizeof(Monitor));

    m->tagset[0] = m->tagset[1] = 1;
    return m;
}

void destroynotify(XEvent *e) {
    Client *c;
    XDestroyWindowEvent *ev = &e->xdestroywindow;

    if((c = wintoclient(ev->window)))
	unmanage(c, True);
}

void detach(Client *c) {
    Client **tc;

    for(tc = &themon->clients; *tc && *tc != c; tc = &(*tc)->next);
    *tc = c->next;
}

void detachstack(Client *c) {
    Client **tc, *t;

    for(tc = &themon->thestack; *tc && *tc != c; tc = &(*tc)->snext);
    *tc = c->snext;

    if(c == themon->thesel) {
	for(t = themon->thestack; t && !ISVISIBLE(t); t = t->snext);
	themon->thesel = t;
    }
}

void eprint(const char *errstr, ...) {
    va_list ap;

    va_start(ap, errstr);
    vfprintf(stderr, errstr, ap);
    va_end(ap);
    exit(EXIT_FAILURE);
}

void ewmh_init(void) {
    XSetWindowAttributes wa;
    Window win;

    /* ICCCM */
    wmatom[WMProtocols] = XInternAtom(display, "WM_PROTOCOLS", False);
    wmatom[WMDelete] = XInternAtom(display, "WM_DELETE_WINDOW", False);
    wmatom[WMState] = XInternAtom(display, "WM_STATE", False);
    wmatom[WMTakeFocus] = XInternAtom(display, "WM_TAKE_FOCUS", False);
 
    /* EWMH */
    netatom[NetActiveWindow] = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
    netatom[NetSupported] = XInternAtom(display, "_NET_SUPPORTED", False);
    netatom[NetSupportingCheck] = XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", False);
    netatom[NetClientList] = XInternAtom(display, "_NET_CLIENT_LIST", False);
    netatom[NetClientListStacking] = XInternAtom(display, "_NET_CLIENT_LIST_STACKING", False);
    netatom[NetNumberOfDesktops] = XInternAtom(display, "_NET_NUMBER_OF_DESKTOPS", False);
    netatom[NetCurrentDesktop] = XInternAtom(display, "_NET_CURRENT_DESKTOP", False);

    /* STATES */
    netatom[NetWMState] = XInternAtom(display, "_NET_WM_STATE", False);
    netatom[NetWMFullscreen] = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);

    /* TYPES */
    netatom[NetWMWindowType] = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    netatom[NetWMWindowTypeNotification] = XInternAtom(display, "_NET_WM_WINDOW_TYPE_NOTIFICATION", False);
    netatom[NetWMWindowTypeDock] = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DOCK", False);
    netatom[NetWMWindowTypeDialog] = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    netatom[NetWMWindowTypeSplash] = XInternAtom(display, "_NET_WM_WINDOW_TYPE_SPLASH", False);
 
    /* CLIENTS */
    netatom[NetWMName] = XInternAtom(display, "_NET_WM_NAME", False);
    netatom[NetWMDesktop] = XInternAtom(display, "_NET_WM_DESKTOP", False);

    /* OTHER */
    netatom[Utf8String] = XInternAtom(display, "UTF8_STRING", False);

    /* Tell which ewmh atoms are supported */
    XChangeProperty(display, root, netatom[NetSupported], XA_ATOM, 32,
		    PropModeReplace, (unsigned char *) netatom, NetLast);

    /* Create our own window! */
    wa.override_redirect = True;
    win = XCreateWindow(display, root, -100, 0, 1, 1,
			0, DefaultDepth(display, screen), CopyFromParent,
			DefaultVisual(display, screen), CWOverrideRedirect, &wa);

    XChangeProperty(display, root, netatom[NetSupportingCheck], XA_WINDOW, 32,
		    PropModeReplace, (unsigned char*)&win, 1);

    /* Set WM name */
    XChangeProperty(display, win, netatom[NetWMName], netatom[Utf8String], 8,
		    PropModeReplace, (unsigned char*)wm_name, strlen(wm_name));

}

void focus(Client *c) {
    if(!c || !ISVISIBLE(c))
	for(c = themon->thestack; c && !ISVISIBLE(c); c = c->snext);
    if(themon->thesel && themon->thesel != c)
	unfocus(themon->thesel, False);
    if(c) {
      if(c->isurgent)
        clearurgent(c);
        detachstack(c);
        attachstack(c);
	grabbuttons(c, True);
        XSetWindowBorder(display, c->win, win_focus);
	setfocus(c);
    }
    else {
      XSetInputFocus(display, root, RevertToPointerRoot, CurrentTime);
      XDeleteProperty(display, root, netatom[NetActiveWindow]);
    }
    themon->thesel = c;
}

void focusin(XEvent *e) { /* there are some broken focus acquiring clients */
    XFocusChangeEvent *ev = &e->xfocus;

    if(themon->thesel && ev->window != themon->thesel->win)
	setfocus(themon->thesel);
}

void focusstack(const Arg *arg) {
    Client *c = NULL, *i;

    if(!themon->thesel)
	return;
    if(arg->i > 0) { /* next */
      for(c = themon->thesel->next; c && !ISVISIBLE(c); c = c->next);
	if(!c)
          for(c = themon->clients; c && !ISVISIBLE(c); c = c->next);
    }
    else { /* prev */
	for(i = themon->clients; i != themon->thesel; i = i->next)
	    if(ISVISIBLE(i))
		c = i;
	if(!c)
	    for(; i; i = i->next)
		if(ISVISIBLE(i))
		    c = i;
    }
    if(c) {
	focus(c);
	restack();
    }
}

Atom getatomprop(Client *c, Atom prop) {
    int di;
    unsigned long dl;
    unsigned char *p = NULL;
    Atom da, atom = None;

    if(XGetWindowProperty(display, c->win, prop, 0L, sizeof atom, False, XA_ATOM,
			  &da, &di, &dl, &dl, &p) == Success && p) {
	atom = *(Atom *)p;
	XFree(p);
    }
    return atom;
}

unsigned long getcolor(const char *colstr) {
    Colormap cmap = DefaultColormap(display, screen);
    XColor color;

    if(!XAllocNamedColor(display, cmap, colstr, &color, &color))
	eprint("error, cannot allocate color '%s'\n", colstr);
    return color.pixel;
}

Bool getrootptr(int *x, int *y) {
    int di;
    unsigned int dui;
    Window dummy;

    return XQueryPointer(display, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

long ewmh_getstate(Window w) {
    int format;
    long result = -1;
    unsigned char *p = NULL;
    unsigned long n, extra;
    Atom real;

    if(XGetWindowProperty(display, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
			  &real, &format, &n, &extra, (unsigned char **)&p) != Success)
	return -1;
    if(n != 0)
	result = *p;
    XFree(p);
    return result;
}

void grab_pointer() {
    XGrabPointer (display, root, True, 0,
		  GrabModeAsync, GrabModeAsync,
		  None, cursor[CurCmd], CurrentTime);
}

void grabbuttons(Client *c, Bool focused) {
    updatenumlockmask();
    {
	unsigned int i, j;
	unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
	XUngrabButton(display, AnyButton, AnyModifier, c->win);
	if(focused) {
	    for(i = 0; i < LENGTH(buttons); i++)
              for(j = 0; j < LENGTH(modifiers); j++)
                XGrabButton(display, buttons[i].button,
                            buttons[i].mask | modifiers[j],
                            c->win, False, BUTTONMASK,
                            GrabModeAsync, GrabModeSync, None, None);
	}
	else
          XGrabButton(display, AnyButton, AnyModifier, c->win, False,
                      BUTTONMASK, GrabModeAsync, GrabModeSync, None, None);
    }
}

void grabkeys(int keytype) {
    unsigned int i;
    unsigned int modifiers[] = {
        0,
        LockMask,
        numlockmask,
	LockMask | numlockmask
    };
    KeyCode code;

    if(keytype == CmdKey) {
	XGrabKey(display, AnyKey, AnyModifier, root, True, GrabModeAsync,
		 GrabModeAsync);

        if (WAITKEY) {
	    grab_pointer();
	}
    }
    else {
	XUngrabKey(display, AnyKey, AnyModifier, root);

        if(HIDE_CURSOR) {
          XWarpPointer(display, None, root, 0, 0, 0, 0, screen_w, screen_h);
	}

	if((code = XKeysymToKeycode(display, PREFIX_KEYSYM)))
	    for(i = 0; i < LENGTH(modifiers); i++)
		XGrabKey(display, code, PREFIX_MODKEY | modifiers[i],
			 root, True, GrabModeAsync,
			 GrabModeAsync);

	XUngrabPointer(display, CurrentTime);
    }
}

void handle_events(void) {
  XEvent ev;

  /* main event loop */
  XSync(display, False);
  while(running && !XNextEvent(display, &ev))
    if(handler[ev.type])
      handler[ev.type](&ev); /* call handler */
}

void keypress(XEvent *e) {
    unsigned int i;
    KeySym keysym;
    XKeyEvent *ev;
    static int prefixset = 0;

    ev = &e->xkey;
    keysym = XkbKeycodeToKeysym(display, (KeyCode)e->xkey.keycode, 0, 0);

    if(!prefixset && keysym == PREFIX_KEYSYM
       && CLEANMASK(ev->state) == PREFIX_MODKEY) {
	prefixset = 1;
	grabkeys(CmdKey);
    }
    else {
	for(i = 0; i < LENGTH(keys); i++)
	    if(keysym == keys[i].keysym
	       && CLEANMASK(ev->state) == keys[i].mod && keys[i].func)
		keys[i].func(&(keys[i].arg));

	prefixset = 0;
	grabkeys(PrefixKey);
    }
}

void killclient(Client *c) {
    if(!themon->thesel)
	return;
    if(!sendevent(themon->thesel, wmatom[WMDelete])) {
	XGrabServer(display);
	XSetErrorHandler(xerrordummy);
	XSetCloseDownMode(display, DestroyAll);
	XKillClient(display, themon->thesel->win);
	sync_display();
	XSetErrorHandler(xerror);
	XUngrabServer(display);
    }
}

void killfocused(const Arg *arg) {
    if(!themon->thesel)
	return;
    killclient(themon->thesel);
}

/* manage the new client */
void manage(Window w, XWindowAttributes *wa) {
    Client *c, *t = NULL;
    Window trans = None;
    XWindowChanges wc;
    XClassHint ch = { NULL, NULL };

    if (checkdock(&w)) {
      return;
    }

    if(!(c = calloc(1, sizeof(Client))))
	eprint("fatal: could not malloc() %u bytes\n", sizeof(Client));
    c->win = w;

    if(XGetTransientForHint(display, w, &trans)) 
      t = wintoclient(trans);
    if(t) 
     c->tags = t->tags;
    else {
      themon = themon;  
    }
    /* rule matching */
    c->isfloating = 1, c->tags = 0;
    XGetClassHint(display, c->win, &ch);

    if(ch.res_class)
      XFree(ch.res_class);
    if(ch.res_name)
      XFree(ch.res_name);
    c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : themon->tagset[themon->seltags];
        
    /* geometry */
    c->x = c->oldx = wa->x;
    c->y = c->oldy = wa->y;
    c->w = c->oldw = wa->width;
    c->h = c->oldh = wa->height;
    c->oldbw = wa->border_width;

    //    c->isdock = False;

    if(c->x + WIDTH(c) > themon->mx + themon->mw)
	c->x = themon->mx + themon->mw - WIDTH(c);
    if(c->y + HEIGHT(c) > themon->my + themon->mh)
	c->y = themon->my + themon->mh - HEIGHT(c);
    c->x = MAX(c->x, themon->mx);
    /* only fix client y-offset, if the client center might cover the bar */
    c->y = MAX(c->y, ((c->x + (c->w / 2) >= themon->wx)
                      && (c->x + (c->w / 2) < themon->wx + themon->ww)) ? DOCK_SIZE : themon->my);
    c->bw = 1;

    wc.border_width = c->bw;
    XConfigureWindow(display, w, CWBorderWidth, &wc);
    XSetWindowBorder(display, w, win_focus);
    configure(c); /* propagates border_width, if size doesn't change */
    ewmh_updatewindowtype(c);
    updatesizehints(c);
    updatewmhints(c);
    XSelectInput(display, w, EVENT_MASK);
    grabbuttons(c, False);
    if(!c->isfloating)
	c->isfloating = c->oldstate = trans != None || c->isfixed;
    if(c->isfloating)
	XRaiseWindow(display, c->win);
    attachend(c);
    attachstackend(c);
    focus(c);
    XChangeProperty(display, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend,
		    (unsigned char *) &(c->win), 1);
    XChangeProperty(display, root, netatom[NetClientListStacking], XA_WINDOW, 32, PropModeAppend,
		    (unsigned char *) &(c->win), 1);
    XMoveResizeWindow(display, c->win, c->x + 2 * screen_w, c->y, c->w, c->h); /* some windows require this */
    ewmh_setclientstate(c, NormalState);
    themon->thesel = c;
    arrange_windows();
    XMapWindow(display, c->win); /* maps the window */
    focus(NULL);
    /* set clients tag as current desktop (_NET_WM_DESKTOP) */
    ewmh_updateclientdesktop(c);
} 

/* regrab when keyboard map changes */
void mappingnotify(XEvent *e) {
    XMappingEvent *ev = &e->xmapping;

    XRefreshKeyboardMapping(ev);
    if(ev->request == MappingKeyboard) {
	updatenumlockmask();
	grabkeys(PrefixKey);
    }
}

void maprequest(XEvent *e) {
    static XWindowAttributes wa;
    XMapRequestEvent *ev = &e->xmaprequest;

    if(!XGetWindowAttributes(display, ev->window, &wa))
	return;
    if(wa.override_redirect)
	return;
    if(!wintoclient(ev->window))
	manage(ev->window, &wa);
}

void maximize(const Arg *arg) {
    if(!themon->thesel || themon->thesel->isfullscreen || !(themon->thesel->isfloating))
	return;
    resize(themon->thesel, themon->wx, themon->wy, themon->ww - 2 * themon->thesel->bw,  
           themon->wh - 2 * themon->thesel->bw, False);
    arrange_windows();
}

void horizontalmax(const Arg *arg) {
    if(!themon->thesel || themon->thesel->isfullscreen || !(themon->thesel->isfloating))
	return;
    resize(themon->thesel, themon->wx, themon->thesel->y, themon->ww - 2 * themon->thesel->bw, themon->thesel->h, False);
    arrange_windows();
}

void verticalmax(const Arg *arg) {
    if(!themon->thesel || themon->thesel->isfullscreen || !(themon->thesel->isfloating))
	return;
    resize(themon->thesel, themon->thesel->x, themon->wy, themon->thesel->w, themon->wh - 2 * themon->thesel->bw, False);
    arrange_windows();
}

void movemouse(const Arg *arg) {
    int x, y, ocx, ocy, nx, ny;
    Client *c;
    XEvent ev;

    if(!(c = themon->thesel))
	return;
    if(c->isfullscreen) /* no support moving fullscreen windows by mouse */
	return;
    restack();
    ocx = c->x;
    ocy = c->y; 
    /* Warp pointer to center on move */
    XWarpPointer(display, None, c->win, 0, 0, 0, 0, c->w / 2, c->h / 2);
    if(XGrabPointer(display, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		    None, cursor[CurMove], CurrentTime) != GrabSuccess)
	return;
    if(!getrootptr(&x, &y))
	return;
    do {
	XMaskEvent(display, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
	switch(ev.type) {
	case ConfigureRequest:
	case Expose:
	case MapRequest:
	    handler[ev.type](&ev);
	    break;
	case MotionNotify:
	    nx = ocx + (ev.xmotion.x - x);
	    ny = ocy + (ev.xmotion.y - y);
	    if(nx >= themon->wx && nx <= themon->wx + themon->ww
	       && ny >= themon->wy && ny <= themon->wy + themon->wh) {
		if(abs(themon->wx - nx) < SNAP)
		    nx = themon->wx;
		else if(abs((themon->wx + themon->ww) - (nx + WIDTH(c))) < SNAP)
		    nx = themon->wx + themon->ww - WIDTH(c);
		if(abs(themon->wy - ny) < SNAP)
		    ny = themon->wy;
		else if(abs((themon->wy + themon->wh) - (ny + HEIGHT(c))) < SNAP)
		    ny = themon->wy + themon->wh - HEIGHT(c);
                if(!c->isfloating && (abs(nx - c->x) > SNAP || abs(ny - c->y) > SNAP));
		    }
            if(c->isfloating)
		resize(c, nx, ny, c->w, c->h, True);
	    break;
	}
    } while(ev.type != ButtonRelease);
    XUngrabPointer(display, CurrentTime);
}

void pop(Client *c) {
    detach(c);
    attach(c);
    focus(c);
    arrange_windows();
}

void propertynotify(XEvent *e) {
    Client *c;
    Window trans;
    XPropertyEvent *ev = &e->xproperty;

    if(ev->state == PropertyDelete)
	return; /* ignore */
    else if((c = wintoclient(ev->window))) {
	switch(ev->atom) {
	default: break;
	case XA_WM_TRANSIENT_FOR:
	    if(!c->isfloating && (XGetTransientForHint(display, c->win, &trans)) &&
	       (c->isfloating = (wintoclient(trans)) != NULL))
		arrange_windows();
	    break;
	case XA_WM_NORMAL_HINTS:
	    updatesizehints(c);
	    break;
	case XA_WM_HINTS:
	    updatewmhints(c);
	    break;
	}
	if(ev->atom == netatom[NetWMWindowType])
	    ewmh_updatewindowtype(c);
    }
}

void quit(const Arg *arg) {
    running = False;
}

void reload(const Arg *arg) {
    running = False;
    if (arg) {
	cleanup();
	execvp(cargv[0], cargv);
	eprint("Can't exec: %s\n", strerror(errno));
    }
}

void resize(Client *c, int x, int y, int w, int h, Bool interact) {
    if(applysizehints(c, &x, &y, &w, &h, interact))
	resizeclient(c, x, y, w, h);
}

void resizeclient(Client *c, int x, int y, int w, int h) {
    XWindowChanges wc;

    c->oldx = c->x; c->x = wc.x = x;
    c->oldy = c->y; c->y = wc.y = y;
    c->oldw = c->w; c->w = wc.width = w;
    c->oldh = c->h; c->h = wc.height = h;
    wc.border_width = c->bw;
    XConfigureWindow(display, c->win, RESIZE_MASK, &wc);
    configure(c);
    sync_display();
}

void resizemouse(const Arg *arg) {
    int ocx, ocy;
    int nw, nh;
    Client *c;
    XEvent ev;

    if(!(c = themon->thesel))
	return;
    if(c->isfullscreen) /* no support resizing fullscreen windows by mouse */
	return;
    restack();
    ocx = c->x;
    ocy = c->y;
    if(XGrabPointer(display, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		    None, cursor[CurResize], CurrentTime) != GrabSuccess)
	return;
    XWarpPointer(display, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
    do {
	XMaskEvent(display, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
	switch(ev.type) {
	case ConfigureRequest:
	case Expose:
	case MapRequest:
	    handler[ev.type](&ev);
	    break;
	case MotionNotify:
	    nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
	    nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
	    if(themon->wx + nw >= themon->wx && themon->wx + nw <= themon->wx + themon->ww
	       && themon->wy + nh >= themon->wy && themon->wy + nh <= themon->wy + themon->wh)
		{
		    if(!c->isfloating && (abs(nw - c->w) > SNAP || abs(nh - c->h) > SNAP));
		}
	    if(c->isfloating)
		resize(c, c->x, c->y, nw, nh, True);
	    break;
	}
    } while(ev.type != ButtonRelease);
    XWarpPointer(display, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
    XUngrabPointer(display, CurrentTime);
    while(XCheckMaskEvent(display, EnterWindowMask, &ev));
}

/* restores all clients */
void restack() {
    XEvent ev;

    if(!themon->thesel)
	return;
    XRaiseWindow(display, themon->thesel->win);
    sync_display();
    while(XCheckMaskEvent(display, EnterWindowMask, &ev));
}

void scan(void) {
    unsigned int i, num;
    Window d1, d2, *wins = NULL;
    XWindowAttributes wa;

    if(XQueryTree(display, root, &d1, &d2, &wins, &num)) {
	for(i = 0; i < num; i++) {
	    if(!XGetWindowAttributes(display, wins[i], &wa)
	       || wa.override_redirect || XGetTransientForHint(display, wins[i], &d1))
		continue;
	    if(wa.map_state == IsViewable || ewmh_getstate(wins[i]) == IconicState)
		manage(wins[i], &wa);
	}
	for(i = 0; i < num; i++) { /* now the transients */
	    if(!XGetWindowAttributes(display, wins[i], &wa))
		continue;
	    if(XGetTransientForHint(display, wins[i], &d1)
	       && (wa.map_state == IsViewable || ewmh_getstate(wins[i]) == IconicState))
		manage(wins[i], &wa);
	}
	if(wins)
	    XFree(wins);
    }
}

void ewmh_setclientstate(Client *c, long state) {
    long data[] = { state, None };

    XChangeProperty(display, c->win, wmatom[WMState], wmatom[WMState], 32,
		    PropModeReplace, (unsigned char *)data, 2);
}

static Bool sendevent(Client *c, Atom proto){
    int n;
    Atom *protocols;
    Bool exists = False;
    XEvent ev;

    if(XGetWMProtocols(display, c->win, &protocols, &n)) {
      while(!exists && n--)
        exists = protocols[n] == proto;
      XFree(protocols);

    }
    if(exists) {
    ev.type = ClientMessage;
    ev.xclient.window = c->win;
    ev.xclient.message_type = wmatom[WMProtocols];
	ev.xclient.format = 32;
    ev.xclient.data.l[0] = proto;
	ev.xclient.data.l[1] = CurrentTime;
    XSendEvent(display, c->win, False, NoEventMask, &ev);
    }
    return exists;
}

void setfocus(Client *c) {
    if(!c->neverfocus) {
	XSetInputFocus(display, c->win, RevertToPointerRoot, CurrentTime);
	XChangeProperty(display, root, netatom[NetActiveWindow],
			XA_WINDOW, 32, PropModeReplace,
			(unsigned char *) &(c->win), 1);
    }
    sendevent(c, wmatom[WMTakeFocus]);
}

void setfullscreen(Client *c, Bool fullscreen) {
    if(fullscreen) {
	XChangeProperty(display, c->win, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
	c->isfullscreen = True;
	c->oldstate = c->isfloating;
	c->oldbw = c->bw;
	c->bw = 0;
	c->isfloating = True;
	resizeclient(c, themon->mx, themon->my, themon->mw, themon->mh);
	XRaiseWindow(display, c->win);
    }
    else {
	XChangeProperty(display, c->win, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char*)0, 0);
	c->isfullscreen = False;
	c->isfloating = c->oldstate;
	c->bw = c->oldbw;
	c->x = c->oldx;
	c->y = c->oldy;
	c->w = c->oldw;
	c->h = c->oldh;
	resizeclient(c, c->x, c->y, c->w, c->h);
	arrange_windows();
    }
}

void ewmh_setcurrentdesktop(void) {
    long data[] = { 0 };

    XChangeProperty(display, root, netatom[NetCurrentDesktop], XA_CARDINAL, 32,
		    PropModeReplace, (unsigned char *)data, 1);
}

void ewmh_setnumbdesktops(void) {
    long data[] = { TAGMASK };

    XChangeProperty(display, root, netatom[NetNumberOfDesktops], XA_CARDINAL, 32,
		    PropModeReplace, (unsigned char *)data, 1);
}

void setup(void) {
    XSetWindowAttributes wa;

    /* clean up any zombies immediately */
    sigchld(0);

    /* init screen */
    screen = DefaultScreen(display);
    root = RootWindow(display, screen);
    screen_w = DisplayWidth(display, screen);
    screen_h = DisplayHeight(display, screen);
    updategeom();

    /* Standard & EWMH atoms */
    ewmh_init();

    /* cursors */
    cursor[CurNormal] = XCreateFontCursor(display, XC_top_left_arrow);
    cursor[CurResize] = XCreateFontCursor(display, XC_bottom_right_corner);
    cursor[CurMove] = XCreateFontCursor(display, XC_fleur);
    cursor[CurCmd] = XCreateFontCursor(display, CURSOR_WAITKEY);
    /* border colors */
    win_unfocus = getcolor(UNFOCUS);
    win_focus = getcolor(FOCUS);

    XDeleteProperty(display, root, netatom[NetClientList]);
    XDeleteProperty(display, root, netatom[NetClientListStacking]);
    /* set EWMH NUMBER_OF_DESKTOPS */
    ewmh_setnumbdesktops();
    /* initialize EWMH CURRENT_DESKTOP */
    ewmh_setcurrentdesktop();
    ewmh_updatecurrenddesktop();
    /* select for events */
    wa.cursor = cursor[CurNormal];
    wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask|ButtonPressMask|PointerMotionMask
	|EnterWindowMask|LeaveWindowMask|StructureNotifyMask|PropertyChangeMask;
    XChangeWindowAttributes(display, root, CWEventMask|CWCursor, &wa);
    XSelectInput(display, root, wa.event_mask);
    updatenumlockmask();
    grabkeys(PrefixKey);
}

void showhide(Client *c) {
    if(!c)
	return;
    if(ISVISIBLE(c)) { /* show clients top down */
	if(c->needresize) {
	    c->needresize = False;
	    XMoveResizeWindow(display, c->win, c->x, c->y, c->w, c->h);
	} else {
	    XMoveWindow(display, c->win, c->x, c->y);
	}
	if(c->isfloating && !c->isfullscreen)
	    resize(c, c->x, c->y, c->w, c->h, False);
	showhide(c->snext);
    }
    else { /* hide clients bottom up */
	showhide(c->snext);
	XMoveWindow(display, c->win, WIDTH(c) * -2, c->y);
    }
}

void sigchld(int unused) {
    if(signal(SIGCHLD, sigchld) == SIG_ERR)
	eprint("Can't install SIGCHLD handler");
    while(0 < waitpid(-1, NULL, WNOHANG));
}

void spawn(const Arg *arg) {
    if(fork() == 0) {
	if(display)
	    close(ConnectionNumber(display));
	setsid();
	execvp(((char **)arg->v)[0], (char **)arg->v);
	fprintf(stderr, "calavera-wm: execvp %s", ((char **)arg->v)[0]);
	perror(" failed");
	exit(EXIT_SUCCESS);
    }
}

void sync_display(void) {
    XSync(display, False);
}

void moveto_workspace(const Arg *arg) {
    if(themon->thesel && arg->ui & TAGMASK) {
	themon->thesel->tags = arg->ui & TAGMASK;
        ewmh_updateclientdesktop(themon->thesel);
	focus(NULL);
	arrange_windows();
    }
    ewmh_updatecurrenddesktop();
}

void moveresize(const Arg *arg) {
  XEvent ev;

  if(!(themon->thesel && arg && arg->v && themon->thesel->isfloating)) 
    return;
  resize(themon->thesel, themon->thesel->x + ((int *)arg->v)[0], themon->thesel->y + ((int *)arg->v)[1],  
         themon->thesel->w + ((int *)arg->v)[2], themon->thesel->h + ((int *)arg->v)[3], True);

  while(XCheckMaskEvent(display, EnterWindowMask, &ev));
}

void fullscreen(const Arg *arg) {

    if(!themon->thesel)
	return;
    setfullscreen(themon->thesel, !themon->thesel->isfullscreen);
}

/* mark window win as unfocused. */
void unfocus(Client *c, Bool setfocus) {
    if(!c)
	return;
    grabbuttons(c, False);
    /* set new border unfocus colour. */ 
    XSetWindowBorder(display, c->win, win_unfocus);
    if(setfocus) {
	XSetInputFocus(display, root, RevertToPointerRoot, CurrentTime);
	XDeleteProperty(display, root, netatom[NetActiveWindow]);
    }
}

/* destroy the client */
void unmanage(Client *c, Bool destroyed) {
    XWindowChanges wc;

    /* The server grab construct avoids race conditions. */
    detach(c);
    detachstack(c);
    if(!destroyed) {
	wc.border_width = c->oldbw;
	XGrabServer(display);
	XSetErrorHandler(xerrordummy);
	XConfigureWindow(display, c->win, CWBorderWidth, &wc); /* restore border */
	XUngrabButton(display, AnyButton, AnyModifier, c->win);
	ewmh_setclientstate(c, WithdrawnState);
	sync_display();
	XSetErrorHandler(xerror);
	XUngrabServer(display);
    }
    free(c);
    focus(NULL);
    ewmh_updateclientlist();
    ewmh_updateclientlist_stacking();
    arrange_windows();
}


void unmapnotify(XEvent *e) {
    Client *c;
    XUnmapEvent *ev = &e->xunmap;

    if((c = wintoclient(ev->window))) {
	if(ev->send_event)
	    ewmh_setclientstate(c, WithdrawnState);
	else
	    unmanage(c, False);
    }
}

void set_padding() {
    themon->wy += DOCK_SIZE;
    themon->wh -= DOCK_SIZE;
}

void ewmh_updateclientlist() {
    Client *c;

    XDeleteProperty(display, root, netatom[NetClientList]);
	for(c = themon->clients; c; c = c->next)
	    XChangeProperty(display, root, netatom[NetClientList],
			    XA_WINDOW, 32, PropModeAppend,
			    (unsigned char *) &(c->win), 1);
}

void ewmh_updateclientlist_stacking() {
    Client *c;

    XDeleteProperty(display, root, netatom[NetClientListStacking]);
        for(c = themon->thestack; c; c = c->snext)
            XChangeProperty(display, root, netatom[NetClientListStacking],
                            XA_WINDOW, 32, PropModeAppend,
                            (unsigned char *) &(c->win), 1);
}

void ewmh_updateclientdesktop(Client *c) {
     long data[] = { c->tags };

     XChangeProperty(display, c->win, netatom[NetWMDesktop], XA_CARDINAL, 32,
		     PropModeReplace, (unsigned char *)data, 1);
}

void ewmh_updatecurrenddesktop() {
    long data[] = { themon->tagset[themon->seltags] };

    XChangeProperty(display, root, netatom[NetCurrentDesktop], XA_CARDINAL, 32,
		    PropModeReplace, (unsigned char *)data, 1);
}

Bool updategeom(void) {
    Bool dirty = False;

    if(!themon)
      themon = createmon();
    if(themon->mw != screen_w || themon->mh != screen_h) {
      dirty = True;
      themon->mw = themon->ww = screen_w;
      themon->mh = themon->wh = screen_h;
      set_padding();
    }
    return dirty;
}

void updatenumlockmask(void) {
    unsigned int i, j;
    XModifierKeymap *modmap;

    numlockmask = 0;
    modmap = XGetModifierMapping(display);
    for(i = 0; i < 8; i++)
	for(j = 0; j < modmap->max_keypermod; j++)
	    if(modmap->modifiermap[i * modmap->max_keypermod + j]
	       == XKeysymToKeycode(display, XK_Num_Lock))
		numlockmask = (1 << i);
    XFreeModifiermap(modmap);
}

void updatesizehints(Client *c) {
    long msize;
    XSizeHints size;

    if(!XGetWMNormalHints(display, c->win, &size, &msize))
	/* size is uninitialized, ensure that size.flags aren't used */
	size.flags = PSize;
    if(size.flags & PBaseSize) {
	c->basew = size.base_width;
	c->baseh = size.base_height;
    }
    else if(size.flags & PMinSize) {
	c->basew = size.min_width;
	c->baseh = size.min_height;
    }
    else
	c->basew = c->baseh = 0;
    if(size.flags & PResizeInc) {
	c->incw = size.width_inc;
	c->inch = size.height_inc;
    }
    else
	c->incw = c->inch = 0;
    if(size.flags & PMaxSize) {
	c->maxw = size.max_width;
	c->maxh = size.max_height;
    }
    else
	c->maxw = c->maxh = 0;
    if(size.flags & PMinSize) {
	c->minw = size.min_width;
	c->minh = size.min_height;
    }
    else if(size.flags & PBaseSize) {
	c->minw = size.base_width;
	c->minh = size.base_height;
    }
    else
	c->minw = c->minh = 0;
    if(size.flags & PAspect) {
	c->mina = (float)size.min_aspect.y / size.min_aspect.x;
	c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
    }
    else
	c->maxa = c->mina = 0.0;
    c->isfixed = (c->maxw && c->minw && c->maxh && c->minh
		  && c->maxw == c->minw && c->maxh == c->minh);
}

void ewmh_updatewindowtype(Client *c) {
    Atom state = getatomprop(c, netatom[NetWMState]);
    Atom wtype = getatomprop(c, netatom[NetWMWindowType]);

    if(state == netatom[NetWMFullscreen])
	setfullscreen(c, True);

    if(wtype == netatom[NetWMWindowTypeDialog]) {
	c->isfloating = True; 
    } else if(wtype == netatom[NetWMWindowTypeDock] 
              || wtype == netatom[NetWMWindowTypeNotification]
              || wtype == netatom[NetWMWindowTypeSplash]) {
      //      c->isdock = True;
      c->neverfocus = True; 
      c->isfloating = True; 
    }
}

void updatewmhints(Client *c) {
    XWMHints *wmh;

    if((wmh = XGetWMHints(display, c->win))) {
	if(c == themon->thesel && wmh->flags & XUrgencyHint) {
	    wmh->flags &= ~XUrgencyHint;
	    XSetWMHints(display, c->win, wmh);
	}
	else
	    c->isurgent = (wmh->flags & XUrgencyHint) ? True : False;
	if(wmh->flags & InputHint)
	    c->neverfocus = !wmh->input;
	else
	    c->neverfocus = False;
	XFree(wmh);
    }
}

void change_workspace(const Arg *arg) {

    if((arg->ui & TAGMASK) == themon->tagset[themon->seltags])
	return;
    themon->seltags ^= 1; /* toggle sel tagset */
    if(arg->ui & TAGMASK)
	themon->tagset[themon->seltags] = arg->ui & TAGMASK;
    focus(NULL);
    arrange_windows();
    ewmh_updatecurrenddesktop();
}

Client *wintoclient(Window w) {
    Client *c;

    for(c = themon->clients; c; c = c->next)
      if(c->win == w)
        return c;
    return NULL;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's).  Other types of errors call Xlibs
 * default error handler, which may call exit.  */
int xerror(Display *display, XErrorEvent *ee) {
    if(ee->error_code == BadWindow
       || (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
       || (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
       || (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
       || (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
       || (ee->request_code == X_GrabKey && ee->error_code == BadAccess))
	return 0;
    fprintf(stderr, "calavera-wm: fatal error: request code=%d, error code=%d\n",
	    ee->request_code, ee->error_code);
    return xerrorxlib(display, ee); /* may call exit */
}

int xerrordummy(Display *display, XErrorEvent *ee) {
    return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int xerrorstart(Display *display, XErrorEvent *ee) {
    eprint("calavera-wm: another window manager is already running\n");
    return -1;
}

void exec(const Arg *arg) {
    int  pos;
    char tmp[32];
    char buf[BUFSIZE];
    Bool grabbing = True;
    KeySym ks;
    XEvent ev;

    // Clear the array
    memset(tmp, 0, sizeof(tmp));
    memset(buf, 0, sizeof(buf));
    pos = 0;

    XGrabKeyboard(display, ROOT, True, GrabModeAsync, GrabModeAsync, CurrentTime);
    sync_display();

    // grab keys
    while(grabbing){
	if(ev.type == KeyPress) {
	    XLookupString(&ev.xkey, tmp, sizeof(tmp), &ks, 0);

	    switch(ks){
	    case XK_Return:
		goto launch;
		grabbing = False;
		break;
	    case XK_BackSpace:
		if(pos) buf[--pos] = 0;
		break;
	    case XK_Escape:
              goto out;
		break;
	    default:
		strncat(buf, tmp, sizeof(tmp));
		++pos;
		break;
	    }
	    sync_display();
	}
	XNextEvent(display, &ev);
    }

 launch:
    if (pos) {
	char *termcmd[]  = { buf, NULL };
        Arg arg = {.v = termcmd };
	spawn (&arg);
    }
  
 out:
    XUngrabKeyboard(display, CurrentTime);
 
    return;
}

int main(int argc, char *argv[]) {
    if(argc == 2 && !strcmp("-v", argv[1]))
	eprint("calavera-wm-"VERSION", © 2006-2012 dwm engineers, see LICENSE for details\n");
    else if(argc != 1)
	eprint("usage: calavera-wm [-v]\n");
    if(!setlocale(LC_CTYPE, "") || !XSupportsLocale())
	fputs("warning: no locale support\n", stderr);
    if(!(display = XOpenDisplay(NULL)))
	eprint("calavera-wm: cannot open display\n");
    cargv = argv;
    checkotherwm();
    setup();
    scan();
    autorun();
    handle_events();
    cleanup();
    /* Close display */
    XCloseDisplay(display);
    return EXIT_SUCCESS;
}
