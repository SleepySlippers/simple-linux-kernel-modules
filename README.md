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
cp -r linux-6.5/* $PATH_TO_KERNEL_SOURCE # or use keycounter.patch
cd $PATH_TO_KERNEL_SOURCE
make defconfig
make menuconfig # and search for `keycounter` with slash and mark to be build-in
make
## and `bzImage` will be located in arch/x86/boot/bzImage (on other architectures didn't tested)
```

## functionality

This module called keycounter implement simple additional key handler which counts their number

example:
```
# tap some keys
sleep 60; dmesg | tail;
## here must be shown a number of tapped keys
```