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

static struct timer_list my_timer;

static atomic_t key_count = ATOMIC_INIT(0);

static const int TIMEOUT = 60000;

static void schedule_timer(void) {
    mod_timer(&my_timer, jiffies + msecs_to_jiffies(TIMEOUT));
}

void my_timer_callback(struct timer_list *timer) {
    int number = atomic_fetch_and(0, &key_count); // read and set to zero (it seems there is no fetch_set, so i need to use fetch_and kekw )
    printk(KERN_INFO "Characters typed in the last minute: %d\n", number);
    schedule_timer();
}

irqreturn_t irq_handler(int irq, void *dev_id)
{
    atomic_inc(&key_count);
    return IRQ_HANDLED;
}

static const int PS2_IRQ = 1; // IRQ for PS/2 keyboard https://en.wikipedia.org/wiki/Interrupt_request#Master_PIC

static int __init keycounter_init(void) {

    printk(KERN_INFO "Keycounter started\n");

    timer_setup(&my_timer, my_timer_callback, 0);
    schedule_timer();
    
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
    del_timer(&my_timer);
    printk(KERN_INFO "Keycounter exited\n");
}

module_init(keycounter_init);
module_exit(keycounter_exit);