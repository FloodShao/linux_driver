/*
* @Author: FloodShao
* @Date:   2019-12-17 18:52:19
* @Last Modified by:   FloodShao
* @Last Modified time: 2019-12-18 13:56:31
*/

#include <linux/module.h>
#include <linux/fs.h> //register_chrdev_region,alloc_chrdev_region
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/slab.h> //mem management
#include <linux/uaccess.h> //copy_*_user

#define		GLOBAL_MEM_SIZE		0x1000
//MEM_CLEAR is a naive ioctl cmd
//#define 	MEM_CLEAR			0x1
//
#define 	GLOBAL_MEM_MAGIC	'g'
#define 	MEM_CLEAR			_IO(GLOBAL_MEM_MAGIC, 0)


#define 	GLOBAL_MEM_MAJOR	230

static int globalmem_major = GLOBAL_MEM_MAJOR;

module_param(globalmem_major, int, S_IRUGO);

struct globalmem_dev{
	struct cdev cdev;
	unsigned char mem[GLOBAL_MEM_SIZE];
};

//define a point of cdev
struct globalmem_dev *globalmem_devp;

static int globalmem_open(struct inode *inode, struct file *filp){
	filp->private_data = globalmem_devp; //setup the private data to be the device pointer
	return 0;
}

static int globalmem_release(struct inode *inode, struct file *filp){
	return 0;
}

static long globalmem_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){

	struct globalmem_dev *dev = filp->private_data;
	switch(cmd){
		case MEM_CLEAR:
			memset(dev->mem, 0, GLOBAL_MEM_SIZE);
			printk(KERN_INFO "globalmem is set to 0\n");
			break;

		default:
			return -EINVAL;
	}

	return 0;

}

static ssize_t globalmem_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos){

	unsigned long p = *ppos;
	unsigned int count = size;
	int ret = 0;
	struct globalmem_dev *dev = filp->private_data;

	if(p >= GLOBAL_MEM_SIZE){
		return 0;
	}
	if(count > GLOBAL_MEM_SIZE - p){
		count = GLOBAL_MEM_SIZE - p;
	}

	if(copy_to_user(buf, dev->mem + p, count)){ 
		return -EFAULT;
	} else{
		*ppos += count;
		ret = count;

		printk(KERN_INFO "read %u bytes from %lu\n", count, p);
	}

	return ret;
}

static ssize_t globalmem_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos){

	unsigned long p = *ppos;
	unsigned int count = size;
	int ret = 0;
	struct globalmem_dev *dev = filp->private_data;

	if(p >= GLOBAL_MEM_SIZE){
		return 0;
	}

	if(count > GLOBAL_MEM_SIZE - p){
		count = GLOBAL_MEM_SIZE - p;
	}

	if(copy_from_user(dev->mem + p, buf, count)){
		ret = -EFAULT;
	} else{
		*ppos += count;
		ret = count;

		printk(KERN_INFO "written %u bytes from %lu\n", count, p);
	}

	return ret;
}

static loff_t globalmem_llseek(struct file *filp, loff_t offset, int orig){
	loff_t ret = 0;
	switch(orig){
	case 0: //seek from the start point of the file
		if(offset < 0){
			ret = -EFAULT;
			break;
		}
		if((unsigned int)offset > GLOBAL_MEM_SIZE){
			ret = -EINVAL;
			break;
		}
		filp->f_pos = (unsigned int) offset;
		ret = filp->f_pos;
		break;

	case 1: //seek from the current file position
		if((filp->f_pos + offset) > GLOBAL_MEM_SIZE){
			ret = -EINVAL;
			break;
		}
		if((filp->f_pos + offset) < 0){
			ret = -EINVAL;
			break;
		}

		filp->f_pos += offset;
		ret = filp->f_pos;
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}


static const struct file_operations globalmem_fops = {
	.owner = THIS_MODULE,
	.llseek = globalmem_llseek,
	.read = globalmem_read,
	.write = globalmem_write,
	.unlocked_ioctl = globalmem_ioctl,
	.open = globalmem_open,
	.release = globalmem_release,

};

static void globalmem_setup_cdev(struct globalmem_dev *dev, int index){
	int err, devno = MKDEV(globalmem_major, index);

	cdev_init(&dev->cdev, &globalmem_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&dev->cdev, devno, 1);
	if(err){
		printk (KERN_NOTICE "Error %d adding globalmem %d", err, index);
	}
}

static int __init globalmem_init(void){
	int ret;
	dev_t devno = MKDEV(globalmem_major, 0);

	if(globalmem_major){
		ret = register_chrdev_region(devno, 1, "globalmem");
	} else{
		ret = alloc_chrdev_region(&devno, 0, 1, "globalmem");
		globalmem_major = MAJOR(devno);
	}

	if(ret <0)
		return ret;

	globalmem_devp = kzalloc(sizeof(struct globalmem_dev), GFP_KERNEL);
	if(!globalmem_devp){
		ret = -ENOMEM;
		goto fail_malloc;
	}

	globalmem_setup_cdev(globalmem_devp, 0);
	return 0;

fail_malloc:
	unregister_chrdev_region(devno, 0);
	return ret;
}

static void __exit globalmem_exit(void){

	cdev_del(&globalmem_devp->cdev);
	kfree(globalmem_devp);
	unregister_chrdev_region(MKDEV(globalmem_major, 0), 1);

}

module_init(globalmem_init);
module_exit(globalmem_exit);
MODULE_LICENSE("GPL v2");