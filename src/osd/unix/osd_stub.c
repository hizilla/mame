/***************************************************************************

    main.c

    stub for unix os dependence.

    Copyright (c) 2024-2024, lixiasong.

***************************************************************************/

#include "osdcore.h"
#include <stdio.h>
#include <time.h>
#include "mamecore.h"
#include "inptport.h"

file_error osd_open(const char *path, UINT32 openflags, osd_file **file, UINT64 *filesize)
{
	const char *mode;
	FILE *fileptr;

	// based on the flags, choose a mode
	if (openflags & OPEN_FLAG_WRITE)
	{
		if (openflags & OPEN_FLAG_READ)
			mode = (openflags & OPEN_FLAG_CREATE) ? "w+b" : "r+b";
		else
			mode = "wb";
	}
	else if (openflags & OPEN_FLAG_READ)
		mode = "rb";
	else
		return FILERR_INVALID_ACCESS;

	// open the file
	fileptr = fopen(path, mode);
	if (fileptr == NULL)
		return FILERR_NOT_FOUND;

	// store the file pointer directly as an osd_file
	*file = (osd_file *)fileptr;

	// get the size -- note that most fseek/ftell implementations are limited to 32 bits
	fseek(fileptr, 0, SEEK_END);
	*filesize = ftell(fileptr);
	fseek(fileptr, 0, SEEK_SET);

	return FILERR_NONE;
}

file_error osd_close(osd_file *file)
{
	// close the file handle
	fclose((FILE *)file);
	return FILERR_NONE;
}

file_error osd_read(osd_file *file, void *buffer, UINT64 offset, UINT32 length, UINT32 *actual)
{
	size_t count;

	// seek to the new location; note that most fseek implementations are limited to 32 bits
	fseek((FILE *)file, offset, SEEK_SET);

	// perform the read
	count = fread(buffer, 1, length, (FILE *)file);
	if (actual != NULL)
		*actual = count;

	return FILERR_NONE;
}

file_error osd_write(osd_file *file, const void *buffer, UINT64 offset, UINT32 length, UINT32 *actual)
{
	size_t count;

	// seek to the new location; note that most fseek implementations are limited to 32 bits
	fseek((FILE *)file, offset, SEEK_SET);

	// perform the write
	count = fwrite(buffer, 1, length, (FILE *)file);
	if (actual != NULL)
		*actual = count;

	return FILERR_NONE;
}

file_error osd_rmfile(const char *filename)
{
    return FILERR_NONE;
}

int osd_uchar_from_osdchar(UINT32 /* unicode_char */ *uchar, const char *osdchar, size_t count)
{
	// we assume a standard 1:1 mapping of characters to the first 256 unicode characters
	*uchar = (UINT8)*osdchar;
	return 1;
}

int osd_is_absolute_path(const char *path)
{
    if ((path == NULL) || (path[0] == '\0')) {
        return 0;
    }
    return (path[0] == '/');
}

#include <unistd.h>
#include <time.h>
//============================================================
//  osd_ticks
//============================================================

osd_ticks_t osd_ticks(void)
{
	// use the standard library clock function
	return times(NULL);
}


//============================================================
//  osd_ticks_per_second
//============================================================

osd_ticks_t osd_ticks_per_second(void)
{
	return sysconf(_SC_CLK_TCK);
}


//============================================================
//  osd_profiling_ticks
//============================================================

osd_ticks_t osd_profiling_ticks(void)
{
	// on x86 platforms, we should return the value of RDTSC here
	// generically, we fall back to clock(), which hopefully is
	// fast
	return times(NULL);
}


//============================================================
//  osd_sleep
//============================================================

void osd_sleep(osd_ticks_t duration)
{
	// if there was a generic, cross-platform way to give up
	// time, this is where we would do it
	osd_ticks_t ticks = osd_ticks_per_second();
	int sleep_us = 1000 * 1000 * duration / ticks;
	usleep(sleep_us);
}


//============================================================
//  osd_lock_alloc
//============================================================

osd_lock *osd_lock_alloc(void)
{
	// the minimal implementation does not support threading
	// just return a dummy value here
	return (osd_lock *)1;
}


//============================================================
//  osd_lock_acquire
//============================================================

void osd_lock_acquire(osd_lock *lock)
{
	// the minimal implementation does not support threading
	// the acquire always "succeeds"
}


//============================================================
//  osd_lock_try
//============================================================

int osd_lock_try(osd_lock *lock)
{
	// the minimal implementation does not support threading
	// the acquire always "succeeds"
	return TRUE;
}


//============================================================
//  osd_lock_release
//============================================================

void osd_lock_release(osd_lock *lock)
{
	// the minimal implementation does not support threading
	// do nothing here
}


