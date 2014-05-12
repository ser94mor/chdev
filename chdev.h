/*
 * Copyright (C) 2014 Sergey Morozov
 * 
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.
 */

#include <linux/types.h>

/*
 * Definition of constants.
 */
#define BUF_5KB          5120

/*
 * Definition of structures.
 */
struct chdev_dev {
	char             *buf;	                    /* chdev circular buffer, item size are stored in first sizeof(short) bytes */        
	uint             buf_size;                  /* size of chdev circular buffer */
	char             *beg;                      /* pointer to the current begining of the buffer */
	char             *end;                      /* pointer to the current end of the buffer */
	bool             inv;                       /* indicator of beg and end positions ([--beg--end--]:false, [--end--beg--]:true, [beg == end]:false) */
	uint             num_item;                  /* number of items in the buffer at the current time point */
	struct semaphore sem;                       /* mutual exclusion semaphore  */
	struct cdev      cdev;	                    /* chdev structure */
};