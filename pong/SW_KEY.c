#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/interrupt.h>
#include "linux/address_map_arm.h"
#include "linux/interrupt_ID.h"
// Changed these
// #include "linux/address_map_arm.h"
// #include "linux/interrupt_ID.h"

/* This kernel module provides two character device drivers. One driver provides the state
 * of the KEY pushbutton switches in the DE1-SoC computer, via the file /dev/KEY. The data is
 * provided as a single digit that represents the hexadecimal valuation of the four keys. The
 * other driver provides the state of the SW slider switches, via the file /dev/SW. The data is
 * provided as three digits that represent the hexadecimal valuation of the ten switches. */

// START of declarations needed for the character device drivers
static int KEY_device_open (struct inode *, struct file *);
static int KEY_device_release (struct inode *, struct file *);
static ssize_t KEY_device_read (struct file *, char *, size_t, loff_t *);
static ssize_t KEY_device_write(struct file *, const char *, size_t, loff_t *);

static int SW_device_open (struct inode *, struct file *);
static int SW_device_release (struct inode *, struct file *);
static ssize_t SW_device_read (struct file *, char *, size_t, loff_t *);
static ssize_t SW_device_write(struct file *, const char *, size_t, loff_t *);

#define SUCCESS 0
#define KEY_DEV "KEY"
#define SW_DEV "SW"
#define KEY_BYTES 2		// number of bytes returned when read
#define SW_BYTES 4		// number of bytes returned when read
#define KEY_SIZE 3		// used for writing -- (usage) command
#define SW_SIZE 3			// used for writing -- (usage) command

static dev_t KEY_dev_no = 0;
static struct cdev *KEY_cdev = NULL;
static struct class *KEY_class = NULL;

static dev_t SW_dev_no = 0;
static struct cdev *SW_cdev = NULL;
static struct class *SW_class = NULL;

static struct file_operations KEY_fops = {
	.owner = THIS_MODULE,
	.read = KEY_device_read,
	.write = KEY_device_write,
	.open = KEY_device_open,
	.release = KEY_device_release
};

static struct file_operations SW_fops = {
	.owner = THIS_MODULE,
	.read = SW_device_read,
	.write = SW_device_write,
	.open = SW_device_open,
	.release = SW_device_release
};
// END of declarations needed for the character device drivers

// START of declarations for the KEY and SW ports
void *LW_virtual;				// used to map physical addresses for the light-weight bridge
volatile int * KEY_ptr;			// virtual pointer to KEY port
volatile int * SW_ptr;			// virtual pointer to SW port
// END of declarations for the ports

/* Code to initialize the drivers */
static int __init init_drivers(void)
{
	int err = 0;

	/* START of code to make KEY port character device driver */
	/* First, get a device number. Get one minor number (0) */
	err = alloc_chrdev_region (&KEY_dev_no, 0, 1, KEY_DEV);
	if (err < 0) {
		printk (KERN_ERR "/dev/%s: alloc_chrdev_region() failed\n", KEY_DEV);
		return err;
	}

	// Allocate and initialize the char device
	KEY_cdev = cdev_alloc ();
	KEY_cdev->ops = &KEY_fops;
	KEY_cdev->owner = THIS_MODULE;

	// Add the character device to the kernel
	err = cdev_add (KEY_cdev, KEY_dev_no, 1);
	if (err < 0) {
		printk (KERN_ERR "/dev/%s: cdev_add() failed\n", KEY_DEV);
		return err;
	}

	// Add the class to the kernel
	KEY_class = class_create (THIS_MODULE, KEY_DEV);
	device_create (KEY_class, NULL, KEY_dev_no, NULL, KEY_DEV );
	printk (KERN_INFO "KEY driver added\n");

	/* START of code to make SW port character device driver */
	/* First, get a device number. Get one minor number (0) */
	err = alloc_chrdev_region (&SW_dev_no, 0, 1, SW_DEV);
	if (err < 0) {
		printk (KERN_ERR "/dev/%s: alloc_chrdev_region() failed\n", SW_DEV);
		return err;
	}

	// Allocate and initialize the char device
	SW_cdev = cdev_alloc ();
	SW_cdev->ops = &SW_fops;
	SW_cdev->owner = THIS_MODULE;

	// Add the character device to the kernel
	err = cdev_add (SW_cdev, SW_dev_no, 1);
	if (err < 0) {
		printk (KERN_ERR "/dev/%s: cdev_add() failed\n", SW_DEV);
		return err;
	}

	// Add the class to the kernel
	SW_class = class_create (THIS_MODULE, SW_DEV);
	device_create (SW_class, NULL, SW_dev_no, NULL, SW_DEV );
	printk (KERN_INFO "SW driver added\n");
	/* END of code to make a character device driver */

	/* START of code to set up virtual port addresses */
	// generate a virtual address for the FPGA lightweight bridge
	LW_virtual = ioremap_nocache (LW_BRIDGE_BASE, LW_BRIDGE_SPAN);

	// Set up the KEY and SW ports

	// I CHANGED THIS FIRST BUT IT HAD NO EFFECT ON THE OUTCOME
	// THE SUCCESSFUL EDIT IS BELOW
	// NEW
	KEY_ptr = LW_virtual + 0x50; //TODO			// KEY port base address
	//*(KEY_ptr + 0x3) = 0xF;
	// TO PUT IT BACK TO THE ORIGINAL, REMOVE THE LINE BELOW
	// AND UNCOMMENT THE LINE ABOVE
	*KEY_ptr = *KEY_ptr;					// clear the Edgecapture register
	SW_ptr = LW_virtual + 0x40;	 //TODO		    // SW port base address

	return 0;
}

