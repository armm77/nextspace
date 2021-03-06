/*
 *  Workspace window manager
 *  Copyright (c) 2015-2021 Sergii Stoian
 *
 *  WINGs library (Window Maker)
 *  Copyright (c) 1998 scottc
 *  Copyright (c) 1999-2004 Dan Pascu
 *  Copyright (c) 1999-2000 Alfredo K. Kojima
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdlib.h>
#include <string.h>
#include <CoreFoundation/CFArray.h>

#include <X11/Xatom.h>

#include "wscreen.h"
#include "dragcommon.h"
#include "drawing.h"
#include "util.h"

#include "selection.h"

#define MAX_PROPERTY_SIZE 8*1024

typedef struct SelectionHandler {
  WMView *view;
  Atom selection;
  Time timestamp;
  WMSelectionProcs procs;
  void *data;

  struct {
    unsigned delete_pending:1;
    unsigned done_pending:1;
  } flags;
} SelectionHandler;

typedef struct SelectionCallback {
  WMView *view;
  Atom selection;
  Atom target;
  Time timestamp;
  WMSelectionCallback *callback;
  void *data;

  struct {
    unsigned delete_pending:1;
    unsigned done_pending:1;
  } flags;
} SelectionCallback;

static CFMutableArrayRef selCallbacks = NULL;

static CFMutableArrayRef selHandlers = NULL;

static Bool gotXError = False;

void WMDeleteSelectionHandler(WMView *view, Atom selection, Time timestamp)
{
  SelectionHandler *handler;
  Display *dpy = WMViewScreen(view)->display;
  Window win = WMViewDrawable(view);

  if (!selHandlers)
    return;

  /*//printf("deleting selection handler for %d", win); */

  for (int i = 0; i < CFArrayGetCount(selHandlers); i++) {
    handler = (SelectionHandler *)CFArrayGetValueAtIndex(selHandlers, i);
    if (handler->view == view && (handler->selection == selection || selection == None)
        && (handler->timestamp == timestamp || timestamp == CurrentTime)) {

      if (handler->flags.done_pending) {
        handler->flags.delete_pending = 1;
        /*//puts(": postponed because still pending"); */
        return;
      }
      /*//printf(": found & removed"); */
      CFArrayRemoveValueAtIndex(selHandlers, i);
      break;
    }
  }

  /*//printf("\n"); */

  XGrabServer(dpy);
  if (XGetSelectionOwner(dpy, selection) == win) {
    XSetSelectionOwner(dpy, selection, None, timestamp);
  }
  XUngrabServer(dpy);
}

static void WMDeleteSelectionCallback(WMView *view, Atom selection, Time timestamp)
{
  SelectionCallback *handler;

  if (!selCallbacks)
    return;

  for (int i = 0; i < CFArrayGetCount(selCallbacks); i++) {
    handler = (SelectionCallback *)CFArrayGetValueAtIndex(selCallbacks, i);
    if (handler->view == view && (handler->selection == selection || selection == None)
        && (handler->timestamp == timestamp || timestamp == CurrentTime)) {
      if (handler->flags.done_pending) {
        handler->flags.delete_pending = 1;
        return;
      }
      CFArrayRemoveValueAtIndex(selCallbacks, i);
      break;
    }
  }
}

static int handleXError(Display *dpy, XErrorEvent *ev)
{
  /* Parameter not used, but tell the compiler that it is ok */
  (void) dpy;
  (void) ev;

  gotXError = True;

  return 1;
}

static Bool writeSelection(Display *dpy, Window requestor, Atom property, Atom type, WMData *data)
{
  static void *oldHandler;
  int format, bpi;

  format = WMGetDataFormat(data);
  if (format == 0)
    format = 8;

  bpi = format / 8;

  /* printf("write to %x: %s\n", requestor, XGetAtomName(dpy, property)); */

  oldHandler = XSetErrorHandler(handleXError);

  gotXError = False;

  XChangeProperty(dpy, requestor, property, type, format, PropModeReplace,
                  WMDataBytes(data), WMGetDataLength(data) / bpi);

  XFlush(dpy);

  XSetErrorHandler(oldHandler);

  return !gotXError;
}