//============================================================
//  osd_lock_free
//============================================================

void osd_lock_free(osd_lock *lock)
{
	// the minimal implementation does not support threading
	// do nothing here
}


//============================================================
//  TYPE DEFINITIONS
//============================================================

struct _osd_work_item
{
	void *result;
};



//============================================================
//  osd_work_queue_alloc
//============================================================

osd_work_queue *osd_work_queue_alloc(int flags)
{
	// this minimal implementation doesn't need to keep any state
	// so we just return a non-NULL pointer
	return (osd_work_queue *)1;
}


//============================================================
//  osd_work_queue_items
//============================================================

int osd_work_queue_items(osd_work_queue *queue)
{
	// we never have pending items
	return 0;
}

osd_work_item *osd_work_item_queue_multiple(osd_work_queue *queue, osd_work_callback callback, INT32 numitems, void *parambase, INT32 paramstep, UINT32 flags)
{
    return NULL;
}

//============================================================
//  osd_work_queue_wait
//============================================================

int osd_work_queue_wait(osd_work_queue *queue, osd_ticks_t timeout)
{
	// never anything to wait for, so do nothing
	return TRUE;
}


//============================================================
//  osd_work_queue_free
//============================================================

void osd_work_queue_free(osd_work_queue *queue)
{
	// never allocated anything, so nothing to do
}


//============================================================
//  osd_work_item_wait
//============================================================

int osd_work_item_wait(osd_work_item *item, osd_ticks_t timeout)
{
	// never anything to wait for, so do nothing
	return TRUE;
}


//============================================================
//  osd_work_item_result
//============================================================

void *osd_work_item_result(osd_work_item *item)
{
	return item->result;
}


//============================================================
//  osd_work_item_release
//============================================================

void osd_work_item_release(osd_work_item *item)
{
	free(item);
}


//============================================================
//  osd_alloc_executable
//============================================================

void *osd_alloc_executable(size_t size)
{
	// to use this version of the code, we have to assume that
	// code injected into a malloc'ed region can be safely executed
	return malloc(size);
}


//============================================================
//  osd_free_executable
//============================================================

void osd_free_executable(void *ptr, size_t size)
{
	free(ptr);
}


//============================================================
//  osd_is_bad_read_ptr
//============================================================

int osd_is_bad_read_ptr(const void *ptr, size_t size)
{
	// there is no standard way to do this, so just say no
	return FALSE;
}


//============================================================
//  osd_break_into_debugger
//============================================================

void osd_break_into_debugger(const char *message)
{
	// there is no standard way to do this, so ignore it
}


void osd_set_mastervolume(int attenuation)
{
}

void osd_customize_inputport_list(input_port_default_entry *defaults)
{
}


#include <dirent.h>
#include <stdlib.h>

struct _osd_directory {
    DIR *dir;
};

osd_directory *osd_opendir(const char *dirname)
{
    osd_directory *od = malloc(sizeof(*od));
    if (od == NULL) {
        return NULL;
    }

    od->dir = opendir(dirname);
    if (od->dir == NULL) {
        free(od);
        return NULL;
    }
    return od;
}

const osd_directory_entry *osd_readdir(osd_directory *od)
{
    if ((od == NULL) || (od->dir == NULL)) {
        return NULL;
    }

    struct dirent *dirent = readdir(od->dir);
    if (dirent == NULL) {
        return NULL;
    }

    osd_directory_entry *ode = malloc(sizeof(*ode));
    if (ode == NULL) {
        return NULL;
    }

    ode->name = dirent->d_name;
    if (dirent->d_type & DT_DIR) {
        ode->type = ENTTYPE_DIR;
    } else if (dirent->d_type & DT_REG) {
        ode->type = ENTTYPE_FILE;
    } else {
        ode->type = ENTTYPE_OTHER;
    }
    return ode;
}

void osd_closedir(osd_directory *dir)
{
    if (dir == NULL) {
        return;
    }
    if (dir->dir != NULL) {
        closedir(dir->dir);
    }
    free(dir);
}

int osd_get_physical_drive_geometry(const char *filename, UINT32 *cylinders, UINT32 *heads, UINT32 *sectors, UINT32 *bps)
{
	// there is no standard way of doing this, so we always return FALSE, indicating
	// that a given path is not a physical drive
	return FALSE;
}

int drc_append_verify_code(void)
{
	return 0;
}

int drc_append_standard_epilogue(void)
{
	return 0;
}

int drc_append_save_call_restore(void)
{
	return 0;
}

int drc_append_set_temp_fp_rounding(void)
{
	return 0;
}

int drc_append_restore_fp_rounding(void)
{
	return 0;
}

int drc_append_set_fp_rounding(void)
{
	return 0;
}
