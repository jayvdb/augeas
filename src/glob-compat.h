/*
 * glob-compat.h: portable glob() for platforms that lack <glob.h>
 *
 * Copyright (C) 2007-2016 David Lutterkort
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 */

#ifndef GLOB_COMPAT_H_
#define GLOB_COMPAT_H_

#ifndef _WIN32

/* POSIX platforms have a perfectly good glob(); use it. */
#include <glob.h>

#else /* _WIN32 */

/* mingw-w64 ships no <glob.h> / glob(). This is a small, self-contained
 * replacement (not derived from glibc/gnulib's GPL glob) covering only the
 * subset augeas relies on: matching "<dir>/<pattern>" against directory
 * entries, the GLOB_NOSORT / GLOB_APPEND flags, and the GLOB_NOMATCH /
 * GLOB_NOSPACE return codes. */

#include <stddef.h>

#define GLOB_NOSORT  0x01    /* don't sort results (we never sort) */
#define GLOB_APPEND  0x02    /* append to results of a previous call */

#define GLOB_NOMATCH 1       /* no matches found */
#define GLOB_NOSPACE 2       /* out of memory */
#define GLOB_ABORTED 3       /* read error (unused here, for completeness) */

typedef struct {
    size_t gl_pathc;         /* count of paths matched so far */
    char **gl_pathv;         /* NULL-padded list of matched paths */
    size_t gl_offs;          /* unused; present for API compatibility */
} glob_t;

int glob(const char *pattern, int flags,
         int (*errfunc)(const char *epath, int eerrno), glob_t *pglob);
void globfree(glob_t *pglob);

#endif /* _WIN32 */

#endif /* GLOB_COMPAT_H_ */
