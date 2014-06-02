/* 
 * Copyright (C) 2014 Sergey Morozov
 * 
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.
 */

#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>

#include "chdev.h"
#include "chdev_common.h"

/*
 * Declarations of functions.
 */
static int  __init     chdev_init_module(void);
static int  __init     chdev_setup(void);
static void            chdev_cleanup_module(void);
static void            chdev_setup_cdev(struct chdev_dev *);
static int             chdev_open(struct inode *, struct file *);
static int             chdev_release(struct inode *, struct file *);
static ssize_t         chdev_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t         chdev_write(struct file *, const char __user *, size_t, loff_t *);
static long            chdev_ioctl(struct file *, unsigned int, unsigned long);
static void __init     chdev_create_proc(void);
static void            chdev_remove_proc(void);
static int             chdev_proc_open(struct inode *, struct file *);
static int             chdev_proc_show(struct seq_file *, void *);

/*
 * Declarations of variables.
 */
static int                    chdev_major   = 0;                    /* dynamic major */
static int                    chdev_minor   = 0; 
static int __initdata         buffer        = BUF_5KB;              /* size of the chdev buffer in 5 KB by default */
static struct chdev_dev       *chdev;
static struct file_operations chdev_fops    = {
    .owner            = THIS_MODULE,
    .open             = chdev_open,
    .release          = chdev_release,
    .read             = chdev_read,
    .write            = chdev_write,
    .unlocked_ioctl   = chdev_ioctl,
};
static struct file_operations chdev_proc_ops = {
    .owner   = THIS_MODULE,
    .open    = chdev_proc_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};

/*
 * Initialization of module parameters.
 */
module_param(buffer, int, 0);
MODULE_PARM_DESC(buffer, "size of chdev buffer in bytes");

MODULE_AUTHOR("Sergey Morozov");
MODULE_LICENSE("Dual BSD/GPL");

/*
 * Implementation of file_operations.open for chdev_fops.
 */
static int chdev_open(struct inode *inode, struct file *filp) {
    struct chdev_dev *dev;    /* device information */
    dev = container_of(inode->i_cdev, struct chdev_dev, cdev);
    filp->private_data = dev; /* for other methods */
    
    return 0;  /* success */
}

/*
 * Implementation of file_operations.release for chdev_fops.
 */
static int chdev_release(struct inode *inode, struct file *filp) {
    return 0;  /* success */
}

/*
 * Implementation of file_operations.read for chdev_fops.
 */
static ssize_t chdev_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) {
    struct chdev_dev *dev = filp->private_data; 
    ssize_t          retval = 0;                 /* 0 because initially we haven't read nothing */
    
    /* enter a critical section */
    if (down_interruptible(&dev->sem)) {
        return -ERESTARTSYS;
    }
    
    retval = chdev_read_common(dev, buf, count); /* call common part of read method */
    
    /* exit a critical section */
    up(&dev->sem);
    
    return retval;
}

/*
 * Implementation of file_operations.write for chdev_fops.
 */
static ssize_t chdev_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {
    struct chdev_dev *dev   = filp->private_data;
    ssize_t          retval = -ENOMEM;      /* -ENOMEM because free_space == 0 by default */
    
    /* enter a critical section */
    if (down_interruptible(&dev->sem)) {
        return -ERESTARTSYS;
    }
    
    retval = chdev_write_common(dev, buf, count); /* call common part of write method */
    
    /* enter a critical section */
    up(&dev->sem);
    return retval;
}

/*
 * Implementation of file_operations.ioctl for chdev_fops.
 */
static long chdev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
    struct chdev_dev *dev = filp->private_data; 
    int               err = 0,
                      retval = 0;
    struct chdev_item item; /* used in read and write requests */
    
    /* extract the type and number bitfields, and don't decode wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok() */
    if (_IOC_TYPE(cmd) != CHDEV_IOCTL_MAGIC) {
        return -ENOTTY;
    }
    
    if (_IOC_NR(cmd) > CHDEV_IOCTL_MAXNR) { 
        return -ENOTTY;
    }
    
    /* the direction is a bitmask, and VERIFY_WRITE catches R/W transfers.                                           */
    /* 'type' is user-oriented, while access_ok is kernel-oriented, so the concept of "read" and "write" is reversed */
    if (_IOC_DIR(cmd) & _IOC_READ) {
        err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
    }
    else if (_IOC_DIR(cmd) & _IOC_WRITE) {
        err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
    }
    
    /* return with -EFAULT if error occured */
    if (err) { 
        return -EFAULT;
    }
    
    switch(cmd) {
        case CHDEV_IOCTL_GET_ITEM:
            /* get chdev_item value from user */
            if (copy_from_user((char *)&item, (char __user *)arg, sizeof(struct chdev_item))) {
                return -EFAULT;
            }
            
            /* enter a critical section */
            if (down_interruptible(&dev->sem)) {
                return -ERESTARTSYS;
            }
            
            /* call common part of read method */
            err = (int)chdev_read_common(dev, item.buf, item.size);
            
            /* exit a critical section */
            up(&dev->sem);
            
            /* if error occured in chdev_read_common(...) */
            if (err < 0) {
                return err;
            }
            break;
            
        case CHDEV_IOCTL_SET_ITEM:
            /* get chdev_item value from user */
            if (copy_from_user((char *)&item, (char __user *)arg, sizeof(struct chdev_item))) {
                return -EFAULT;
            }
            
            /* enter a critical section */
            if (down_interruptible(&dev->sem)) {
                return -ERESTARTSYS;
            }
            
            /* call common part of write method */
            err = (int)chdev_write_common(dev, item.buf, item.size);
            
            /* exit a critical section */
            up(&dev->sem);
            
            /* if error occured in chdev_read_common(...) */
            if (err < 0) {
                return err;
            }
            break;
            
        case CHDEV_IOCTL_GET_NUM_ITEM:
            retval = __put_user(chdev->num_item, (uint __user *)arg);
            break;
            
        case CHDEV_IOCTL_GET_BUF_SIZE:
            retval = __put_user(chdev->buf_size, (uint __user *)arg);
            break;
            
        default:  /* redundant, as cmd was checked against MAXNR */
            return -ENOTTY;
    }
    return retval;
}

