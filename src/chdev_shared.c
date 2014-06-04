/* 
 * Copyright (C) 2014 Sergey Morozov
 * 
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.
 */

#include <linux/cdev.h>
#include <linux/semaphore.h>
#include <asm/uaccess.h>

#include "chdev.h"

/*
 * Implementation of common part of read functions.
 */
ssize_t chdev_read_common(struct chdev_dev *dev, char __user *buf, size_t count) {
    size_t           used_space = dev->buf_size;       /* occupied space in a dev circular buffer */
    short            item_len = 0;                     /* length of current item in dev->buf, initially equals 0 */
    
    if (dev->num_item == 0) {
        return 0; /* there is nothing to read from buffer */
    }
    
    /* calculation of used space in a buffer, and reading data */   
    #define USED_SPACE                       (dev->end - dev->beg)
    #define USED_SPACE_INV                   (dev->buf_size - (dev->end - dev->beg))
    #define USED_SPACE_UPSIDE                (dev->end - dev->buf)
    #define USED_SPACE_DOWNSIDE              (dev->buf + dev->buf_size - dev->beg)
    
    used_space = dev->inv ? USED_SPACE_INV : USED_SPACE;  
    
    /* determine the way of storing data in the buffer (depends on dev->inv value) */ 
    if (dev->inv) {  
        if (USED_SPACE_DOWNSIDE <= sizeof(short)) {
            /* read item length */
            memcpy((char *)&item_len, dev->beg, USED_SPACE_DOWNSIDE); /* bytes downside the buffer */
            memcpy(((char *)&item_len) + USED_SPACE_DOWNSIDE, dev->buf, sizeof(short) - USED_SPACE_DOWNSIDE); /* bytes upside the buffer */
            
            /* case: input buffer is smaller than item length */
            if ((size_t)item_len > count) {
                return -ENOMEM;
            }
            
            /* copy item to user */
            if (copy_to_user(buf, dev->buf + sizeof(short) - USED_SPACE_DOWNSIDE, item_len)) {               
                return -EFAULT;
            }
            
            /* update dev state */
            dev->beg = dev->buf + sizeof(short) - USED_SPACE_DOWNSIDE + item_len; /* item_len + sizeof(short) bytes were totally read from buffer */
            dev->inv = false; /* dev->beg and dev->end are not inversed now */                   
        } 
        else {
            /* read item length */
            memcpy((char *)&item_len, dev->beg, sizeof(short));
            
            /* case: input buffer is smaller than item length */
            if ((size_t)item_len > count) {
                return -ENOMEM;
            }
            
            if (USED_SPACE_DOWNSIDE > (item_len + sizeof(short))) {
                /* copy item to user */
                if (copy_to_user(buf, dev->beg + sizeof(short), item_len)) {                           
                    return -EFAULT;
                }
                
                /* update dev state */
                dev->beg += (size_t)item_len + sizeof(short); /* item_len + sizeof(short) bytes were totally read from buffer */
                /* dev->beg and dev->end are still inversed */ 
            }
            else {
                /* copy item to user */
                if (copy_to_user(buf, dev->beg + sizeof(short), USED_SPACE_DOWNSIDE - sizeof(short))) { 
                    return -EFAULT;
                }
                if (copy_to_user(buf + USED_SPACE_DOWNSIDE - sizeof(short), dev->buf, (size_t)item_len - USED_SPACE_DOWNSIDE + sizeof(short))) {
                    return -EFAULT;
                }
                
                /* update dev state */
                dev->beg = dev->buf + (size_t)item_len - USED_SPACE_DOWNSIDE + sizeof(short); /* item_len + sizeof(short) bytes */
                /* were totally read from buffer  */
                dev->inv = false;   /* dev->beg and dev-> end are not inversed now */
            }
        }
    }
    else {
        /* read item length */
        memcpy((char *)&item_len, dev->beg, sizeof(short));
        
        /* case: input buffer is smaller than item length */
        if ((size_t)item_len > count) {
            return -ENOMEM;
        }
        
        /* copy item to user */
        if (copy_to_user(buf, dev->beg + sizeof(short), item_len)) { 
            return -EFAULT;
        }
        
        /* update dev state */
        dev->beg += item_len + sizeof(short);
    }
    --dev->num_item; /* one item was read from dev->buf */
    
    /* reset beg and end pointers to default in case num_item == 0 */
    if (dev->num_item == 0) {
        dev->beg = dev->buf;
        dev->end = dev->buf;
        /* dev->inv is already equals false */
    }
    
    #undef  USED_SPACE
    #undef  USED_SPACE_INV
    #undef  USED_SPACE_UPSIDE
    #undef  USED_SPACE_DOWNSIDE 
    
    return item_len;
}