static void notifySelection(XEvent *event, Atom prop)
{
  XEvent ev;

  /* printf("event to %x\n", event->xselectionrequest.requestor); */

  ev.xselection.type = SelectionNotify;
  ev.xselection.serial = 0;
  ev.xselection.send_event = True;
  ev.xselection.display = event->xselectionrequest.display;
  ev.xselection.requestor = event->xselectionrequest.requestor;
  ev.xselection.target = event->xselectionrequest.target;
  ev.xselection.selection = event->xselectionrequest.selection;
  ev.xselection.property = prop;
  ev.xselection.time = event->xselectionrequest.time;

  XSendEvent(event->xany.display, event->xselectionrequest.requestor, False, 0, &ev);
  XFlush(event->xany.display);
}

static void handleRequestEvent(XEvent *event)
{
  SelectionHandler *handler;
  CFArrayRef copy;
  Bool handledRequest;

  for (int i = 0; i < CFArrayGetCount(selHandlers); i++) {
    handler = (SelectionHandler *)CFArrayGetValueAtIndex(selHandlers, i);

    switch (event->type) {
    case SelectionClear:
      if (WMViewDrawable(handler->view)
          != event->xselectionclear.window) {
        break;
      }

      handler->flags.done_pending = 1;
      if (handler->procs.selectionLost)
        handler->procs.selectionLost(handler->view, handler->selection, handler->data);
      handler->flags.done_pending = 0;
      handler->flags.delete_pending = 1;
      break;

    case SelectionRequest:
      if (WMViewDrawable(handler->view) != event->xselectionrequest.owner) {
        break;
      }

      if (handler->procs.convertSelection != NULL
          && handler->selection == event->xselectionrequest.selection) {
        Atom atom;
        WMData *data;
        Atom prop;

        /* they're requesting for something old.. maybe another handler
         *can handle it */
        if (event->xselectionrequest.time < handler->timestamp
            && event->xselectionrequest.time != CurrentTime) {
          break;
        }

        handledRequest = False;

        handler->flags.done_pending = 1;

        data = handler->procs.convertSelection(handler->view,
                                               handler->selection,
                                               event->xselectionrequest.target,
                                               handler->data, &atom);

        prop = event->xselectionrequest.property;
        /* obsolete clients that don't set the property field */
        if (prop == None)
          prop = event->xselectionrequest.target;

        if (data) {
          if (writeSelection(event->xselectionrequest.display,
                             event->xselectionrequest.requestor, prop, atom, data)) {
            handledRequest = True;
          }
          WMReleaseData(data);
        }

        notifySelection(event, (handledRequest == True ? prop : None));

        if (handler->procs.selectionDone != NULL) {
          handler->procs.selectionDone(handler->view,
                                       handler->selection,
                                       event->xselectionrequest.target,
                                       handler->data);
        }

        handler->flags.done_pending = 0;
      }
      break;
    }
  }

  /* delete handlers */
  copy = CFArrayCreateCopy(kCFAllocatorDefault, selHandlers);
  for (int i = 0; i < CFArrayGetCount(copy); i++) {
    handler = (SelectionHandler *)CFArrayGetValueAtIndex(copy, i);
    if (handler && handler->flags.delete_pending) {
      WMDeleteSelectionHandler(handler->view, handler->selection, handler->timestamp);
    }
  }
  CFRelease(copy);
}

static WMData *getSelectionData(Display *dpy, Window win, Atom where)
{
  WMData *wdata;
  unsigned char *data;
  Atom rtype;
  int bits, bpi;
  unsigned long len, bytes;

  if (XGetWindowProperty(dpy, win, where, 0, MAX_PROPERTY_SIZE,
                         False, AnyPropertyType, &rtype, &bits, &len, &bytes, &data) != Success) {
    return NULL;
  }

  bpi = bits / 8;

  wdata = WMCreateDataWithBytesNoCopy(data, len *bpi, (WMFreeDataProc *) XFree);
  WMSetDataFormat(wdata, bits);

  return wdata;
}

