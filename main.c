/* 
 * Copyright (C) 2014 Sergey Morozov
 * 
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/semaphore.h>
#include <linux/moduleparam.h>
#include <linux/stat.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>

#include "chdev.h"

/*
 * Declaration of functions.
 */
static int  __init     chdev_init_module(void);
static int  __init     chdev_setup(void);
static void            chdev_cleanup_module(void);
static void            chdev_setup_cdev(struct chdev_dev *dev);
static int             chdev_open(struct inode *inode, struct file *filp);
static int             chdev_release(struct inode *inode, struct file *filp);
static ssize_t         chdev_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
static ssize_t         chdev_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
static void __init     chdev_create_proc(void);
static void            chdev_remove_proc(void);
static int             chdev_proc_open(struct inode *inode, struct file *filp);
static int             chdev_proc_show(struct seq_file *s, void *v);

/*
 * Declaration of variables.
 */
static int                    chdev_major   = 0;                    /* dynamic major */
static int                    chdev_minor   = 0; 
static int __initdata         buffer        = BUF_5KB;              /* size of the chdev buffer in 5 KB by default */
static struct chdev_dev       *chdev;
static struct file_operations chdev_fops    = {
	.owner   = THIS_MODULE,
	.open    = chdev_open,
	.release = chdev_release,
	.read    = chdev_read,
	.write   = chdev_write,
	//.ioctl =    chdev_ioctl,
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
	size_t           used_space = chdev->buf_size;     /* occupied space in a chdev circular buffer */
	ssize_t          retval = 0;                       /* 0 because initially we haven't read nothing */
	short            item_len = 0;                     /* length of current item in chdev->buf, initially equals 0 */
	
	if (chdev->num_item == 0) {
/*@@@*/ printk(KERN_DEBUG "case 0\n");
		return 0; /* there is nothing to read from buffer */
	}
	
	if (down_interruptible(&dev->sem)) {
		return -ERESTARTSYS;
	}
	
/*@@@*/	printk(KERN_DEBUG "\nENTER in chdev_read\n"); 	

/*@@@*/	printk(KERN_DEBUG "__user *buf size == %d", (int)count); 	
	
	/* calculation of used space in a buffer, and reading data */	
#define USED_SPACE                       (chdev->end - chdev->beg)
#define USED_SPACE_INV                   (chdev->buf_size - (chdev->end - chdev->beg))
#define USED_SPACE_UPSIDE                (chdev->end - chdev->buf)
#define USED_SPACE_DOWNSIDE              (chdev->buf + chdev->buf_size - chdev->beg)

/*@@@*/	printk(KERN_DEBUG "USED_SPACE == %d\n", (int)USED_SPACE);  
/*@@@*/	printk(KERN_DEBUG "USED_SPACE_INV == %d\n", (int)USED_SPACE_INV);  
/*@@@*/	printk(KERN_DEBUG "USED_SPACE_UPSIDE == %d\n", (int)USED_SPACE_UPSIDE);  
/*@@@*/	printk(KERN_DEBUG "USED_SPACE_DOWNSIDE == %d\n", (int)USED_SPACE_DOWNSIDE); 

        /* calculation of a used space in a buffer, and writing data */
	used_space = chdev->inv ? USED_SPACE_INV : USED_SPACE;	
	
