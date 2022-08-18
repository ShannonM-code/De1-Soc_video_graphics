#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include "../address_map_arm.h"

/* This kernel module provides a character device driver that controls a video display.
 * The user-interface to the driver is through the file /dev/video. The following commands
 * can be written to the driver through this file: "clear" and "pixel x,y color".
 * Reading from the /dev/video produces the string "screen_x screen_y", where these are
 * the number of columns and rows on the screen, respectively. */

/* Prototypes for functions used for video display */
void get_screen_specs (volatile int *);
void clear_screen (void);
void plot_pixel(int, int, short int);
void usage(void);
void draw_line(int, int, int, int, int);
int ABS(int);
void vsync(volatile int*);
void box(int, int, int, int, int);

// START of declarations needed for a character device
static int device_open (struct inode *, struct file *);
static int device_release (struct inode *, struct file *);
static ssize_t device_read (struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);

#define SUCCESS 0
#define DEV_NAME "video"
#define VIDEO_BYTES 8					// number of bytes to return when read (includes a \n)
#define VIDEO_SIZE 18					// max number of bytes accepted for a write

static dev_t dev_no = 0;
static struct cdev *video_cdev = NULL;
static struct class *video_class = NULL;

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = device_read,
	.write = device_write,
	.open = device_open,
	.release = device_release
};
// END of declarations needed for a character device

// START of declarations for the video
void *LW_virtual;             	// used to map physical addresses for the light-weight bridge
volatile int * pixel_ctrl_ptr;	// virtual address of pixel buffer DMA controller
int pixel_buffer, back_buffer, SDRAM_virtual, FPGA_ONCHIP_virtual;
int back_buffer;				// current location of pixel buffer memory
int resolution_x, resolution_y;	// VGA screen size
// END of declarations

/* Code to initialize the video driver */
static int __init start_video(void)
{
	int err;
	/* START of code to make a character device driver */
	/* First, get a device number. Get one minor number (0) */
	err = alloc_chrdev_region (&dev_no, 0, 1, DEV_NAME);
	if (err < 0) {
		printk (KERN_ERR "/dev/%s: alloc_chrdev_region() failed\n", DEV_NAME);
		return err;
	}
	video_class = class_create (THIS_MODULE, DEV_NAME);

	// Allocate and initialize the char device
	video_cdev = cdev_alloc ();
	video_cdev->ops = &fops;
	video_cdev->owner = THIS_MODULE;

	// Add the character device to the kernel
	err = cdev_add (video_cdev, dev_no, 1);
	if (err < 0) {
		printk (KERN_ERR "/dev/%s: cdev_add() failed\n", DEV_NAME);
		return err;
	}

	// Add the class to the kernel
	device_create (video_class, NULL, dev_no, NULL, DEV_NAME );
	/* END of code to make a character device driver */

   // generate a virtual address for the FPGA lightweight bridge
   LW_virtual = ioremap_nocache (LW_BRIDGE_BASE, LW_BRIDGE_SPAN);
	if (LW_virtual == 0)
		printk (KERN_ERR "/dev/%s: ioremap_nocache returned NULL\n", DEV_NAME);

	// Create virtual memory access to the pixel buffer controller
	pixel_ctrl_ptr = (unsigned int *) (LW_virtual + PIXEL_BUF_CTRL_BASE);
	get_screen_specs (pixel_ctrl_ptr);				// determine X, Y screen size
	//*(pixel_ctrl_ptr + 1) = FPGA_ONCHIP_BASE;
	vsync(pixel_ctrl_ptr);

	// Create virtual memory access to the pixel buffer
	pixel_buffer = (int) ioremap_nocache (FPGA_ONCHIP_BASE, FPGA_ONCHIP_SPAN);
	if (pixel_buffer == 0)
		printk (KERN_ERR "/dev/%s: ioremap_nocache returned NULL\n", DEV_NAME);

	/* Erase the pixel buffer */
	clear_screen ( );
	return 0;
}

/* Function to find the VGA screen specifications */
void get_screen_specs(volatile int * pixel_ctrl_ptr)
{
	int resolution_reg;

	resolution_reg = *(pixel_ctrl_ptr + 2);
	resolution_x = resolution_reg & 0xFFFF;
	resolution_y = resolution_reg >> 16;
}

/* Function to clear the pixel buffer */
void clear_screen( )
{
	int y, x;

	for (x = 0; x < resolution_x; x++)
		for (y = 0; y < resolution_y; y++)
			plot_pixel (x, y, 0);
}

