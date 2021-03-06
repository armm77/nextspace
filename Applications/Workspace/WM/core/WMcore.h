/*
 *  Workspace window manager
 *  Copyright (c) 2015-2021 Sergii Stoian
 *
 *  WINGs library (Window Maker)
 *  Copyright (c) 1998 scottc
 *  Copyright (c) 1999-2004 Dan Pascu
 *  Copyright (c) 1999-2000 Alfredo K. Kojima
 *  Copyright (c) 2014 Window Maker Team
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

#ifndef __WORKSPACE_WM_WMCORE__
#define __WORKSPACE_WM_WMCORE__

#include <X11/Xlib.h>
#include <limits.h>
#include <sys/types.h>

#ifndef WMAX
# define WMAX(a,b)	((a)>(b) ? (a) : (b))
#endif
#ifndef WMIN
# define WMIN(a,b)	((a)<(b) ? (a) : (b))
#endif

#ifndef __GNUC__
#define  __attribute__(x)  /*NOTHING*/
#endif

#ifdef NDEBUG

#define wassertr(expr) if (!(expr)) { return; }

#define wassertrv(expr, val) if (!(expr)) { return (val); }

#else /* !NDEBUG */

#define wassertr(expr) \
    if (!(expr)) { \
        WMLogWarning("wassertr: assertion %s failed", #expr); \
        return;\
    }

#define wassertrv(expr, val) \
    if (!(expr)) { \
        WMLogWarning("wassertrv: assertion %s failed", #expr); \
        return (val);\
    }

#endif /* !NDEBUG */


#ifdef static_assert
# define _wutil_static_assert(check, message) static_assert(check, message)
#else
# ifdef __STDC_VERSION__
#  if __STDC_VERSION__ >= 201112L
/*
 * Ideally, we would like to include <assert.h> to have 'static_assert'
 * properly defined, but as we have to be sure about portability and
 * because we're a public header we can't count on 'configure' to tell
 * us about availability, so we use the raw C11 keyword
 */
#   define _wutil_static_assert(check, message) _Static_assert(check, message)
#  else
#   define _wutil_static_assert(check, message) /**/
#  endif
# else
#  define _wutil_static_assert(check, message) /**/
# endif
#endif


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* ---[ Macros ]---------------------------------------------------------- */

#define wlengthof(array)                                                \
  ({                                                                    \
    _wutil_static_assert(sizeof(array) > sizeof(array[0]),              \
                         "the macro 'wlengthof' cannot be used on pointers, only on known size arrays"); \
    sizeof(array) / sizeof(array[0]);                                   \
  })

/*-------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __WORKSPACE_WM_WMCORE__ */
