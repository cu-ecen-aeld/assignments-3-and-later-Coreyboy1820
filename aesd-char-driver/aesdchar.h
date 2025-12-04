/*
 * aesdchar.h
 *
 *  Created on: Oct 23, 2019
 *      Author: Dan Walkes
 */

#ifndef AESD_CHAR_DRIVER_AESDCHAR_H_
#define AESD_CHAR_DRIVER_AESDCHAR_H_

#define AESD_DEBUG 1  //Remove comment on this line to enable debug

#undef PDEBUG             /* undef it, just in case */
#ifdef AESD_DEBUG
#  ifdef __KERNEL__
     /* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_DEBUG "aesdchar: " fmt, ## args)
#  else
     /* This one for user space */
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif
#else
#  define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif

struct aesd_dev
{
    /**
     * TODO: Add structure(s) and locks needed to complete assignment requirements
     */

     struct aesd_circular_buffer *circularBuffer; // the circular buffer which stores data
     struct aesd_buffer_entry *workingEntry; // the working buffer for incomplete messages
     struct mutex lock; // a mutex for accessing the circular buffer
     struct cdev cdev;     /* Char device structure      */
};

int aesd_open(struct inode *inode, struct file *filp);
int aesd_release(struct inode *inode, struct file *filp);
ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos);
ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos);
off_t aesd_llseek(struct file *filp, loff_t off, int whence);
long aesd_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
#endif /* AESD_CHAR_DRIVER_AESDCHAR_H_ */