void plot_pixel(int x, int y, short int color)
{
	*(short int *)(pixel_buffer + (y << 10) + (x << 1)) = color;
}

/* Remove the character device driver */
static void __exit stop_video(void)
{
	/* unmap the physical-to-virtual mappings */
   iounmap (LW_virtual);
	iounmap ((void *) pixel_buffer);

	/* Remove the device from the kernel */
	device_destroy (video_class, dev_no);
	cdev_del (video_cdev);
	class_destroy (video_class);
	unregister_chrdev_region (dev_no, 1);
}

/* Called when a process opens the video */
static int device_open(struct inode *inode, struct file *file)
{
	return SUCCESS;
}

/* Called when a process closes the video */
static int device_release(struct inode *inode, struct file *file)
{
	return 0;
}

/* Called when a process reads from the video. If *offset = 0, the current value of
 * the video is copied back to the user, and VIDEO_BYTES is returned. But if
 * *offset = VIDEO_BYTES, nothing is copied back to the user and 0 (EOF) is returned. */
static ssize_t device_read(struct file *filp, char *buffer, size_t length, loff_t *offset)
{
	char themessage[VIDEO_BYTES + 1];			// extra byte is for a NULL terminator
  	size_t bytes;

	bytes = VIDEO_BYTES - (*offset);				// has the video data been sent yet?
	if (bytes)
	{
		sprintf(themessage, "%03d %03d\n", resolution_x, resolution_y);
		if (copy_to_user (buffer, themessage, bytes) != 0)
			printk (KERN_ERR "/dev/%s: copy_to_user unsuccessful\n", DEV_NAME);
	}
	(*offset) = bytes;								// keep track of number of bytes sent to the user
	return bytes;										// returns VIDEO_BYTES, or EOF
}

void usage( )
{
	printk (KERN_ERR "Usage: --\n");
	printk (KERN_ERR "Usage: clear\n");
	printk (KERN_ERR "       pixel X,Y color\n");
	printk (KERN_ERR "Notes: X,Y are integers, color is 4-digit hex value\n");
}

/* Called when a process writes to the video */
static ssize_t device_write(struct file *filp, const char *buffer, size_t length, loff_t *offset)
{
	char video_string[VIDEO_SIZE + 1]; 			// extra byte is for a NULL terminator
	char command[8];
	int bytes;
	int x, y, x2, y2, color;

	bytes = length;
	if (bytes > VIDEO_SIZE)						// can copy all at once, or not?
		bytes = VIDEO_SIZE;
	if (copy_from_user (video_string, buffer, bytes) != 0)
		printk (KERN_ERR "/dev/%s: copy_from_user unsuccessful\n", DEV_NAME);

	video_string[bytes] = '\0';					// NULL terminate
	if (video_string[0] == '-' && video_string[1] == '-')
	{
		usage ( );
	}
	else if (video_string[0] == 'c' && video_string[1] == 'l')
	{
		clear_screen ( );
	}
	else if (video_string[0] == 'p' && video_string[1] == 'i' && video_string[2] == 'x' && video_string[3] == 'e' && video_string[4] == 'l')
	{
		if (sscanf (video_string, "%s %d,%d %X", command, &x, &y, &color) != 4)
		{
			printk (KERN_ERR "/dev/%s: can't parse user input: %s\n", DEV_NAME, video_string);
		}
		if (strcmp (command, "pixel") == 0)
		{
			if ((x < 0) || (x > resolution_x - 1) || (y < 0) || (y > resolution_y - 1))
			{
				printk (KERN_ERR "/dev/%s: bad coordinates: %d,%d\n", DEV_NAME, x, y);
				usage ( );
			}
			else
				plot_pixel (x, y, color);
		}
		else
		{
			printk (KERN_ERR "/dev/%s: bad argument: %s\n", DEV_NAME, command);
			usage ( );
		}
	}
    else if (video_string[0] == 'l' && video_string[1] == 'i' && video_string[2] == 'n' && video_string[3] == 'e'){
        if (sscanf (video_string, "%s %d,%d %d,%d %X", command, &x, &y, &x2, &y2, &color) != 6)
		{
			printk (KERN_ERR "/dev/%s: can't parse user input: %s\n", DEV_NAME, video_string);
		}
		if (strcmp (command, "line") == 0)
		{
			if ((x < 0) || (x > resolution_x - 1) || (y < 0) || (y > resolution_y - 1) || (x2 < 0) || (x2 > resolution_x - 1) || (y2 < 0) || (y2 > resolution_y - 1))
			{
				printk (KERN_ERR "/dev/%s: bad coordinates: %d,%d %d,%d\n", DEV_NAME, x, y, x2, y2);
				usage ( );
			}
			else{
				draw_line(x, y, x2, y2, color);
			}
		}
		else
		{
			printk (KERN_ERR "/dev/%s: bad argument: %s\n", DEV_NAME, command);
			usage ( );
		}
    }
    else if (video_string[0] == 's' && video_string[1] == 'y' && video_string[2] == 'n' && video_string[3] == 'c'){
        if (strcmp (command, "sync") == 0){
                vsync(pixel_ctrl_ptr);
		}
		else
		{
			printk (KERN_ERR "/dev/%s: bad argument: %s\n", DEV_NAME, command);
			usage ( );
		}
    }
    else if (video_string[0] == 'b' && video_string[1] == 'o' && video_string[2] == 'x'){
        if (sscanf (video_string, "%s %d,%d %d,%d %X", command, &x, &y, &x2, &y2, &color) != 6)
        {
            printk (KERN_ERR "/dev/%s: can't parse user input: %s\n", DEV_NAME, video_string);
        }
        if (strcmp (command, "box") == 0)
        {
            if ((x < 0) || (x > resolution_x - 1) || (y < 0) || (y > resolution_y - 1) || (x2 < 0) || (x2 > resolution_x - 1) || (y2 < 0) || (y2 > resolution_y - 1))
            {
				printk (KERN_ERR "/dev/%s: bad coordinates: %d,%d %d,%d\n", DEV_NAME, x, y, x2, y2);
				usage ( );
            }
            else{
				box(x, y, x2, y2, color);
            }
		}
    }

	return length;
}

