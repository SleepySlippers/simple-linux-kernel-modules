#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/time.h>
#include <linux/interrupt.h>

MODULE_LICENSE("MIT");
MODULE_AUTHOR("arseny staroverov.ag@phystech.ru");
MODULE_DESCRIPTION("reports how much keys was entered");
MODULE_VERSION("0.0");

static unsigned int key_count = 0;

static time64_t last_time;

irqreturn_t irq_handler(int irq, void *dev_id)
{
    time64_t cur_time = ktime_get_seconds();

    if (cur_time < last_time + 60) {
        ++key_count;
    } else {
        last_time = cur_time;
        printk(KERN_INFO "Characters typed in the last minute: %u\n", key_count);
        key_count = 1;
    }

    return IRQ_HANDLED;
}

static const int PS2_IRQ = 1; // IRQ for PS/2 keyboard https://en.wikipedia.org/wiki/Interrupt_request#Master_PIC

static int __init keycounter_init(void) {

    printk(KERN_INFO "Keycounter started\n");

    last_time = ktime_get_seconds();
    
    int err = request_irq(PS2_IRQ, irq_handler, IRQF_SHARED, "Keycounter", (void*) irq_handler);
    if (err) {
        pr_err("Failed to register IRQ handler ret code: %d\n", err);
        return -EIO;
    }
    
    printk(KERN_INFO "Keycounter module initialized\n");

    return 0;
}

static void __exit keycounter_exit(void) {
    free_irq(PS2_IRQ, (void*) irq_handler);
    printk(KERN_INFO "Keycounter exited\n");
}

module_init(keycounter_init);
module_exit(keycounter_exit);