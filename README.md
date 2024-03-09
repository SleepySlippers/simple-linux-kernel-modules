# simple-linux-kernel-modules
Linux kernel modules homework

This repository contains 3 tasks in corespoinding *branches*

Phonebook https://github.com/SleepySlippers/simple-linux-kernel-modules/tree/phonebook this module adds character device to store and get some user contacts

Keycounter https://github.com/SleepySlippers/simple-linux-kernel-modules/tree/keycounter implement simple additional key handler which counts number of pressed keys

Sched_num https://github.com/SleepySlippers/simple-linux-kernel-modules/tree/sched_num adds `sched_num` counter into `task_struct` which counts how many times this task is planned to be the next executed and can be accessed via `cat /proc/<pid>/sched_num`