static void handleNotifyEvent(XEvent *event)
{
  SelectionCallback *handler;
  CFArrayRef copy;
  WMData *data;

  for (int i = 0; i < CFArrayGetCount(selCallbacks); i++) {
    handler = (SelectionCallback *)CFArrayGetValueAtIndex(selCallbacks, i);

    if (WMViewDrawable(handler->view) != event->xselection.requestor
        || handler->selection != event->xselection.selection) {
      continue;
    }
    handler->flags.done_pending = 1;

    if (event->xselection.property == None) {
      data = NULL;
    } else {
      data = getSelectionData(event->xselection.display,
                              event->xselection.requestor, event->xselection.property);
    }

    (*handler->callback) (handler->view, handler->selection,
                          handler->target, handler->timestamp, handler->data, data);

    if (data != NULL) {
      WMReleaseData(data);
    }
    handler->flags.done_pending = 0;
    handler->flags.delete_pending = 1;
  }

  /* delete callbacks */
  copy = CFArrayCreateCopy(kCFAllocatorDefault, selCallbacks);
  for (int i = 0; i < CFArrayGetCount(selCallbacks); i++) {
    handler = (SelectionCallback *)CFArrayGetValueAtIndex(selCallbacks, i);
    if (handler && handler->flags.delete_pending) {
      WMDeleteSelectionCallback(handler->view, handler->selection, handler->timestamp);
    }
  }
  CFRelease(copy);
}

void WMHandleSelectionEvent(XEvent *event)
{
  /*//printf("%d received selection ", event->xany.window); */
  /*//switch(event->type) {
    case SelectionNotify:
    puts("notify"); break;
    case SelectionRequest:
    puts("request"); break;
    case SelectionClear:
    puts("clear"); break;
    default:
    puts("unknown"); break;
    } */

  if (event->type == SelectionNotify) {
    handleNotifyEvent(event);
  } else {
    handleRequestEvent(event);
  }
}

static void freeArrayItemCallback(CFAllocatorRef allocator, const void *item)
{
  wfree((void *)item);
}

Bool WMCreateSelectionHandler(WMView *view, Atom selection, Time timestamp, WMSelectionProcs *procs, void *cdata)
{
  SelectionHandler *handler;
  Display *dpy = WMViewScreen(view)->display;

  XSetSelectionOwner(dpy, selection, WMViewDrawable(view), timestamp);
  if (XGetSelectionOwner(dpy, selection) != WMViewDrawable(view)) {
    return False;
  }

  /*//printf("created selection handler for %d\n", WMViewDrawable(view)); */

  handler = wmalloc(sizeof(SelectionHandler));
  handler->view = view;
  handler->selection = selection;
  handler->timestamp = timestamp;
  handler->procs = *procs;
  handler->data = cdata;
  memset(&handler->flags, 0, sizeof(handler->flags));

  if (selHandlers == NULL) {
    CFArrayCallBacks cbs = {0, NULL, freeArrayItemCallback, NULL, NULL};
    selHandlers = CFArrayCreateMutable(kCFAllocatorDefault, 4, &cbs);
  }

  CFArrayAppendValue(selHandlers, handler);

  return True;
}

Bool
WMRequestSelection(WMView *view, Atom selection, Atom target, Time timestamp,
		   WMSelectionCallback *callback, void *cdata)
{
  SelectionCallback *handler;

  if (XGetSelectionOwner(WMViewScreen(view)->display, selection) == None)
    return False;

  if (!XConvertSelection(WMViewScreen(view)->display, selection, target,
                         WMViewScreen(view)->clipboardAtom, WMViewDrawable(view), timestamp)) {
    return False;
  }

  handler = wmalloc(sizeof(SelectionCallback));
  handler->view = view;
  handler->selection = selection;
  handler->target = target;
  handler->timestamp = timestamp;
  handler->callback = callback;
  handler->data = cdata;

  if (selHandlers == NULL) {
    CFArrayCallBacks cbs = {0, NULL, freeArrayItemCallback, NULL, NULL};
    selCallbacks = CFArrayCreateMutable(kCFAllocatorDefault, 4, &cbs);
  }

  CFArrayAppendValue(selCallbacks, handler);

  return True;
}
