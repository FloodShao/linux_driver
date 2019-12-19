## simple scull driver

### globalmem instructions
```
make
# install the driver
insmod globalmem.ko
# check the driver
cat /proc/devices
# the major should be 230
# make a device in /dev, cmd should be: mknod [name] b/c MAJOR MINOR
mknod /dev/globalmem c 230 0
# check the dev
ls -l /dev | grep globalmem
# process the device
echo 111000 > /dev/globalmem
cat /dev/globalmem
```

### Notes
1. linux header file dir: /usr/src/linux-headers...
2. errno is defined in /linux/errno.h
3. if a nagetive number passed to if condition, the condition is true. only 0 stands for the false condition. noted for copy_*_user() because the 2 API only returns 0 for success and negative number for unsuccess.