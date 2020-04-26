/* Author: Matthew Boyd, U11308313 */
/* Adapted from provided nibbler and hellofb code */
/* Necessary includes for device drivers */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h> /* kmalloc() */
#include <linux/fs.h> /* everything... */
#include <linux/errno.h> /* error codes */
#include <linux/types.h> /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h> /* O_ACCMODE */
#include <asm/system_misc.h> /* cli(), *_flags */
#include <linux/uaccess.h>
#include <asm/uaccess.h> /* copy_from/to_user */
#include <linux/sched.h>
#include <linux/fb.h> /* framebuffer */
#include <linux/mutex.h>

MODULE_LICENSE("Dual BSD/GPL");

/* Framebuffer Items */
struct fb_info *info;
struct fb_fillrect *rect;
// struct fb_var_screeninfo *var;
static unsigned int black = (u32)0x00, white = (u32)0x0F; 

/* Helper function borrowed from drivers/video/fbdev/core/fbmem.c */
static struct fb_info *get_fb_info(unsigned int idx)
{
    struct fb_info *fb_info;
    if (idx >= FB_MAX)
        return ERR_PTR(-ENODEV);

    fb_info = registered_fb[idx];
    if (fb_info)
        atomic_inc(&fb_info->count);

    return fb_info;
}
static void clearScreen(void);
static void timer_set(void);

/* Timer Items */
static void timer_expire(struct timer_list *tl);
unsigned long seconds = 15;
unsigned long next_jiffies;

struct My_Timer {
    unsigned char timer_expired; /* 0=timer running, 1=timer expired (screensaver running) */
    struct timer_list timer;
};
struct My_Timer *my_timer;

/* Declaration of memory.c functions */
static int mycounter_open(struct inode *inode, struct file *filp);
static int mycounter_release(struct inode *inode, struct file *filp);
static ssize_t mycounter_read(struct file *filp,
                            char *buf, size_t count, loff_t *f_pos);
static ssize_t mycounter_write(struct file *filp,
                             const char *buf, size_t count, loff_t *f_pos);
static void mycounter_exit(void);
static int mycounter_init(void);

/* Structure that declares the usual file access functions */
struct file_operations mycounter_fops = {
read:
    mycounter_read,
write:
    mycounter_write,
open:
    mycounter_open,
release:
    mycounter_release
};

/* Declaration of the init and exit functions */
module_init(mycounter_init);
module_exit(mycounter_exit);

/* Global variables of the driver - Major number */
static int mycounter_major = 61;

/* Buffer to store data */
static char *mycounter_buffer;
unsigned char mycounter_len = 0;

// Write a screen-covering rectangle to the framebuffer (basically clearing the screen)
static void clearScreen(void) {
	if (num_registered_fb > 0) {
	    info = get_fb_info(0);
	    /* Must acquire the framebuffer lock before drawing */
	    lock_fb_info(info);
	    /* Draw the rectangle! */
	    sys_fillrect(info, rect);
	    /* Unlock mutex */
	    unlock_fb_info(info);
    } else {
    	printk(KERN_ERR "No framebuffers detected!\n");
	}
}

static void timer_expire(struct timer_list *tl)
{
    struct My_Timer *dev = from_timer(dev, tl, timer);
    // Checking timer was just running just to be safe
    if (dev->timer_expired == 0) {
    	dev->timer_expired = 1;  /* 0=timer running, 1=timer expired (screensaver running) */
        clearScreen();  /* Clear screen */
        printk(KERN_ALERT "...calling screen saver...\n");
    }
}

static void timer_set(void)
{
	// Set the timer to 15 seconds in the future
	next_jiffies = jiffies + (HZ * seconds);
	my_timer->timer_expired = 0;  /* 0=timer running, 1=timer expired (screensaver running) */
	mod_timer(&my_timer->timer, next_jiffies);  /* Set timer value */    
}

static int mycounter_init(void)
{
    int result;

    /* Registering device */
    result = register_chrdev(mycounter_major, "mycounter", &mycounter_fops);
    if (result < 0)
    {
        printk(KERN_ALERT
               "mycounter: cannot obtain major number %d\n", mycounter_major);
        return result;
    }

    /* Allocate space for My_Timer */
    my_timer = kmalloc(sizeof(struct My_Timer), GFP_KERNEL);
    if (my_timer == NULL) {
        printk(KERN_ALERT "Insufficient kernel memory for My_Timer\n");
        result = -ENOMEM;
        goto fail;
    }

    /* Allocate and setup rectangle */
    rect = kmalloc(sizeof(struct fb_fillrect), GFP_KERNEL);
    
    /* Position of rectangle in pixels, origin is top left-hand corner */
    rect->dx = 0;
    rect->dy = 0;
    /* Dimensions of the rectangle in pixels */
    rect->width = 1023;
    rect->height = 767;
    /* Color of the rectangle as a 32-bit unsigned int */
    rect->color = black;
    /* Method of drawing rectangle */
    rect->rop = ROP_COPY;

    /* Allocating mycounter for the buffer */
    mycounter_buffer = kmalloc(6, GFP_KERNEL);
    if (!mycounter_buffer)
    {
        printk(KERN_ALERT "Insufficient kernel memory for mycounter\n");
        result = -ENOMEM;
        goto fail;
    }
    memset(mycounter_buffer, 0, 4);

    //printk(KERN_ALERT "Inserting mycounter module\n");
    /* Setup timer */
	my_timer->timer_expired = 0;  /* 0=timer running, 1=timer expired (screensaver running) */
    timer_setup(&my_timer->timer, timer_expire, 0);
    // Set the timer to 15 seconds in the future
	next_jiffies = jiffies + (HZ * seconds);
    mod_timer(&my_timer->timer, next_jiffies);  /* Set timer value */
    return 0;

fail:
    mycounter_exit();
    return result;
}

static void mycounter_exit(void)
{
    /* Freeing the major number */
    unregister_chrdev(mycounter_major, "mycounter");

    /* Cleanup */
    if (rect) kfree(rect);

    /* Freeing buffer memory */
    if (mycounter_buffer)
    {
        kfree(mycounter_buffer);
    }

    del_timer_sync(&my_timer->timer);
    kfree(my_timer);

    printk(KERN_ALERT "Removing mycounter module\n");

}

static int mycounter_open(struct inode *inode, struct file *filp)
{
    /* Success */
    return 0;
}

static int mycounter_release(struct inode *inode, struct file *filp)
{    
    /* Success */
    return 0;
}

static ssize_t mycounter_read(struct file *filp,
                            char *buf, size_t count, loff_t *f_pos)
{
    // Print current values
    printk(KERN_INFO "Current Value: %ld, Timer_expired: %d \n", (next_jiffies-jiffies)/HZ, my_timer->timer_expired);
    return 0;
}

static ssize_t mycounter_write(struct file *filp, const char *buf,
                             size_t count, loff_t *f_pos)
{
    return count;
}