/* Remove the character device drivers */
static void __exit stop_drivers(void)
{
   iounmap (LW_virtual);
	/* Remove the devices from the kernel */
	device_destroy (KEY_class, KEY_dev_no);
	cdev_del (KEY_cdev);
	class_destroy (KEY_class);
	unregister_chrdev_region (KEY_dev_no, 1);

	device_destroy (SW_class, SW_dev_no);
	cdev_del (SW_cdev);
	class_destroy (SW_class);
	unregister_chrdev_region (SW_dev_no, 1);
}

/* Called when a process opens the KEY driver */
static int KEY_device_open(struct inode *inode, struct file *file)
{
	return SUCCESS;
}

/* Called when a process closes the KEY driver */
static int KEY_device_release(struct inode *inode, struct file *file)
{
	return 0;
}

/* Called when a process reads from the KEY driver. If *offset = 0, the contents of the KEY
 * Edgecapture register are copied back to the user, and KEY_BYTES is returned. But if
 * *offset = KEY_BYTES, nothing is copied back to the user and 0 (EOF) is returned. */
static ssize_t KEY_device_read(struct file *filp, char *buffer, size_t length, loff_t *offset)
{
	int KEY_data;
	char themessage[KEY_BYTES + 1];					// extra byte is for a NULL terminator
	size_t bytes;

	bytes = KEY_BYTES - (*offset);					// has the KEY data been sent yet?
	if (bytes)
	{
		// ***** THIS IS WHAT I CHANGED *****
		// NEW
	    KEY_data = *KEY_ptr;					// read the KEY port Edgecapture register
		*KEY_ptr = KEY_data;
		// OLD
		// KEY_data = *(KEY_ptr + 0x3);					// read the KEY port Edgecapture register
		// *(KEY_ptr + 0x3) = KEY_data;					// clear the Edgecapture register
		sprintf(themessage, "%1X\n", KEY_data);	//TODO  // put data into the message
		if (copy_to_user (buffer, themessage, bytes) != 0)
			printk (KERN_ERR "/dev/%s: copy_to_user unsuccessful\n", KEY_DEV);
	}
	(*offset) = bytes;									// keep track of number of bytes sent
	return bytes;											// returns KEY_BYTES, or EOF
}

/* Called when a process writes to the KEY driver. Only the -- usage command is supported */
static ssize_t KEY_device_write(struct file *filp, const char *buffer, size_t length,
	loff_t *offset)
{
	char KEY_string[KEY_SIZE + 1];	// extra byte is for a NULL terminator
	size_t bytes;
	bytes = length;

	if (bytes > KEY_SIZE)	// can copy all at once, or not?
		bytes = KEY_SIZE;
	if (copy_from_user (KEY_string, buffer, bytes) != 0)
		printk (KERN_ERR "/dev/%s: copy_from_user unsuccessful\n", KEY_DEV);
	KEY_string[bytes] = '\0';	// NULL terminate
	*offset += bytes;	// keep track of how much was copied

	if (KEY_string[0] == '-')
	{
		printk (KERN_ERR "Intel FPGA sample device driver\n");
		printk (KERN_ERR "Returns: H, where 0 <= H <= F\n");
	}
	return length;
}

/* Called when a process opens the SW driver */
static int SW_device_open(struct inode *inode, struct file *file)
{
	return SUCCESS;
}

/* Called when a process closes the SW driver */
static int SW_device_release(struct inode *inode, struct file *file)
{
	return 0;
}

/* Called when a process reads from the SW driver. If *offset = 0, the contents of the SW
 * data register are copied back to the user, and SW_BYTES is returned. But if
 * *offset = SW_BYTES, nothing is copied back to the user and 0 (EOF) is returned. */
static ssize_t SW_device_read(struct file *filp, char *buffer, size_t length, loff_t *offset)
{
	int SW_data;
	char themessage[SW_BYTES + 1];				// extra byte is for a NULL terminator
  	size_t bytes;

	bytes = SW_BYTES - (*offset);					// has the SW data been sent yet?
	if (bytes)
	{
		SW_data = *(SW_ptr); 							// read the SW port data register
		sprintf(themessage, "%03X\n", SW_data);	//TODO  // put data into the message
		if (copy_to_user (buffer, themessage, bytes) != 0)
			printk (KERN_ERR "/dev/%s: copy_to_user unsuccessful\n", SW_DEV);
	}
	(*offset) = bytes;								// keep track of number of bytes sent to the user
	return bytes;										// returns SW_BYTES, or EOF
}

/* Called when a process writes to the SW driver. Only the -- usage command is supported */
static ssize_t SW_device_write(struct file *filp, const char *buffer, size_t length,
	loff_t *offset)
{
	char SW_string[SW_SIZE + 1];	// extra byte is for a NULL terminator
	size_t bytes;
	bytes = length;

	if (bytes > SW_SIZE)	// can copy all at once, or not?
		bytes = SW_SIZE;
	if (copy_from_user (SW_string, buffer, bytes) != 0)
		printk (KERN_ERR "/dev/%s: copy_from_user unsuccessful\n", SW_DEV);
	SW_string[bytes] = '\0';	// NULL terminate
	*offset += bytes;	// keep track of how much was copied

	if (SW_string[0] == '-')
	{
		printk (KERN_ERR "Intel FPGA sample device driver\n");
		printk (KERN_ERR "Returns: HHH, where 0 <= H <= F\n");
	}
	return length;
}

MODULE_LICENSE("GPL");
module_init (init_drivers);
module_exit (stop_drivers);
