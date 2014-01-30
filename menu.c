/* Copyright (c) 1994-1996 David Hogan, see README for licence details */
#include <stdio.h>
#include <signal.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "dat.h"
#include "fns.h"

Client	*hiddenc[MAXHIDDEN];

int	numhidden;

char	*b3items[B3FIXED+MAXHIDDEN+1] =
{
        "Run...",
	"New",
	"Reshape",
	"Move",
	"Delete",
	"Hide",
	0,
};

Menu	b3menu =
{
	b3items,
};

Menu	egg =
{
	version,
};

void
button(e)
XButtonEvent *e;
{
	int n, shift;
	Client *c;
	Window dw;
	ScreenInfo *s;

	curtime = e->time;
	s = getscreen(e->root);
	if (s == 0)
		return;
	c = getclient(e->window, 0);
	if (c) {
		e->x += c->x - BORDER + 1;
		e->y += c->y - BORDER + 1;
	}
	else if (e->window != e->root)
		XTranslateCoordinates(dpy, e->window, s->root, e->x, e->y,
				&e->x, &e->y, &dw);
	switch (e->button) {
	case Button1:
		if (c) {
			XMapRaised(dpy, c->parent);
			top(c);
			active(c);
		}
		return;
	case Button2:
		if ((e->state&(ShiftMask|ControlMask))==(ShiftMask|ControlMask))
			menuhit(e, &egg);
		return;
	default:
		return;
	case Button3:
		break;
	}

	if (current && current->screen == s)
		cmapnofocus(s);
	switch (n = menuhit(e, &b3menu)) {
	case 0:          /* Run*/
                launcher();
        case 1: 	/* New */
		spawn(s);
		break;
	case 2: 	/* Reshape */
		reshape(selectwin(1, 0, s));
		break;
	case 3: 	/* Move */
		move(selectwin(0, 0, s));
		break;
	case 4: 	/* Delete */
		shift = 0;
		c = selectwin(1, &shift, s);
		delete(c, shift);
		break;
	case 5: 	/* Hide */
		hide(selectwin(1, 0, s));
		break;
	default:	/* unhide window */
		unhide(n - B3FIXED, 1);
		break;
	case -1:	/* nothing */
		break;
	}
	if (current && current->screen == s)
		cmapfocus(current);
}

void
launcher()
{
	/*
         * Added a Launcher (run menu for launch apps with gmrun)
	 * ugly dance to avoid leaving zombies.  Could use SIGCHLD,
	 * but it's not very portable.
	 */
	if (fork() == 0) {
		if (fork() == 0) {
			close(ConnectionNumber(dpy));
                        execlp("gmrun", "gmrun", "-ut", 0);
			perror("9wm: exec gmrun/ failed");
			exit(1);
		}
		exit(0);
	}
	wait((int *) 0);
}

void
spawn(s)
ScreenInfo *s;
{
	/*
	 * ugly dance to avoid leaving zombies.  Could use SIGCHLD,
	 * but it's not very portable.
	 */
	if (fork() == 0) {
		if (fork() == 0) {
			close(ConnectionNumber(dpy));
			if (s->display[0] != '\0')
				putenv(s->display);
			if (termprog != NULL) {
				execl(shell, shell, "-c", termprog, 0);
				fprintf(stderr, "9wm: exec %s", shell);
				perror(" failed");
			}
			execlp("9term", "9term", "-9wm", 0);
//			execlp("xterm", "xterm", "-ut", 0);
                        execlp("urxvt", "urxvt", "-ut", 0);
			perror("9wm: exec 9term/urxvt failed");
			exit(1);
		}
		exit(0);
	}
	wait((int *) 0);
}

void
reshape(c)
Client *c;
{
	int odx, ody;

	if (c == 0)
		return;
	odx = c->dx;
	ody = c->dy;
	if (sweep(c) == 0)
		return;
	active(c);
	top(c);
	XRaiseWindow(dpy, c->parent);
	XMoveResizeWindow(dpy, c->parent, c->x-BORDER, c->y-BORDER,
					c->dx+2*(BORDER-1), c->dy+2*(BORDER-1));
	if (c->dx == odx && c->dy == ody)
		sendconfig(c);
	else
		XMoveResizeWindow(dpy, c->window, BORDER-1, BORDER-1, c->dx, c->dy);
}

void
move(c)
Client *c;
{
	if (c == 0)
		return;
	if (drag(c) == 0)
		return;
	active(c);
	top(c);
	XRaiseWindow(dpy, c->parent);
	XMoveWindow(dpy, c->parent, c->x-BORDER, c->y-BORDER);
	sendconfig(c);
}

void
delete(c, shift)
Client *c;
int shift;
{
	if (c == 0)
		return;
	if ((c->proto & Pdelete) && !shift)
		sendcmessage(c->window, wm_protocols, wm_delete, 0);
	else
		XKillClient(dpy, c->window);		/* let event clean up */
}

void
hide(c)
Client *c;
{
	if (c == 0 || numhidden == MAXHIDDEN)
		return;
	if (hidden(c)) {
		fprintf(stderr, "9wm: already hidden: %s\n", c->label);
		return;
	}
	XUnmapWindow(dpy, c->parent);
	XUnmapWindow(dpy, c->window);
	_setstate(c, IconicState);
	if (c == current)
		nofocus();
	hiddenc[numhidden] = c;
	b3items[B3FIXED+numhidden] = c->label;
	numhidden++;
	b3items[B3FIXED+numhidden] = 0;
}

void
unhide(n, map)
int n;
int map;
{
	Client *c;
	int i;

	if (n >= numhidden) {
		fprintf(stderr, "9wm: unhide: n %d numhidden %d\n", n, numhidden);
		return;
	}
	c = hiddenc[n];
	if (!hidden(c)) {
		fprintf(stderr, "9wm: unhide: not hidden: %s(0x%x)\n",
			c->label, c->window);
		return;
	}

	if (map) {
		XMapWindow(dpy, c->window);
		XMapRaised(dpy, c->parent);
		_setstate(c, NormalState);
		active(c);
		top(c);
	}

	numhidden--;
	for (i = n; i < numhidden; i ++) {
		hiddenc[i] = hiddenc[i+1];
		b3items[B3FIXED+i] = b3items[B3FIXED+i+1];
	}
	b3items[B3FIXED+numhidden] = 0;
}

void
unhidec(c, map)
Client *c;
int map;
{
	int i;

	for (i = 0; i < numhidden; i++)
		if (c == hiddenc[i]) {
			unhide(i, map);
			return;
		}
	fprintf(stderr, "9wm: unhidec: not hidden: %s(0x%x)\n",
		c->label, c->window);
}

void
renamec(c, name)
Client *c;
char *name;
{
	int i;

	if (name == 0)
		name = "???";
	c->label = name;
	if (!hidden(c))
		return;
	for (i = 0; i < numhidden; i++)
		if (c == hiddenc[i]) {
			b3items[B3FIXED+i] = name;
			return;
		}
}