	/* read item length */
	if (chdev->inv) {
/*@@@*/ printk(KERN_DEBUG "case 1\n");	  
		if (USED_SPACE_DOWNSIDE <= sizeof(short)) {
			memcpy((char *)&item_len, chdev->beg, USED_SPACE_DOWNSIDE); /* bytes downside the buffer */
			memcpy(((char *)&item_len) + USED_SPACE_DOWNSIDE, chdev->buf, sizeof(short) - USED_SPACE_DOWNSIDE); /* bytes upside the buffer */
			if (copy_to_user(buf, chdev->buf + sizeof(short) - USED_SPACE_DOWNSIDE, item_len)) {
/*@@@*/ printk(KERN_DEBUG "case 1.1:unsuccessful\n");	  			  
				retval = -EFAULT;
				goto out;
			}
			chdev->beg = chdev->buf + sizeof(short) - USED_SPACE_DOWNSIDE + item_len;  /* item_len + sizeof(short) bytes were */
			                                                                           /* totally read from buffer            */
			chdev->inv = false;      /* chdev->beg and chdev-> end are not inversed now */
/*@@@*/ printk(KERN_DEBUG "case 1.1:successful\n");	  			  			
		} 
		else {
			memcpy((char *)&item_len, chdev->beg, sizeof(short));
			if (USED_SPACE_DOWNSIDE > (item_len + sizeof(short))) {
				if (copy_to_user(buf, chdev->beg + sizeof(short), item_len)) {
/*@@@*/ printk(KERN_DEBUG "case 1.2.1:unsuccessful\n");	  			  				  
					retval = -EFAULT;
					goto out;
				}
				chdev->beg += (size_t)item_len + sizeof(short); /* item_len + sizeof(short) bytes were totally read from buffer */
				                                                /* chdev->beg and chdev->end are still inversed */
/*@@@*/ printk(KERN_DEBUG "case 1.2.1:successful\n");	  
			}
			else {
				if (copy_to_user(buf, chdev->beg + sizeof(short), USED_SPACE_DOWNSIDE - sizeof(short))) {
/*@@@*/ printk(KERN_DEBUG "case 1.2.2:unsuccessful\n");	  
					retval = -EFAULT;
					goto out;
				}
				if (copy_to_user(buf + USED_SPACE_DOWNSIDE - sizeof(short), 
				                 chdev->buf, 
		                                 (size_t)item_len - USED_SPACE_DOWNSIDE + sizeof(short))) {
/*@@@*/ printk(KERN_DEBUG "case 1.2.2:unsuccessful\n");	  
					retval = -EFAULT;
					goto out;
				}
				chdev->beg = chdev->buf + (size_t)item_len - USED_SPACE_DOWNSIDE + sizeof(short); /* item_len + sizeof(short) bytes */
				                                                                                  /* were totally read from buffer  */
				chdev->inv = false;   /* chdev->beg and chdev-> end are not inversed now */
/*@@@*/ printk(KERN_DEBUG "case 1.2.2:successful\n");	  
			}
		}
	}
	else {
		memcpy((char *)&item_len, chdev->beg, sizeof(short));
		if (copy_to_user(buf, chdev->beg + sizeof(short), item_len)) {
/*@@@*/ printk(KERN_DEBUG "case 2:unsuccessful\n");	  
			retval = -EFAULT;
			goto out;
		}
		chdev->beg += item_len + sizeof(short);
/*@@@*/ printk(KERN_DEBUG "case 2:successful\n");	  
	}
	retval = item_len;
	--chdev->num_item; /* one item was read from chdev->buf */

	/* reset beg and end pointers to default in num_item == 0 */
	if (chdev->num_item == 0) {
		chdev->beg = chdev->buf;
		chdev->end = chdev->buf;
		/* chdev->inv is already equals false */
	}
	  
	
#undef  USED_SPACE
#undef  USED_SPACE_INV
#undef  USED_SPACE_UPSIDE
#undef  USED_SPACE_DOWNSIDE

/*@@@*/	printk(KERN_DEBUG "\nEXIT from chdev_read\n"); 	

  out:
	up(&dev->sem);
	return retval;
}

/*
 * Implementation of file_operations.write for chdev_fops.
 */
