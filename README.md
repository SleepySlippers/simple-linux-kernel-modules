# simple-linux-kernel-modules
Linux kernel modules homework

## how to use
run 
`qemu-system-x86_64 -kernel bzImage --enable-kvm -m 128m -initrd tiny-initrd.img`

if it runs then read for `functionality` part below 

## (optional) create initrd manually

run `python3 make-tiny-image.py` (script taken from https://gitlab.com/berrange/tiny-vm-tools and modified a little)

## (optional) create bzImage manually

download linux 6.5 kernel sources

```
cp -r linux-6.5/* $PATH_TO_KERNEL_SOURCE # or use phonebook.patch
cd $PATH_TO_KERNEL_SOURCE
make defconfig
make menuconfig # and search for `phonebook` with slash and mark to be build-in
make
## and `bzImage` will be located in arch/x86/boot/bzImage (on other architectures didn't tested)
```

## functionality

This module called phonebook implement simple phone book which can store some info about users and their contact info

it has functionality to add user, del by surname, get by surname

communication happens via character device `phonebook` in normal condition it must be already bound as `/dev/phonebook` 

if not just do the following `find / -name "phonebook*"` to find where it located. you need that one which located in `/sys/devices/virtual` and you need to cat its major and minor numbers e.g. `cat /sys/devices/virtual/phonebook/phonebook-0/dev` it must print two numbers. Then you need to do `mknod -m 666 /dev/phonebook/ c $major $minor` where $major and $minor this two numbers which you see just now


be careful about spaces and number of arguments when add user it must be 5 space separated groups of characters and `age` must be correct number less than `999` all arguments will be cut at `32` characters

example:
```
# echo "+ surname name 66 +76666666 email" > /dev/phonebook
# echo "? surname" > /dev/phonebook
# cat /dev/phonebook
### here must be printed output like `surname name 66 +76666666 email`
# echo "- surname" > /dev/phonebook
# echo "? surname" > /dev/phonebook
### here must be printed nothing
```