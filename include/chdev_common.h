/* 
 * Copyright (C) 2014 Sergey Morozov
 * 
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.
 */

/*
 * Definitions for ioctl().
 */
#define CHDEV_IOCTL_MAGIC           'k'
#define CHDEV_IOCTL_GET_ITEM        _IOR(CHDEV_IOCTL_MAGIC,  0, char *)
#define CHDEV_IOCTL_SET_ITEM        _IOW(CHDEV_IOCTL_MAGIC,  1, char *)
#define CHDEV_IOCTL_GET_NUM_ITEM    _IOR(CHDEV_IOCTL_MAGIC,  2, uint  )
#define CHDEV_IOCTL_GET_BUF_SIZE    _IOR(CHDEV_IOCTL_MAGIC,  3, uint  )
#define CHDEV_IOCTL_MAXNR           4 

/*
 * Definitions of shared structures.
 */
struct chdev_item {
    char  *buf; /* item buffer */
    short size; /* item size (in bytes) */
} __attribute__ ((__packed__)) ;
  