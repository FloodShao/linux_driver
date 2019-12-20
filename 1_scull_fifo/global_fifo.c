/*
* @Author: FloodShao
* @Date:   2019-12-19 13:59:28
* @Last Modified by:   FloodShao
* @Last Modified time: 2019-12-19 16:18:03
*/

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/uaccess.h> //copy_*_user
#include <linux/cdev.h>
#include <linux/mutex.h> //mutex
#include <linux/wait.h> //wait_queue_head_t
#include <linux/types.h> //all the ssize_t, loff_t
#include <linux/sched/signal.h>
#include <linux/slab.h> //mem manage kzalloc()

#define GLOBALMEM_SIZE		0x1000
#define FIFO_CLEAR			0x01	//ioctl cmd
#define GLOBALFIFO_MAJOR	230

static int globalfifo_major = GLOBALFIFO_MAJOR;
module_param(globalfifo_major, int, S_IRUGO); //config module args: name, type, perm

struct globalfifo_dev {
	struct cdev cdev;
	unsigned int current_len; // for counting the current fifo len
	unsigned char mem[GLOBALMEM_SIZE]; //actual mem space
	struct mutex mutex;
	wait_queue_head_t r_wait;
	wait_queue_head_t w_wait;
};

struct globalfifo_dev *globalfifo_devp;


static int globalfifo_open(struct inode *inode, struct file *filp){
	//move the globalfifo_dev to filp->private_data
	filp->private_data = globalfifo_devp;
	return 0;
}

static int globalfifo_release(struct inode *inode, struct file *filp){
	return 0;
}

static ssize_t globalfifo_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos){
	int ret;
	struct globalfifo_dev *dev = filp->private_data;
	DECLARE_WAITQUEUE(wait, current); //initialize a marco type for wait queue. check the definition. 
	// @wait is the name of the marco 
	// @current is a tsk, which holds the information for current thread
	// @current is defined in different arch, in x86, current is define as the get_current() to get the current task

	//getting the mutex
	mutex_lock(&dev->mutex); //if not getting the lock, sleep at this step and wait for the signal
	add_wait_queue(&dev->r_wait, &wait); //adding the queue elem in the wait queue

	//after getting the lock, check the resources
	while(dev->current_len == 0){ // there is no resources to read
		if(filp->f_flags & O_NONBLOCK){ // file is read as non block
			ret = -EAGAIN;
			goto out;
		}
		//block read, release the lock first
		mutex_unlock(&dev->mutex);

		__set_current_state(TASK_INTERRUPTIBLE);
		schedule(); // the thread sleeps here

		//the thread wakes up in here, 
		//can be woke up by the schedule signal(because the other thread has used up their time), but the resources still not available
		//if the thread gets running, it must check the resource again before process the reading
		if(signal_pending(current)){ //checking the flags in thread_info
			ret = -ERESTARTSYS;
			goto out2;
		}

		mutex_lock(&dev->mutex); //pair with the previous mutex_unlock.
	}

	//we have true resources
	if(size > dev->current_len){
		size = dev->current_len;
	}
	if(copy_to_user(buf, dev->mem, size)){
		ret = -EFAULT;
		goto out;
	} else{
		memcpy(dev->mem, dev->mem+size, dev->current_len - size); //fifo
		dev->current_len -= size; //update
		printk(KERN_INFO "Read %lu bytes, current_len %u\n", size, dev->current_len);

		//wake up the write queue
		wake_up_interruptible(&dev->w_wait);
		ret = size;
	}


out:
	mutex_unlock(&dev->mutex);
out2:
	remove_wait_queue(&dev->r_wait, &wait);
	set_current_state(TASK_RUNNING); //same as __set_current_state
	return ret;
}

static ssize_t globalfifo_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos){
	int ret;
	struct globalfifo_dev *dev = filp->private_data;
	DECLARE_WAITQUEUE(wait, current);

	mutex_lock(&dev->mutex);
	add_wait_queue(&dev->w_wait, &wait);

	while(dev->current_len == GLOBALMEM_SIZE){ //no resources
		if(filp->f_flags & O_NONBLOCK){
			ret = -EAGAIN;
			goto out;
		}

		//block
		mutex_unlock(&dev->mutex);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule(); // the thread sleeps here

		if(signal_pending(current)){
			ret = -ERESTARTSYS;
			goto out2;
		}

		mutex_lock(&dev->mutex);
	}

	//the resources are true
	if(size > GLOBALMEM_SIZE - dev->current_len){
		size = GLOBALMEM_SIZE - dev->current_len;
	}
	if(copy_from_user(dev->mem + dev->current_len, buf, size)){
		ret = -EFAULT;
		goto out;
	} else{
		dev->current_len += size;
		ret = size;

		wake_up_interruptible(&dev->r_wait);
		printk(KERN_INFO "Write %lu bytes, current_len %u\n", size, dev->current_len);
	}

out:
	mutex_unlock(&dev->mutex);
out2:
	remove_wait_queue(&dev->w_wait, &wait);
	set_current_state(TASK_RUNNING);
	return ret;
}

static long globalfifo_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){
	struct globalfifo_dev *dev = filp->private_data;
	switch(cmd){
	case FIFO_CLEAR:
		mutex_lock(&dev->mutex);
		dev->current_len = 0;
		memset(dev->mem, 0, GLOBALMEM_SIZE);
		mutex_unlock(&dev->mutex);

		printk(KERN_INFO "globalfifo is set to 0\n");
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct file_operations globalfifo_fops={
	.owner = THIS_MODULE,
	.read = globalfifo_read,
	.write = globalfifo_write,
	.unlocked_ioctl = globalfifo_ioctl,
	.open = globalfifo_open,
	.release = globalfifo_release,
};


static void globalfifo_setup_cdev(struct globalfifo_dev *dev, int index){
	int err;
	int devno = MKDEV(globalfifo_major, index);

	cdev_init(&dev->cdev, &globalfifo_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&dev->cdev, devno, 1); //add the chrdev in the system
	if(err)
		printk(KERN_NOTICE "ERROR: code %d, adding globalfifo %d", err, index);
}

static int __init globalfifo_init(void){
	int ret;

	//register the devno
	dev_t devno = MKDEV(globalfifo_major, 0);
	if(globalfifo_major){
		ret = register_chrdev_region(devno, 1, "globalfifo");
	} else{
		ret = alloc_chrdev_region(&devno, 0, 1, "globalfifo");
		globalfifo_major = MAJOR(devno);
	}
	if(ret < 0)
		return ret;


	//connect the devno with the driver
	//alloc the mem space for the device
	globalfifo_devp = kzalloc(sizeof(struct globalfifo_dev), GFP_KERNEL);
	if(!globalfifo_devp){
		ret = -ENOMEM;
		goto fail_malloc;
	}

	//setup for cdev
	globalfifo_setup_cdev(globalfifo_devp, 0);

	//init for the mutex and wait queue
	mutex_init(&globalfifo_devp->mutex);
	init_waitqueue_head(&globalfifo_devp->r_wait);
	init_waitqueue_head(&globalfifo_devp->w_wait);

	return 0;

fail_malloc:
	unregister_chrdev_region(devno, 1);
	return ret;
}

static void __exit globalfifo_exit(void){
	cdev_del(&globalfifo_devp->cdev);
	kfree(globalfifo_devp);
	unregister_chrdev_region(MKDEV(globalfifo_major, 0), 1);
}

module_init(globalfifo_init);
module_exit(globalfifo_exit);
MODULE_LICENSE("GPL v2");