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
#include "aesd_ioctl.h"
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

    //current implementation does not require additional cleanup, leaving as no-op

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
    // PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */

    // Lock the mutex to ensure safe access to the circular buffer
    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    // Find the entry corresponding to the current file position
    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->aesd_circular_buffer, *f_pos, &entry_offset_byte_rtn);
    if (!entry) {
        PDEBUG("No entry found for offset %lld\n", *f_pos);
        retval = 0; // EOF
        goto unlock_and_return;
    }

    // Determine how many bytes to copy
    bytes_to_copy = min(count, entry->size - entry_offset_byte_rtn);

    // Copy to user space
    if (copy_to_user(buf, entry->buffptr + entry_offset_byte_rtn, bytes_to_copy)) {
        PDEBUG("Error copying data to user space\n");
        retval = -EFAULT;
        goto unlock_and_return;
    }

    // Update file position
    *f_pos += bytes_to_copy;
    retval = bytes_to_copy;

    // PDEBUG("Read %zu bytes from entry (offset %zu, size %zu)", 
    //        bytes_to_copy, 
    //        entry_offset_byte_rtn, 
    //        entry->size);

unlock_and_return:
    mutex_unlock(&dev->lock); // Unlock the mutex
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM; // Default return value for allocation failure
    struct aesd_dev *dev = filp->private_data; // Get the device structure
    char *new_buffer = NULL; // Temporary buffer for reallocations
    const char *newline_pos = NULL; // Pointer to newline character in the input buffer
    //PDEBUG("write %zu bytes with offset %lld\n",count,*f_pos);
    //PDEBUG("Test #X");
    /**
     * TODO: handle write
     */

    //PDEBUG("write %zu bytes with offset %lld", count, *f_pos);

    // Lock the mutex to protect shared resources
    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    // Append New Data to the Partial Buffer
    new_buffer = krealloc(dev->partial_write_buffer, dev->partial_write_size + count, GFP_KERNEL);
    if (!new_buffer) {
        retval = -ENOMEM;
        goto unlock_and_return;
    }
    dev->partial_write_buffer = new_buffer;

    // Copy data from user space into the partial buffer
    if (copy_from_user(dev->partial_write_buffer + dev->partial_write_size, buf, count)) {
        retval = -EFAULT;
        goto unlock_and_return;
    }
    dev->partial_write_size += count;

    // Check for Newline in the Write
    newline_pos = memchr(dev->partial_write_buffer, '\n', dev->partial_write_size);
    if (newline_pos) {
        // The write operation ends with a newline (take advantage of the newline accumulation requirement)
        size_t message_size = dev->partial_write_size; // Entire buffer size

        // Allocate memory for the new circular buffer entry
        struct aesd_buffer_entry new_entry;
        new_entry.buffptr = dev->partial_write_buffer;
        new_entry.size = message_size;

        // Add the entry to the circular buffer
        aesd_circular_buffer_add_entry(&dev->aesd_circular_buffer, &new_entry);

        // Reset the partial write buffer
        dev->partial_write_buffer = NULL;
        dev->partial_write_size = 0;
    }

    retval = count; // Return the number of bytes written

unlock_and_return:
    mutex_unlock(&dev->lock); // Unlock the mutex
    return retval;
}

// function signature borrowed from https://docs.kernel.org/filesystems/vfs.html
loff_t aesd_llseek(struct file *filp, loff_t offset, int whence){
    
    struct aesd_dev *dev = filp->private_data; // Get the device structure
    loff_t result;
    size_t total_size = 0;
    int index;
    struct aesd_buffer_entry *entryptr;

    PDEBUG("llseek");

    // Acquire the mutex lock
    if (mutex_lock_interruptible(&dev->lock)) {
        return -ERESTARTSYS;
    }

    // Compute the total size of the circular buffer
    AESD_CIRCULAR_BUFFER_FOREACH(entryptr, &dev->aesd_circular_buffer, index)
    {
        if (entryptr->buffptr != NULL) // Only consider valid entries
        {
            total_size += entryptr->size; // Add the size of the entry
        }
    }

    // Use fixed_size_llseek to calculate the new position - option 2 from the Assignment 9 Overview Video
    // "Add your own llseek function, with locking and logging, but use fixed_size_llseek for logic."
    result = fixed_size_llseek(filp, offset, whence, total_size);

    // Log the result
    PDEBUG("llseek: offset=%lld whence=%d result=%lld\n", offset, whence, result);

    // Release the mutex lock
    mutex_unlock(&dev->lock);

    return result;
}

long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){
    struct aesd_dev *dev = filp->private_data; // Access device structure
    struct aesd_seekto seekto; //the type of seek to be performed on the aesdchar driver from the userspace (via IOCTL)
    long result = 0;
    loff_t new_pos = 0;
    size_t cmd_offset = 0;
    size_t i = 0;
    struct aesd_buffer_entry *entry;

    // check if the cmd argument passed is valid, _IOC commands defined in 
    if (_IOC_TYPE(cmd) != AESD_IOC_MAGIC || _IOC_NR(cmd) > AESDCHAR_IOC_MAXNR){
        return -ENOTTY;
    }
    
    if (copy_from_user(&seekto, (void __user *)arg, sizeof(seekto))){
        return -EFAULT;
    }

    if (mutex_lock_interruptible(&dev->lock)){
        return -ERESTARTSYS;
    }

    // Validate `write_cmd` within bounds
    if (seekto.write_cmd >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED ||
        seekto.write_cmd >= (dev->aesd_circular_buffer.full ? 
            AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED : dev->aesd_circular_buffer.in_offs)) {
        result = -EINVAL;
        goto unlock_and_return;
    }

    // Get the circular buffer entry for `write_cmd`
    entry = &dev->aesd_circular_buffer.entry[seekto.write_cmd];
    if (!entry->buffptr || seekto.write_cmd_offset >= entry->size) {
        result = -EINVAL;
        goto unlock_and_return;
    }

    // Calculate the new file position
    for (i = 0; i < seekto.write_cmd; ++i) {
        cmd_offset += dev->aesd_circular_buffer.entry[i].size;
    }
    new_pos = cmd_offset + seekto.write_cmd_offset;

    // Update the file pointer
    filp->f_pos = new_pos;

unlock_and_return:
    mutex_unlock(&dev->lock);  // Release the lock
    return result;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
    .llseek =   aesd_llseek,
    .unlocked_ioctl = aesd_ioctl,
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
    aesd_device.partial_write_buffer = NULL;
    aesd_device.partial_write_size = 0;
    
    // Initialize the mutex lock
    mutex_init(&aesd_device.lock);

    // Initialize the circular buffer
    aesd_circular_buffer_init(&aesd_device.aesd_circular_buffer);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    struct aesd_buffer_entry *entry;
    int index;
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */

    // Free all entries in the circular buffer, defined in aesd-circular-buffer.h
    AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.aesd_circular_buffer, index) {
        if (entry->buffptr) {
            PDEBUG("Freeing buffer entry at index %u", index);
            kfree(entry->buffptr);  // Free allocated memory for each buffer entry
            entry->buffptr = NULL;  // Nullify pointer to avoid dangling references
        }
    }

    //Free the partial write buffer if it exists
    if (aesd_device.partial_write_buffer) {
        PDEBUG("Freeing partial write buffer");
        kfree(aesd_device.partial_write_buffer);
        aesd_device.partial_write_buffer = NULL;  // Nullify pointer
    }

    // Destroy the mutex
    mutex_destroy(&aesd_device.lock);
    
    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