/*
 * Implementation of common part of write functions.
 */
ssize_t chdev_write_common(struct chdev_dev *dev, const char __user *buf, size_t count) {
    size_t           free_space = 0;            /* free space in a dev circular buffer */
    short            item_len   = (short)count; /* length of input data */
    
    /* calculation of a free space in a buffer, and writing data */
    #define FREE_SPACE                       (dev->buf_size - (dev->end - dev->beg))
    #define FREE_SPACE_INV                   (dev->beg - dev->end)
    #define FREE_SPACE_UPSIDE                (dev->beg - dev->buf)
    #define FREE_SPACE_DOWNSIDE              (dev->buf + dev->buf_size - dev->end)
    
    free_space = dev->inv ? FREE_SPACE_INV : FREE_SPACE;
    
    if (count + sizeof(short) > free_space) {
        return -ENOMEM; /* item length is greater than free space in the buffer */
    }
    
    /* determine the way of storing data in the buffer (depends on dev->inv value) */    
    if (dev->inv) {
        /* write item length */
        memcpy(dev->end, (const char *)&item_len, sizeof(short));
        
        /* copy item from user */
        if (copy_from_user(dev->end + sizeof(short), buf, count)) {
            return -EFAULT;
        }
        
        /* update dev state */
        dev->end += count + sizeof(short); /* update end of buffer position */
    }
    else {
        if (count + sizeof(short) <= FREE_SPACE_DOWNSIDE) {
            /* write item length */
            memcpy(dev->end, (const char *)&item_len, sizeof(short)); 
            
            /* copy item from user */            
            if (copy_from_user(dev->end + sizeof(short), buf, count)) {
                return -EFAULT;
            }
            
            /* update dev state */
            dev->end += count + sizeof(short); /* update end of buffer position */
        }
        else {
            if (sizeof(short) >= FREE_SPACE_DOWNSIDE) {
                /* write item length */
                memcpy(dev->end, (const char *)&item_len, FREE_SPACE_DOWNSIDE);
                memcpy(dev->buf, (const char *)&item_len + FREE_SPACE_DOWNSIDE, sizeof(short) - FREE_SPACE_DOWNSIDE);
                
                /* copy item from user */ 
                if (copy_from_user(dev->buf + sizeof(short) - FREE_SPACE_DOWNSIDE, buf, count)) {
                    return -EFAULT;
                }
                
                /* update dev state */
                dev->inv = true;                                                     /* positions of dev->beg and dev->end are now inversed */
                dev->end = dev->buf + sizeof(short) - FREE_SPACE_DOWNSIDE + count;   /* update end of buffer position */
            }
            else {
                /* write item length */
                memcpy(dev->end, (const char *)&item_len, sizeof(short));
                
                /* copy item from user */ 
                if (copy_from_user(dev->end + sizeof(short), buf, FREE_SPACE_DOWNSIDE - sizeof(short))) {
                    return -EFAULT;
                }
                if (copy_from_user(dev->buf, buf + FREE_SPACE_DOWNSIDE - sizeof(short), count - FREE_SPACE_DOWNSIDE + sizeof(short))) {
                    return -EFAULT;
                }
                
                /* update dev state */
                dev->inv = true;                            /* data was splitted */
                dev->end = dev->buf + count - FREE_SPACE;   /* update end of buffer position */           
            }
        }
    }
    ++dev->num_item; /* new item was added to dev->buf */        
    
    #undef  FREE_SPACE
    #undef  FREE_SPACE_INV
    #undef  FREE_SPACE_UPSIDE
    #undef  FREE_SPACE_DOWNSIDE
    
    return item_len;
}
