/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/mutex.h>
#include "aesdchar.h"
#include "aesd_ioctl.h"
#include "aesd-circular-buffer.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Corey Kelley");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
    .llseek = aesd_llseek,
    .unlocked_ioctl = aesd_unlocked_ioctl,
};

int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev *dev;

    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);

    filp->private_data = dev;
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    struct aesd_dev * dev;
    struct aesd_buffer_entry *circBufEntry = NULL;
    size_t entryOffset = 0;
    size_t *position = (size_t *)f_pos;

    PDEBUG("READ\n");
    // get the private data
    dev = (struct aesd_dev *)filp->private_data;
    
    // iterate over all the bytes reqeusted to return
    if (mutex_lock_interruptible(&dev->lock))
    {
        return -ERESTARTSYS;
    }

    // get the specified entry
    circBufEntry = aesd_circular_buffer_find_entry_offset_for_fpos(dev->circularBuffer, *position, &entryOffset);
    PDEBUG("pos: %d, offset: %d, ptr %X\n", *position, entryOffset, circBufEntry);

    if(circBufEntry)
    {

        PDEBUG("size: %d, buf: %.*s", circBufEntry->size, circBufEntry->size, circBufEntry->buffptr);

        unsigned int sizeToCopy = circBufEntry->size - entryOffset;

        sizeToCopy = min(sizeToCopy, count);

        if (copy_to_user(buf, circBufEntry->buffptr, sizeToCopy))
        {
            mutex_unlock(&dev->lock);
            return -EFAULT;
        }

        *f_pos += sizeToCopy;
        mutex_unlock(&dev->lock);
        return sizeToCopy;
    }
    
    mutex_unlock(&dev->lock);

    return 0;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = count;
    struct aesd_dev * dev;
    const char *removedEntry;
    PDEBUG("%s\n", "WRITE");

    dev = (struct aesd_dev *)filp->private_data;

    unsigned int newSize = dev->workingEntry->size + count;

    if (mutex_lock_interruptible(&dev->lock))
    {
        return -ERESTARTSYS;
    }
    
    char *newPtr = krealloc(dev->workingEntry->buffptr, newSize, GFP_KERNEL);
    if(!newPtr)
    {
        mutex_unlock(&dev->lock);
        return -ENOMEM;
    }

    dev->workingEntry->buffptr = newPtr;

    if(copy_from_user(&(dev->workingEntry->buffptr[dev->workingEntry->size]), buf, count))
    {
        mutex_unlock(&dev->lock);
        return -EFAULT;
    }

    dev->workingEntry->size = newSize;

    PDEBUG("size: %d, count: %d Buf: %.*s", dev->workingEntry->size, count, dev->workingEntry->size, dev->workingEntry->buffptr);

    // if the last byte is a newline, add it to the circular buffer
    if(dev->workingEntry->buffptr[dev->workingEntry->size-1] == '\n')
    {

        removedEntry = aesd_circular_buffer_add_entry(dev->circularBuffer, dev->workingEntry);

        if(removedEntry)
        {
            kfree(removedEntry);
        }

        dev->workingEntry->size = 0;
        dev->workingEntry->buffptr = NULL;

    }

    mutex_unlock(&dev->lock);

    return retval;
}

off_t aesd_llseek(struct file *filp, loff_t off, int whence)
{
    struct aesd_dev * dev;
    dev = (struct aesd_dev *)filp->private_data;
    
    if (mutex_lock_interruptible(&dev->lock))
    {
        return -ERESTARTSYS;
    }

    loff_t size = dev->circularBuffer->totalNumBytes;


    off_t offset = fixed_size_llseek(filp, off, whence, size);

    mutex_unlock(&dev->lock);

    return offset;
}

long aesd_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long __user arg)
{
    struct aesd_seekto *seekToStruct;

    PDEBUG("ioctl\n");

    switch(cmd)
    {
        case AESDCHAR_IOCSEEKTO:
            if (copy_from_user(seekToStruct, (void __user *)arg, sizeof(struct aesd_seekto)))
            {
                return -EFAULT;
            }

            return aesd_adjust_file_offset(filp, seekToStruct->write_cmd, seekToStruct->write_cmd_offset);
        break;
    }
}

long aesd_adjust_file_offset(struct file * filp, unsigned int write_cmd, unsigned int write_cmd_offset)
{
    struct aesd_dev * dev;
    struct aesd_buffer_entry readEntry = {0};
    struct aesd_buffer_entry *pReadEntry = &readEntry;
    unsigned int totalLength = 0;

    dev = (struct aesd_dev *)filp->private_data;

    // iterate over all the bytes requested to return
    if (mutex_lock_interruptible(&dev->lock))
    {
        return -ERESTARTSYS;
    }

    if(write_cmd < dev->circularBuffer->length)
    {
        for(unsigned int i = 0; i < write_cmd; i++)
        {
            aesd_circular_buffer_peek(dev->circularBuffer, &pReadEntry, i);

            totalLength += pReadEntry->size;
        }
        
        if(write_cmd_offset < pReadEntry->size)
        {
            totalLength += write_cmd_offset;
            filp->f_pos = totalLength;
            mutex_unlock(&dev->lock);
            return 0;
        }
    }

    mutex_unlock(&dev->lock);
    
    // if this is reached, the input was invalid
    return -EINVAL;
}

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    aesd_device.circularBuffer = (struct aesd_circular_buffer *)kmalloc(sizeof(struct aesd_circular_buffer), GFP_KERNEL);
    aesd_device.workingEntry = (struct aesd_buffer_entry *)kmalloc(sizeof(struct aesd_buffer_entry), GFP_KERNEL);

    aesd_circular_buffer_init(aesd_device.circularBuffer);
    memset(aesd_device.workingEntry,0,sizeof(struct aesd_buffer_entry));

    mutex_init(&aesd_device.lock);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    kfree(aesd_device.circularBuffer);
    kfree(aesd_device.workingEntry);

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
