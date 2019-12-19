/*
* @Author: FloodShao
* @Date:   2019-12-18 14:03:59
* @Last Modified by:   FloodShao
* @Last Modified time: 2019-12-18 15:28:12
*/
#include <linux/module.h>
#include <linux/fs.h> //register_chrdev_region
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/uaccess.h>


#define GLOBALMEM_SIZE	0x1000
#define MEM_CLEAR		0x01
#define GLOABLMEM_MAJOR	230
#define DEVICE_NUM		10

static int globalmem_major = GLOABLMEM_MAJOR;
module_param(globalmem_major, int, S_IRUGO); //S_IRUGO access 

struct globalmem_dev{
	struct cdev cdev;
	unsigned char mem[GLOBALMEM_SIZE];
};

struct globalmem_dev *globalmem_devp; //define a global pointer for the device

static int globalmem_open(struct inode *inode, struct file *filp){
	//unnecessary to writ the open function
	//
	//set the filp->private_data to be the inode->i_cdev;
	//container_of gets the pointer of globalmem_dev which has inode->i_cdev
	struct globalmem_dev *dev = container_of(inode->i_cdev, struct globalmem_dev, cdev);
	filp->private_data = dev;
	return 0;
}

static int globalmem_release(struct inode *inode, struct file *filp){
	return 0;
}

static ssize_t globalmem_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos){
	unsigned long p = *ppos;
	unsigned int count = size;
	int ret = 0;
	struct globalmem_dev *dev = filp->private_data;

	if(p >= GLOBALMEM_SIZE)
		return 0;
	if(count > GLOBALMEM_SIZE - p){
		count = GLOBALMEM_SIZE - p;
	}

	if(copy_to_user(buf, dev->mem + p, count)){
		ret = -EFAULT;
	} else{
		*ppos += count;
		ret = count;

		printk(KERN_INFO "Read %u bytes from %lu\n", count, p);
	}

	return ret;
}

static ssize_t globalmem_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos){

	int ret =  0;
	struct globalmem_dev *dev = filp->private_data;
	unsigned int count = size;
	unsigned long p = *ppos;

	if(p >= GLOBALMEM_SIZE)
		return 0;
	if(count > GLOBALMEM_SIZE - p)
		count = GLOBALMEM_SIZE - p;

	if(copy_from_user(dev->mem+p, buf, count)){ //copy_*_user(*to, *from, count)
		ret = -EFAULT;
	} else{
		*ppos += count;
		ret = count;

		printk(KERN_INFO "Write %u bytes from %lu\n", count, p);
	}

	return ret;
}

static loff_t globalmem_llseek(struct file *filp, loff_t offset, int orig){

	//typedef __kernel_loff_t loff_t
	//typedef long long __kernel_loff_t
	//offset is actually an unsigned number
	loff_t ret = 0;
	switch(orig){
	case 0:
		if(offset < 0){
			ret = -EINVAL;
			return ret;
		}
		if((unsigned int)offset > GLOBALMEM_SIZE){
			ret = -EINVAL;
			return ret;
		}
		filp->f_pos = (unsigned int) offset;
		ret = filp->f_pos;
		break;

	case 1:
		if(offset + filp->f_pos < 0){
			ret = -EINVAL;
			return ret;
		}
		if(offset + filp->f_pos > GLOBALMEM_SIZE){
			ret = -EINVAL;
			return ret;
		}
		filp->f_pos += offset;
		ret = filp->f_pos;
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static long globalmem_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){

	struct globalmem_dev *dev = filp->private_data;
	switch(cmd){

	case MEM_CLEAR:
		memset(dev->mem, 0, GLOBALMEM_SIZE);
		printk(KERN_INFO "globalmem is set to zero\n");
		break;
	default:
		return -EINVAL;
	}

	return 0;

}


static const struct file_operations globalmem_fops = { //defined in fs.h
	.owner = THIS_MODULE,
	.llseek = globalmem_llseek,
	.open = globalmem_open,
	.release = globalmem_release,
	.read = globalmem_read,
	.write = globalmem_write,
	.unlocked_ioctl = globalmem_ioctl,
};


static void globalmem_setup_cdev(struct globalmem_dev *dev, int index){

	int err, devno = MKDEV(globalmem_major, index); //define the device number

	cdev_init(&dev->cdev, &globalmem_fops); //init the cdev with fops

	dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&dev->cdev, devno, 1); //add the devno to the cdev
	if(err)
		printk(KERN_NOTICE "Error %d adding globalmem %d", err, index);

}


static int __init globalmem_init(void){

	int ret;
	int i;
	dev_t devno = MKDEV(globalmem_major, 0); //create a device number

	// register the major and minor of the device
	if(globalmem_major){
		ret = register_chrdev_region(devno, DEVICE_NUM, "globalmem");
	}
	else{
		ret = alloc_chrdev_region(&devno, 0, DEVICE_NUM, "globalmem"); //major, minor, device_num, device_name
		globalmem_major = MAJOR(devno);
	}
	if(ret<0){
		return ret;
	}

	//alloc the mem space for dev 
	globalmem_devp = kzalloc(sizeof(struct globalmem_dev) * DEVICE_NUM, GFP_KERNEL);
	if(!globalmem_devp){ //failed allocate
		ret = -ENOMEM;
		goto fail_malloc;
	}

	for(i = 0; i<DEVICE_NUM; i++){
		globalmem_setup_cdev(globalmem_devp+i, i); //config the data struct of globalmem_dev in each mem space 
	}


fail_malloc:
	unregister_chrdev_region(devno, DEVICE_NUM);
	return ret;

}

static void __exit globalmem_exit(void){

	int i;
	for(i = 0; i<DEVICE_NUM; i++){
		cdev_del(&(globalmem_devp + i)->cdev); //delete the chrdev in kernel space
	}
	kfree(globalmem_devp);
	unregister_chrdev_region(MKDEV(globalmem_major, 0), DEVICE_NUM); //delete the device number

}

module_init(globalmem_init);
module_exit(globalmem_exit);
MODULE_LICENSE("GPL v2");