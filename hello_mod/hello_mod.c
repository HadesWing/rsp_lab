// T1：第一个内核模块 —— 验证 module_init/exit、printk、insmod/rmmod 全链路
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/jiffies.h>

static int __init hello_init(void)
{
	pr_info("hello_mod: 加载成功，jiffies=%lu\n", jiffies);
	return 0;
}

static void __exit hello_exit(void)
{
	pr_info("hello_mod: 卸载\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("hades");
MODULE_DESCRIPTION("bsp_lab 第一个内核模块");
