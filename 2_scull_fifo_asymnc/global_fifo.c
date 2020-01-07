/*
* @Author: FloodShao
* @Date:   2020-01-06 09:35:25
* @Last Modified by:   FloodShao
* @Last Modified time: 2020-01-06 15:03:58
*/

#include <linux/module.h>
#include <linux/types.h>
#include <linux/sched/signal.h> //system schedual
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/slab.h> //kzalloc
#include <linux/poll.h>



#define GLOBALFIFO_SIZE 0x1000
#define FIFO_CLEAR 0x01
#define GLOBALFIFO_MAJOR 231

static int globalfifo_major = GLOBALFIFO_MAJOR;
module_param(globalfifo_major, int, S_IRUGO);

struct globalfifo_dev{
	struct cdev cdev;
	unsigned int current_len;
	unsigned char mem[GLOBALFIFO_SIZE];
	struct mutex mutex;
	wait_queue_head_t r_wait;
	wait_queue_head_t w_wait;
	struct fasync_struct *async_queue;
};

struct globalfifo_dev *globalfifo_devp;

/***************functions*********************/
static int globalfifo_fasync(int fd, struct file *filp, int mode){
	struct globalfifo_dev *dev = filp->private_data;
	return fasync_helper(fd, filp, mode, &dev->async_queue); //setup asynchronous for the dev
}

static int globalfifo_open(struct inode *inode, struct file *filp){
	filp->private_data = globalfifo_devp;
	return 0;
}

static int globalfifo_release(struct inode *inode, struct file *filp){
	globalfifo_fasync(-1, filp, 0);
	return 0;
}

static long globalfifo_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){
	struct globalfifo_dev *dev = filp->private_data;

	switch(cmd){
	case FIFO_CLEAR:
		mutex_lock(&dev->mutex);
		dev->current_len = 0;
		memset(dev->mem, 0, GLOBALFIFO_SIZE);
		mutex_unlock(&dev->mutex);

		printk(KERN_INFO "globalfifo is set to zero\n");
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static unsigned int globalfifo_poll(struct file *filp, poll_table *wait){
	unsigned int mask = 0;
	struct globalfifo_dev *dev = filp->private_data;

	mutex_lock(&dev->mutex);
	poll_wait(filp, &dev->r_wait, wait);
	poll_wait(filp, &dev->w_wait, wait);
	if(dev->current_len != 0){
		mask |= POLLIN | POLLRDNORM;
	}
	if(dev->current_len != GLOBALFIFO_SIZE){
		mask |= POLLOUT | POLLWRNORM;
	}

	mutex_unlock(&dev->mutex);
	return mask;
}


static ssize_t globalfifo_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos){
	int ret;
	struct globalfifo_dev *dev = filp->private_data;
	DECLARE_WAITQUEUE(wait, current); //create a wiate queue for the current task

	mutex_lock(&dev->mutex);
	add_wait_queue(&dev->r_wait, &wait);

	while(dev->current_len == 0){ //nothing to read
		if(filp->f_flags & O_NONBLOCK){
			ret = -EAGAIN;
			goto out;
		}

		//the device is IO block, but there is nothing to read now
		__set_current_state(TASK_INTERRUPTIBLE);
		mutex_unlock(&dev->mutex);

		schedule(); //quit CPU, sleep
		//wake up from sleep
		if(signal_pending(current)){ //check whether there is a signal to be solve.
			//signal is activated restart the read function
			ret = -ERESTARTSYS;
			goto out2;
		}
		// no signal, process the unfinished function
		mutex_lock(&dev->mutex);
	}

	if(count > dev->current_len){
		count = dev->current_len;
	}

	if(copy_to_user(buf, dev->mem, count)){
		//unsuccess, copy_to_user return non-zero
		ret = -EFAULT;
		goto out;
	} else{
		memcpy(dev->mem, dev->mem+count, dev->current_len - count); //fifo
		dev->current_len -= count;
		printk(KERN_INFO "read %d bytes, current_len: %d\n", count, dev->current_len);

		wake_up_interruptible(&dev->w_wait);
		ret = count;
	}

out:
	mutex_unlock(&dev->mutex);
out2:
	remove_wait_queue(&dev->r_wait, &wait);
	set_current_state(TASK_RUNNING);
	return ret;
}


static ssize_t globalfifo_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos){
	struct globalfifo_dev *dev = filp->private_data;
	int ret;
	DECLARE_WAITQUEUE(wait, current);

	mutex_lock(&dev->mutex);
	add_wait_queue(&dev->w_wait, &wait);

	while(dev->current_len == GLOBALFIFO_SIZE){ //no mem to write
		if(filp->f_flags & O_NONBLOCK){ //non-block IO
			ret = -EAGAIN;
			goto out;
		}

		__set_current_state(TASK_INTERRUPTIBLE);
		mutex_unlock(&dev->mutex);

		schedule();
		if(signal_pending(current)){
			//if there is a signal to be solved, restart the function to check the device availbility
			ret = -ERESTARTSYS;
			goto out2;
		}

		mutex_lock(&dev->mutex);
	}

	if(count > GLOBALFIFO_SIZE - dev->current_len){
		count = GLOBALFIFO_SIZE - dev->current_len;
	}

	if(copy_from_user(dev->mem + dev->current_len, buf, count)){ //fail
		ret = -EFAULT;
		goto out;
	} else{ //success, fifo
		dev->current_len += count;
		printk(KERN_INFO "written %d bytes, current_len: %d\n", count, dev->current_len);
		wake_up_interruptible(&dev->r_wait);

		if(dev->async_queue){
			kill_fasync(&dev->async_queue, SIGIO, POLL_IN);
			printk(KERN_DEBUG, "%s kill SIGIO\n", __func__);
		}

		ret = count;
	}

out:
	mutex_unlock(&dev->mutex);
out2:
	remove_wait_queue(&dev->w_wait, &wait);
	set_current_state(TASK_RUNNING);
	return ret;
}