static ssize_t chdev_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {
	struct chdev_dev *dev = filp->private_data;
	size_t           free_space = 0;            /* free space in a chdev circular buffer */
	ssize_t          retval     = -ENOMEM;      /* -ENOMEM because free_space == 0 by default */
	short            item_len   = (short)count; /* length of input data */
	
	if (down_interruptible(&dev->sem)) {
		return -ERESTARTSYS;
	}
	
/*@@@*/	printk(KERN_DEBUG "\nENTER in chdev_write\n"); 	

/*@@@*/	printk(KERN_DEBUG "item_len == %d", (int)item_len); 
        	
#define FREE_SPACE                       (chdev->buf_size - (chdev->end - chdev->beg))
#define FREE_SPACE_INV                   (chdev->beg - chdev->end)
#define FREE_SPACE_UPSIDE                (chdev->beg - chdev->buf)
#define FREE_SPACE_DOWNSIDE              (chdev->buf + chdev->buf_size - chdev->end)

/*@@@*/	printk(KERN_DEBUG "FREE_SPACE == %d\n", (int)FREE_SPACE);  
/*@@@*/	printk(KERN_DEBUG "FREE_SPACE_INV == %d\n", (int)FREE_SPACE_INV);  
/*@@@*/	printk(KERN_DEBUG "FREE_SPACE_UPSIDE == %d\n", (int)FREE_SPACE_UPSIDE);  
/*@@@*/	printk(KERN_DEBUG "FREE_SPACE_DOWNSIDE == %d\n", (int)FREE_SPACE_DOWNSIDE); 
	
	/* calculation of a free space in a buffer, and writing data */
	free_space = chdev->inv ? FREE_SPACE_INV : FREE_SPACE;
	
	/* is item length greater than free space in the buffer */
	if (count + sizeof(short) > free_space) {
		retval = -ENOMEM;
		goto out;
	}
	
	/* determine the way of storing data in the buffer (depends on chdev->inv value) */    
	if (chdev->inv) {
		if (copy_from_user(chdev->end, &item_len, sizeof(short)) || 
		    copy_from_user(chdev->end + sizeof(short), buf, count)) {
/*@@@*/	printk(KERN_DEBUG "if stmt: copy_from_user error, chdev->inv == true, retval = -EFAULT\n");
			retval = -EFAULT;
			goto out; 
		}
		chdev->end += count + sizeof(short); /* update end of buffer position */
	}
	else {
		/* TESTED */
		if (count <= FREE_SPACE_DOWNSIDE) {
			memcpy(chdev->end, (const char *)&item_len, sizeof(short));			    
			if (copy_from_user(chdev->end + sizeof(short), buf, count)) {
/*@@@*/	printk(KERN_DEBUG "if stmt: copy_from_user error, chdev->inv == false, count <= FREE_SPACE_DOWNSIDE, retval = -EFAULT\n");
				retval = -EFAULT;
				goto out; 
			}
			chdev->end += count + sizeof(short); /* update end of buffer position */
		}
		else {
			if (sizeof(short) >= FREE_SPACE_DOWNSIDE) {
				if (copy_from_user(chdev->end, &item_len, FREE_SPACE_DOWNSIDE)) {
/*@@@*/	printk(KERN_DEBUG "if stmt: copy_from_user error, chdev->inv == false, count < FREE_SPACE_DOWNSIDE, sizeof(short) > FREE_SPACE_DOWNSIDE, retval = -EFAULT\n");
					retval = -EFAULT;
					goto out; 
				}
				if (copy_from_user(chdev->buf, 
				                   &item_len + FREE_SPACE_DOWNSIDE, 
		                                   sizeof(short) - FREE_SPACE_DOWNSIDE) || 
				    copy_from_user(chdev->buf + sizeof(short) - FREE_SPACE_DOWNSIDE, buf, count)) {
/*@@@*/	printk(KERN_DEBUG "if stmt: copy_from_user error, chdev->inv == false, count < FREE_SPACE_DOWNSIDE, sizeof(short) >= FREE_SPACE_DOWNSIDE, retval = -EFAULT\n");
					retval = -EFAULT;
					goto out; 
				}
				chdev->inv = true; /* positions of chdev->beg and chdev->end are now inversed */
				chdev->end = chdev->buf + sizeof(short) - FREE_SPACE_DOWNSIDE + count; /* update end of buffer position */
			}
			else {
				if (copy_from_user(chdev->end, &item_len, sizeof(short)) || 
				    copy_from_user(chdev->end + sizeof(short), buf, FREE_SPACE_DOWNSIDE - sizeof(short))) {
					retval = -EFAULT;
					goto out; 
				}
				if (copy_from_user(chdev->buf, 
				                   buf + FREE_SPACE_DOWNSIDE - sizeof(short), 
						   count - FREE_SPACE_DOWNSIDE + sizeof(short))) {
					memset(chdev->end, 0, FREE_SPACE_DOWNSIDE); /* fill corrupted memory with zeros */
					retval = -EFAULT;
					goto out;
				}
				chdev->inv = true;                            /* data was splitted */
				chdev->end = chdev->buf + count - FREE_SPACE; /* update end of buffer position */			
			}
		}
	}
	retval = item_len;
	++chdev->num_item; /* new item was added to chdev->buf */
	
/*@@@*/	printk(KERN_DEBUG "\nEXIT from chdev_write\n"); 		
	
#undef  FREE_SPACE
#undef  FREE_SPACE_INV
#undef  FREE_SPACE_UPSIDE
#undef  FREE_SPACE_DOWNSIDE

  out:
	up(&dev->sem);
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