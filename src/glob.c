/*
 * glob.c: portable glob() for platforms that lack <glob.h> (e.g. mingw-w64)
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

#include <config.h>

#include "glob-compat.h"

/* Everything here is only needed where the platform lacks glob(); on POSIX
 * this translation unit is empty bar a dummy typedef (see bottom). */
#ifdef _WIN32

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fnmatch.h>

/* This is a small, self-contained glob() (NOT derived from glibc/gnulib's
 * GPL implementation). It supports wildcards (`*`, `?`, `[...]`) in any path
 * component by walking the directory tree component-by-component, which is
 * everything augeas asks of glob(). It deliberately omits features augeas
 * never uses: brace/tilde expansion, GLOB_MARK, custom error callbacks, etc. */

static int has_meta(const char *s) {
    return s[strcspn(s, "*?[")] != '\0';
}

/* opendir() argument for a prefix: "" means the current directory. */
static const char *dir_arg(const char *base) {
    return base[0] != '\0' ? base : ".";
}

/* Join a directory prefix and a name into a freshly allocated path, matching
 * the spelling glob() would return ("" prefix -> bare name, "/" -> /name). */
static char *path_join(const char *base, const char *name) {
    char *res;
    if (base[0] == '\0') {
        res = strdup(name);
    } else if (base[1] == '\0' && base[0] == '/') {
        if (asprintf(&res, "/%s", name) < 0)
            res = NULL;
    } else {
        if (asprintf(&res, "%s/%s", base, name) < 0)
            res = NULL;
    }
    return res;
}

static int append_match(glob_t *pglob, char *path) {
    char **nv = realloc(pglob->gl_pathv,
                        (pglob->gl_pathc + 2) * sizeof(*nv));
    if (nv == NULL)
        return -1;
    pglob->gl_pathv = nv;
    pglob->gl_pathv[pglob->gl_pathc++] = path;
    pglob->gl_pathv[pglob->gl_pathc] = NULL;
    return 0;
}

/* Recursively expand comps[ci..ncomp) against the directory prefix `base`.
 * Returns 0 on success or GLOB_NOSPACE on allocation failure. */
static int expand(const char *base, char **comps, int ci, int ncomp,
                  glob_t *pglob) {
    const char *comp = comps[ci];
    int last = (ci + 1 == ncomp);

    if (!has_meta(comp)) {
        /* Literal component: descend without scanning the directory. */
        char *next = path_join(base, comp);
        if (next == NULL)
            return GLOB_NOSPACE;
        int rc = 0;
        if (last) {
            struct stat st;
            if (stat(next, &st) == 0) {
                if (append_match(pglob, next) < 0) {
                    free(next);
                    rc = GLOB_NOSPACE;
                }
                /* on success `next` is owned by gl_pathv */
            } else {
                free(next);
            }
        } else {
            rc = expand(next, comps, ci + 1, ncomp, pglob);
            free(next);
        }
        return rc;
    }

    /* Wildcard component: scan the directory and match each entry. */
    DIR *d = opendir(dir_arg(base));
    if (d == NULL)
        return 0;               /* nothing matches down this branch */

    int rc = 0;
    struct dirent *e;
    while (rc == 0 && (e = readdir(d)) != NULL) {
        /* FNM_PERIOD keeps `*`/`?` from matching a leading dot, so "." and
         * ".." (and hidden files) are skipped unless explicitly requested. */
        if (fnmatch(comp, e->d_name, FNM_PERIOD) != 0)
            continue;
        char *next = path_join(base, e->d_name);
        if (next == NULL) {
            rc = GLOB_NOSPACE;
            break;
        }
        if (last) {
            if (append_match(pglob, next) < 0) {
                free(next);
                rc = GLOB_NOSPACE;
            }
            /* next is owned by gl_pathv on success */
        } else {
            rc = expand(next, comps, ci + 1, ncomp, pglob);
            free(next);
        }
    }
    closedir(d);
    return rc;
}

int glob(const char *pattern, int flags,
         int (*errfunc)(const char *epath, int eerrno), glob_t *pglob) {
    (void) errfunc;

    if (!(flags & GLOB_APPEND)) {
        pglob->gl_pathc = 0;
        pglob->gl_pathv = NULL;
        pglob->gl_offs = 0;
    }
    size_t start = pglob->gl_pathc;

    /* Split the pattern into '/'-separated components, collapsing empty ones
     * (so "a//b" behaves like "a/b"). A leading '/' makes it absolute. */
    char *dup = strdup(pattern);
    if (dup == NULL)
        return GLOB_NOSPACE;
    int absolute = (pattern[0] == '/');

    char **comps = NULL;
    int ncomp = 0, cap = 0;
    int rc = 0;
    for (char *tok = strtok(dup, "/"); tok != NULL; tok = strtok(NULL, "/")) {
        if (ncomp == cap) {
            cap = cap ? cap * 2 : 8;
            char **nc = realloc(comps, cap * sizeof(*nc));
            if (nc == NULL) { rc = GLOB_NOSPACE; goto done; }
            comps = nc;
        }
        comps[ncomp++] = tok;
    }

    if (ncomp > 0)
        rc = expand(absolute ? "/" : "", comps, 0, ncomp, pglob);

 done:
    free(comps);
    free(dup);
    if (rc != 0)
        return rc;
    return pglob->gl_pathc > start ? 0 : GLOB_NOMATCH;
}

void globfree(glob_t *pglob) {
    if (pglob->gl_pathv != NULL) {
        for (size_t i = 0; i < pglob->gl_pathc; i++)
            free(pglob->gl_pathv[i]);
        free(pglob->gl_pathv);
        pglob->gl_pathv = NULL;
    }
    pglob->gl_pathc = 0;
}

#else /* !_WIN32 */

/* Avoid an empty translation unit (undefined behaviour in ISO C) when the
 * platform provides its own glob(). */
typedef int glob_compat_iso_c_dummy;

#endif /* _WIN32 */