static const struct file_operations globalfifo_fops = {
	.owner = THIS_MODULE,
	.read = globalfifo_read,
	.write = globalfifo_write,
	.open = globalfifo_open,
	.release = globalfifo_release,
	.unlocked_ioctl = globalfifo_ioctl,
	.poll = globalfifo_poll,
	.fasync = globalfifo_fasync,
};

static void globalfifo_setup_cdev(struct globalfifo_dev *dev, int index){ //link the operations and add the cdev
	int err, devno = MKDEV(globalfifo_major, index);

	cdev_init(&dev->cdev, &globalfifo_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&dev->cdev, devno, 1);
	if(err){
		printk(KERN_NOTICE "Error %d adding globalfifo %d", err, index);
	}
}

static int __init globalfifo_init(void){
	int ret;
	dev_t devno = MKDEV(globalfifo_major, 0);

	if(globalfifo_major){
		ret = register_chrdev_region(devno, 1, "globalfifo_async");
	} else{
		ret = alloc_chrdev_region(&devno, 0, 1, "globalfifo_async");
		globalfifo_major = MAJOR(devno);
	}

	if(ret < 0){
		return ret;
	}

	globalfifo_devp = kzalloc(sizeof(struct globalfifo_dev), GFP_KERNEL);
	if(!globalfifo_devp){
		ret = -ENOMEM;
		goto fail_malloc;
	}

	globalfifo_setup_cdev(globalfifo_devp, 0);

	mutex_init(&globalfifo_devp->mutex);
	init_waitqueue_head(&globalfifo_devp->r_wait);
	init_waitqueue_head(&globalfifo_devp->w_wait);

	return 0;

fail_malloc:
	unregister_chrdev_region(devno, 1);
	return ret;
}
module_init(globalfifo_init);


static void __exit globalfifo_exit(void){
	cdev_del(&globalfifo_devp->cdev);
	kfree(globalfifo_devp);
	unregister_chrdev_region(MKDEV(globalfifo_major, 0), 1);
}
module_exit(globalfifo_exit);

MODULE_LICENSE("GPL v2");
