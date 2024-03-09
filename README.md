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
cd $PATH_TO_KERNEL_SOURCE
git apply $PATH_TO_SCHED_NUM_PATCH/sched_num.patch # or use linux `patch` command
make defconfig
make
## and `bzImage` will be located in arch/x86/boot/bzImage (on other architectures didn't tested)
```

## functionality

This patch adds `sched_num` counter into `task_struct` which counts how many times this task is planned to be the next executed

it can be accessed via

`cat /proc/<pid_id>/sched_num`