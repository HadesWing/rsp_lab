// T3：platform driver + 设备树 compatible 匹配 + misc 字符设备 + sysfs 属性
//
// 这是 BSP 面试的核心考点：
//   设备树节点(compatible="hades,demo") → of_match_table 匹配 → probe 被调用 → 注册 /dev/bspdemo
// 注意：probe 是否被调用，就是"设备树和驱动有没有接上"的唯一判据。
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/sysfs.h>

#define DEVNAME "bspdemo"
#define BUFMAX  64

static char   msg[BUFMAX] = "hello from bsp_lab\n";
static size_t msg_len = 19;

static ssize_t bspdemo_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{
	if (*off >= msg_len)
		return 0;
	if (len > msg_len - *off)
		len = msg_len - *off;
	if (copy_to_user(buf, msg + *off, len))
		return -EFAULT;
	*off += len;
	return len;
}

static ssize_t bspdemo_write(struct file *f, const char __user *buf, size_t len, loff_t *off)
{
	if (len > BUFMAX - 1)
		len = BUFMAX - 1;
	if (copy_from_user(msg, buf, len))
		return -EFAULT;
	msg[len] = '\0';
	msg_len = len;
	pr_info("bspdemo: 用户写入 %zu 字节\n", len);
	return len;
}

static const struct file_operations bspdemo_fops = {
	.owner = THIS_MODULE,
	.read  = bspdemo_read,
	.write = bspdemo_write,
};

static struct miscdevice bspdemo_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = DEVNAME,
	.fops  = &bspdemo_fops,
	.mode  = 0666,		/* 方便免 sudo 读写，产品里不该这么干 */
};

/* /sys/class/misc/bspdemo/msg —— 只读，验证 sysfs 属性怎么写 */
static ssize_t msg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%s", msg);
}
static DEVICE_ATTR_RO(msg);

static int bspdemo_probe(struct platform_device *pdev)
{
	int ret;

	/* 能打印这一行，才说明设备树节点和 of_match_table 接上了 */
	pr_info("bspdemo: probe 被调用（设备树 compatible 匹配成功）\n");

	ret = misc_register(&bspdemo_misc);
	if (ret) {
		pr_err("bspdemo: misc_register 失败 %d\n", ret);
		return ret;
	}
	device_create_file(bspdemo_misc.this_device, &dev_attr_msg);
	return 0;
}

/* 6.11 起 platform_driver.remove 改为返回 void */
static void bspdemo_remove(struct platform_device *pdev)
{
	device_remove_file(bspdemo_misc.this_device, &dev_attr_msg);
	misc_deregister(&bspdemo_misc);
	pr_info("bspdemo: remove\n");
}

static const struct of_device_id bspdemo_of_match[] = {
	{ .compatible = "hades,demo" },
	{ }
};
MODULE_DEVICE_TABLE(of, bspdemo_of_match);

static struct platform_driver bspdemo_driver = {
	.probe  = bspdemo_probe,
	.remove = bspdemo_remove,
	.driver = {
		.name = "bspdemo",
		.of_match_table = bspdemo_of_match,
	},
};
module_platform_driver(bspdemo_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("hades");
MODULE_DESCRIPTION("bsp_lab demo platform driver");
