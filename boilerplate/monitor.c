#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/mm.h>
#include <linux/pid.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#include "monitor_ioctl.h"

#define DEVICE_NAME "container_monitor"
#define CHECK_INTERVAL_SEC 1

struct monitored_node {
    pid_t pid;
    char id[32];
    unsigned long soft_limit;
    unsigned long hard_limit;
    int soft_warned;
    struct list_head list;
};

static LIST_HEAD(monitored_list);
static DEFINE_MUTEX(monitor_lock);
static struct timer_list monitor_timer;
static dev_t dev_num;
static struct cdev c_dev;
static struct class *cl;

/* ================= MEMORY CHECK ================= */

static long get_rss_bytes(pid_t pid) {
    struct task_struct *task;
    struct mm_struct *mm;
    long rss_pages = 0;

    rcu_read_lock();
    task = pid_task(find_vpid(pid), PIDTYPE_PID);
    if (!task) {
        rcu_read_unlock();
        return -1;
    }
    get_task_struct(task);
    rcu_read_unlock();

    mm = get_task_mm(task);
    if (mm) {
        rss_pages = get_mm_rss(mm);
        mmput(mm);
    }
    put_task_struct(task);

    return rss_pages * PAGE_SIZE;
}

/* ================= TIMER ================= */

static void timer_callback(struct timer_list *t) {
    struct monitored_node *node, *tmp;
    long rss;

    mutex_lock(&monitor_lock);

    list_for_each_entry_safe(node, tmp, &monitored_list, list) {
        rss = get_rss_bytes(node->pid);

        /* process died */
        if (rss < 0) {
            list_del(&node->list);
            kfree(node);
            continue;
        }

        /* SOFT LIMIT */
        if (rss > node->soft_limit && !node->soft_warned) {
            printk(KERN_WARNING
                   "[container_monitor] SOFT LIMIT: %s pid=%d rss=%ld limit=%lu\n",
                   node->id, node->pid, rss, node->soft_limit);
            node->soft_warned = 1;
        }

        if (rss <= node->soft_limit)
            node->soft_warned = 0;

        /* HARD LIMIT */
        if (rss > node->hard_limit) {
            struct task_struct *task;

            rcu_read_lock();
            task = pid_task(find_vpid(node->pid), PIDTYPE_PID);
            if (task)
                send_sig(SIGKILL, task, 1);
            rcu_read_unlock();

            printk(KERN_WARNING
                   "[container_monitor] HARD LIMIT: %s pid=%d rss=%ld limit=%lu\n",
                   node->id, node->pid, rss, node->hard_limit);

            list_del(&node->list);
            kfree(node);
        }
    }

    mutex_unlock(&monitor_lock);

    mod_timer(&monitor_timer, jiffies + CHECK_INTERVAL_SEC * HZ);
}

/* ================= IOCTL ================= */

static long monitor_ioctl(struct file *f, unsigned int cmd, unsigned long arg) {
    struct monitor_req req;
    pid_t pid_only;
    struct monitored_node *node, *tmp;

    switch (cmd) {

    case MONITOR_REGISTER:
        if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
            return -EFAULT;

        node = kmalloc(sizeof(*node), GFP_KERNEL);
        if (!node) return -ENOMEM;

        node->pid = req.pid;
        node->soft_limit = req.soft_limit_bytes;
        node->hard_limit = req.hard_limit_bytes;
        node->soft_warned = 0;

        strncpy(node->id, req.id, sizeof(node->id));

        mutex_lock(&monitor_lock);
        list_add(&node->list, &monitored_list);
        mutex_unlock(&monitor_lock);

        printk(KERN_INFO
               "[container_monitor] REGISTER: %s pid=%d soft=%lu hard=%lu\n",
               node->id, node->pid, node->soft_limit, node->hard_limit);

        return 0;

    case MONITOR_UNREGISTER:
        if (copy_from_user(&pid_only, (void __user *)arg, sizeof(pid_only)))
            return -EFAULT;

        mutex_lock(&monitor_lock);

        list_for_each_entry_safe(node, tmp, &monitored_list, list) {
            if (node->pid == pid_only) {
                list_del(&node->list);
                kfree(node);

                printk(KERN_INFO
                       "[container_monitor] UNREGISTER pid=%d\n",
                       pid_only);
                break;
            }
        }

        mutex_unlock(&monitor_lock);
        return 0;

    default:
        return -EINVAL;
    }
}

/* ================= DEVICE ================= */

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = monitor_ioctl,
};

/* ================= INIT ================= */

static int __init monitor_init(void) {
    if (alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME) < 0)
        return -1;

    cl = class_create(DEVICE_NAME);
    device_create(cl, NULL, dev_num, NULL, DEVICE_NAME);

    cdev_init(&c_dev, &fops);
    cdev_add(&c_dev, dev_num, 1);

    timer_setup(&monitor_timer, timer_callback, 0);
    mod_timer(&monitor_timer, jiffies + CHECK_INTERVAL_SEC * HZ);

    printk(KERN_INFO "[container_monitor] Module loaded.\n");
    return 0;
}

/* ================= EXIT ================= */

static void __exit monitor_exit(void) {
    struct monitored_node *node, *tmp;

    del_timer_sync(&monitor_timer);

    mutex_lock(&monitor_lock);
    list_for_each_entry_safe(node, tmp, &monitored_list, list) {
        list_del(&node->list);
        kfree(node);
    }
    mutex_unlock(&monitor_lock);

    cdev_del(&c_dev);
    device_destroy(cl, dev_num);
    class_destroy(cl);
    unregister_chrdev_region(dev_num, 1);

    printk(KERN_INFO "[container_monitor] Module unloaded.\n");
}

module_init(monitor_init);
module_exit(monitor_exit);

MODULE_LICENSE("GPL");
