/*
* @Author: FloodShao
* @Date:   2020-01-07 09:17:59
* @Last Modified by:   FloodShao
* @Last Modified time: 2020-01-07 14:17:25
*/

#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
//#include <linux/timer.h> //timer related functions
#include <linux/init.h>
#include <linux/slab.h> //mem alloc
#include <linux/fs.h>
//#include <linux/mm.h> //unuse


#define SECOND_MAJOR 248

static int second_major = SECOND_MAJOR;
module_param(second_major, int, S_IRUGO); //pass the cmd param to module

struct second_cdev{
	struct cdev cdev;
	atomic_t counter; //indicate the counted seconds
	struct timer_list s_timer; //define a timer example
};

static struct second_cdev *second_cdevp;

/***************functions*********************/

/****************irp_handler******************/
//reset the timer expire, and increment the counter
static void second_timer_handler(struct timer_list *timer_list){ //look at kernel-4.15.0

	mod_timer(&second_cdevp->s_timer, jiffies + HZ);
	atomic_inc(&second_cdevp->counter);

	printk(KERN_INFO "current jiffies is %ld\n", jiffies);
}

/*****************drivers functions *********/
static int second_open(struct inode *inode, struct file *filp){
	//init_timer(&second_cdevp->s_timer);
	timer_setup(&second_cdevp->s_timer, &second_timer_handler, 0); //v4.15
	second_cdevp->s_timer.function = &second_timer_handler;
	second_cdevp->s_timer.expires = jiffies + HZ; //jiffies = tickes, HZ = cpu frequency

	add_timer(&second_cdevp->s_timer);
	atomic_set(&second_cdevp->counter, 0);

	return 0;
}

static int second_release(struct inode *inode, struct file *filp){
	del_timer(&second_cdevp->s_timer);

	return 0;
}


static ssize_t second_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos){
	int counter;
	counter = atomic_read(&second_cdevp->counter);
	if(put_user(counter, (int*) buf)){
		return -EFAULT;
	} else{
		return sizeof(unsigned int);
	}
}


static const struct file_operations second_fops = {
	.owner = THIS_MODULE,
	.read = second_read,
	.open = second_open,
	.release = second_release,
};


/************init the drivers********************/
static void second_setup_cdev(struct second_cdev *dev, int index){
	int err, devno = MKDEV(second_major, index);

	cdev_init(&dev->cdev, &second_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&dev->cdev, devno, 1);
	if(err){
		printk(KERN_ERR "Failed to add second device\n");
	}
}

static int __init second_init(void){
	int ret;
	int devno = MKDEV(second_major, 0);

	if(second_major){
		ret = register_chrdev_region(devno, 1, "second");
	} else{
		ret = alloc_chrdev_region(&devno, 0, 1, "second");
		second_major = MAJOR(devno);
	}

	if(ret < 0){
		return ret;
	}

	second_cdevp = kzalloc(sizeof(*second_cdevp), GFP_KERNEL);

	if(!second_cdevp){
		ret = -ENOMEM;
		goto fail_malloc;
	}

	second_setup_cdev(second_cdevp, 0);
	return 0;

fail_malloc:
	unregister_chrdev_region(devno, 1);
	return ret;
}
module_init(second_init);

static void __exit second_exit(void){
	cdev_del(&second_cdevp->cdev);
	kfree(second_cdevp);
	unregister_chrdev_region(MKDEV(second_major, 0), 1);
}
module_exit(second_exit);

MODULE_LICENSE("GPL v2");