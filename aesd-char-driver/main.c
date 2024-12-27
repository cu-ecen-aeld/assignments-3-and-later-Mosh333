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
#include <linux/slab.h> //for kfree and kmalloc
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Mosh333"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    // Approach 1 based on "Device Driver File Operations" lecture video
    // handles multiple device opens
    struct aesd_dev *aesd_device;
    
    PDEBUG("open");
    /**
     * TODO: handle open
     */

    aesd_device = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = aesd_device;   // way to pass context

    // Approach 2, directly assign the global var device (a single device only)
    // does not seem to work
    // filp->private_data = &aesd_device;


    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    // flexible function for handling cleanup tasks such as cleaning
    // per-file state or resources
    PDEBUG("release");
    /**
     * TODO: handle release
     */

    if (filp->private_data){
        kfree(filp->private_data);
        filp->private_data = NULL;
    }

    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry *entry;
    size_t entry_offset_byte_rtn;
    size_t bytes_to_copy;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */

    // Find the entry corresponding to the current file position
    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->aesd_circular_buffer, *f_pos, &entry_offset_byte_rtn);
    if (!entry) {
        PDEBUG("No entry found for offset %lld\n", *f_pos);
        return 0; // EOF
    }

    // Determine how many bytes to copy
    bytes_to_copy = min(count, entry->size - entry_offset_byte_rtn);

    // Copy to user space
    if (copy_to_user(buf, entry->buffptr + entry_offset_byte_rtn, bytes_to_copy)) {
        PDEBUG("Error copying data to user space\n");
        return -EFAULT;
    }

    // Update file position
    *f_pos += bytes_to_copy;
    retval = bytes_to_copy;

    PDEBUG("Read %zu bytes from entry\n", bytes_to_copy);

    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry new_entry;
    char *kern_buf = NULL;
    int i;
    struct aesd_buffer_entry *entry;
    PDEBUG("write %zu bytes with offset %lld\n",count,*f_pos);
    PDEBUG("Test1 - moshiur!!!");
    /**
     * TODO: handle write
     */

    // Allocate memory for incoming data
    kern_buf = kmalloc(count, GFP_KERNEL);
    if (!kern_buf) {
        PDEBUG("Failed to allocate memory with kmalloc\n");
        return -ENOMEM; // Return error if allocation fails
    }

    // Copy data from user space
    if (copy_from_user(kern_buf, buf, count)) {
        PDEBUG("Error copying data from user space\n");
        kfree(kern_buf); // Free memory in case of failure
        return -EFAULT;  // Return -EFAULT
    }

    // Log copied data for verification
    PDEBUG("Copied data: %.*s\n", (int)count, kern_buf);

    // Set up the new entry
    new_entry.buffptr = kern_buf;
    new_entry.size = count;

    // Add the new entry to the circular buffer
    aesd_circular_buffer_add_entry(&dev->aesd_circular_buffer, &new_entry);

    // Free allocated memory (temporary, for testing only)
    // kfree(kern_buf);

    // Log successful addition
    PDEBUG("Added entry to circular buffer: %.*s\n", (int)count, kern_buf);

    // Loop through the circular buffer and print its contents for debugging
    PDEBUG("Current buffer contents:");
    AESD_CIRCULAR_BUFFER_FOREACH(entry, &dev->aesd_circular_buffer, i) {
        if (entry->buffptr != NULL) {
            PDEBUG("Entry %d: %.*s (size: %zu)", i, (int)entry->size, entry->buffptr, entry->size);
        } else {
            PDEBUG("Entry %d: Empty", i);
        }
    }

    // Return number of bytes written
    retval = count;

    return retval;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

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

    /**
     * TODO: initialize the AESD specific portion of the device
     */

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

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