void draw_line(int x0, int y0, int x1, int y1, int c){
    int delta_x = 0, delta_y = 0;
    int temp1 = 0, temp2 = 0;
    int error = 0;
    int x = 0, y = 0, y_step = 0;
    int is_steep = -1;
    int i = 0;

    if(ABS(y1 - y0) > ABS(x1 - x0))
        is_steep = 1;
    else
        is_steep = -1;

    if(is_steep == 1){
        // Swap the values
        temp1 = y0;
        temp2 = y1;
        y0 = x0;
        x0 = temp1;
        y1 = x1;
        x1 = temp2;
    }
    if(x0 > x1){
        // Swap the values
        temp1 = x1;
        temp2 = y1;
        x1 = x0;
        x0 = temp1;
        y1 = y0;
        y0 = temp2;
    }

    delta_x = x1 - x0;
    delta_y = ABS(y1-y0);
    error = -(delta_x / 2);
    y = y0;
    x = x0;

    if(y0 < y1)
        y_step = 1;
    else
        y_step = -1;

    for(i = x0; i < x1; i++){
        if(is_steep == 1)
            plot_pixel(y, i, c);
        else
            plot_pixel(i, y, c);

        error += delta_y;

        if(error >= 0){
            y += y_step;
            error -= delta_x;
        }
    }
}

int ABS(int num){
    if(num < 0)
        return -1 * num;
    else
        return num;

}

void vsync(volatile int * pixel_ctrl_ptr){

    volatile int status;
    /* Synchronizing with the VGA controller can be accomplished by writing the value 1 into
    the Buffer register in the pixel buffer controller, and then waiting until bit S of the
    Status register becomes equal to 0. */
    // Write value 1 into the pixel buffer control
    *pixel_ctrl_ptr = 1;

    // Set the status register to a value
    status = *(pixel_ctrl_ptr + 3);

    // Wait until the status is changed back to 0
    while((status & 0x01) != 0){
        status = *(pixel_ctrl_ptr + 3);
    }

    if(*(pixel_ctrl_ptr + 1) == SDRAM_BASE)
        back_buffer = (int) SDRAM_virtual;
    else
        back_buffer = (int) FPGA_ONCHIP_virtual;

}

void box(int x1, int y1, int x2, int y2, int color){
    int i, j;

    for(i = 0; i < ABS(x2 - x1); i++){
        for(j = 0; j < ABS(y2-y1); j++){
            plot_pixel(i, j, color);
        }
    }
}


MODULE_LICENSE("GPL");
module_init (start_video);
module_exit (stop_video);