/*
 * Create "chdevstat" file in /proc file system.
 */
static void __init chdev_create_proc(void) {
    proc_create("chdev", 0, NULL, &chdev_proc_ops);
}

/*
 * Remove "chdevstat" file from /proc file system.
 */
static void chdev_remove_proc(void) {
    /* no problem if it was not registered */
    remove_proc_entry("chdev", NULL /* parent dir */);
}

/*
 * Implementation of file_operations.open for chdev_proc_ops.
 */
static int chdev_proc_open(struct inode *inode, struct file *filp) {
    return single_open(filp, chdev_proc_show, NULL);
}

/*
 * Implementation of show method for /proc file system
 */
static int chdev_proc_show(struct seq_file *s, void *v) {
    seq_printf(s, "%-20.20s : %10u\n"
    "%-20.20s : %10u\n",
    "Buffer size",  chdev->buf_size,
    "Item counter", chdev->num_item);
    return 0;
}

/*
 * Set up the cdev structure for this device.
 */
static void chdev_setup_cdev(struct chdev_dev *dev) {
    int err; 
    int devno = MKDEV(chdev_major, chdev_minor);
    
    cdev_init(&dev->cdev, &chdev_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &chdev_fops;
    
    err = cdev_add(&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_NOTICE "Error %d adding chdev", err);  
    }
}

/*
 * Set up chdev_dev structure for this device.
 */
static int __init chdev_setup(void) {
    int   result;
    /* allocate chdev memory */
    chdev = kmalloc(sizeof(struct chdev_dev), GFP_KERNEL);
    if (!chdev) {
        result = -ENOMEM;
        goto fail;
    }
    memset(chdev, 0, sizeof(struct chdev_dev));
    
    /* allocate buffer memory */
    chdev->buf = kmalloc(buffer, GFP_KERNEL);
    if (!(chdev->buf)) {
        result = -ENOMEM;
        goto fail;
    }
    chdev->buf_size = buffer;
    
    /* set beg and end pointers */
    chdev->beg   = chdev->buf;
    chdev->end   = chdev->buf;
    chdev->inv   = false;
    
    /* set statistics */
    chdev->num_item   = 0;
    
    /* initialize device */
    chdev_setup_cdev(chdev);
    
    sema_init(&(chdev->sem), 1);
    
    return 0;
    
    fail:
    chdev_cleanup_module();
    return result; /* failed */
}

/*
 * Initialization function.
 */
static int __init chdev_init_module(void) {
    int   result;
    dev_t dev = 0; /* device number */
    
    /* asking for a dinamic major */
    result      = alloc_chrdev_region(&dev, chdev_minor, 1, "chdev");
    chdev_major = MAJOR(dev);
    /* allocation was unsuccessful */
    if (result < 0) {
        printk(KERN_WARNING "chdev: can't get major %d\n", chdev_major);
        return result;
    }
    /* initialize chdev fields */
    result = chdev_setup();
    /* create file in a /proc file system */
    chdev_create_proc();
    
    printk(KERN_DEBUG "chdev_init_module: result == %d", result);  
    
    return result;      /* succeed or failed */
}

/*
 * The cleanup function is used as module exit function and to handle initialization failures.
 */
static void chdev_cleanup_module(void) {
    dev_t devno = MKDEV(chdev_major, chdev_minor);
    
    /* free previously allocated memory */
    cdev_del(&chdev->cdev);
    kfree(chdev->buf);
    kfree(chdev);
    
    /* remove files associated with chdev driver from /proc file system */
    chdev_remove_proc();
    
    /* cleanup_module is never called if registering failed */
    unregister_chrdev_region(devno, 1);
}

module_init(chdev_init_module);
module_exit(chdev_cleanup_module);;