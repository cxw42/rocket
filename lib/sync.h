/* Copyright (C) 2010 Contributors
 * For conditions of distribution and use, see copyright notice in COPYING
 */

#ifndef SYNC_H
#define SYNC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

struct sync_device;
struct sync_track;

struct sync_device *sync_create_device(const char *);
void sync_destroy_device(struct sync_device *);

#ifndef SYNC_PLAYER
/* Editor interface */
struct sync_cb {
	void (*pause)(void *, int);
	void (*set_row)(void *, int);
	int (*is_playing)(void *);
};
#define SYNC_DEFAULT_PORT 1338
int sync_connect(struct sync_device *, const char *, unsigned short);
int sync_update(struct sync_device *, int, struct sync_cb *, void *);
void sync_save_tracks(const struct sync_device *);
int sync_has_keys(const struct sync_track *);
    /*  returns 1 if the given track has any keys, otherwise 0.  A sanity
        check in case the editor hasn't connected yet. */

#else /* defined(SYNC_PLAYER) */
struct sync_io_cb {
	void *(*open)(const char *filename, const char *mode);
	size_t (*read)(void *ptr, size_t size, size_t nitems, void *stream);
	int (*close)(void *stream);
};
void sync_set_io_cb(struct sync_device *d, struct sync_io_cb *cb);
#endif /* defined(SYNC_PLAYER) */

const struct sync_track *sync_get_track(struct sync_device *, const char *);
double sync_get_val(const struct sync_track *, double);

int sync_get_first_row_this_interval(const struct sync_track *, double);
    /*  given a row number, return the row number for the beginning of the
        interval containing that row. */

int sync_get_first_row_next_interval(const struct sync_track *, double);
    /*  given a row number, return the row number for the beginning of the
        interval following that row, or -1 if there isn't one (i.e.,
        the row is in the last interval) */


#ifdef __cplusplus
}
#endif

#endif /* !defined(SYNC_H) */
