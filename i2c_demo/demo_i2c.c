// demo_i2c.c —— 最简 I2C 客户端驱动（T4）
//
// 教学目的：把 i2c_driver 的完整骨架走一遍，不依赖任何真实器件。
// 配套 i2c-stub.ko 提供一个 256 寄存器的假芯片，本驱动绑到它上面。
//
// 三个考点：
//   ① i2c_driver 三件套：id_table 匹配 / probe 初始化 / remove 释放
//   ② probe 里做 SMBus 读寄存器，返回值必须判负（-ENXIO = 器件没应答）
//   ③ 用 misc 设备把寄存器读写暴露给用户态（copy_to_user 不能用 memcpy）

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define DEMO_I2C_ADDR	0x50	/* 假芯片地址，与 insmod i2c-stub chip_addr=0x50 对应 */
#define DEMO_REG_ID	0x00	/* 用 0x00 号寄存器当“当前值” */

struct demo_i2c {
	struct i2c_client *client;
	struct miscdevice misc;
};

static ssize_t demo_read(struct file *f, char __user *buf, size_t len,
			 loff_t *ppos)
{
	struct demo_i2c *d = container_of(f->private_data, struct demo_i2c, misc);
	char tmp[32];
	int val, n;

	if (*ppos)
		return 0;

	val = i2c_smbus_read_byte_data(d->client, DEMO_REG_ID);
	if (val < 0) {
		dev_err(&d->client->dev, "读寄存器失败: %d\n", val);
		return val;
	}

	n = scnprintf(tmp, sizeof(tmp), "reg[0x%02x]=0x%02x\n", DEMO_REG_ID, val);
	if (n > len)
		n = len;

	if (copy_to_user(buf, tmp, n))	/* 内核对用户指针必须用 copy_* */
		return -EFAULT;

	*ppos += n;
	return n;
}

static ssize_t demo_write(struct file *f, const char __user *buf, size_t len,
			  loff_t *ppos)
{
	struct demo_i2c *d = container_of(f->private_data, struct demo_i2c, misc);
	unsigned char kbuf[8];
	int ret;

	if (len == 0)
		return 0;
	if (len > sizeof(kbuf))
		len = sizeof(kbuf);

	if (copy_from_user(kbuf, buf, len))
		return -EFAULT;

	ret = i2c_smbus_write_byte_data(d->client, DEMO_REG_ID, kbuf[0]);
	if (ret < 0)
		return ret;

	dev_info(&d->client->dev, "写 reg[0x%02x] = 0x%02x\n", DEMO_REG_ID,
		 kbuf[0]);
	*ppos += len;
	return len;
}

static const struct file_operations demo_fops = {
	.owner	= THIS_MODULE,
	.read	= demo_read,
	.write	= demo_write,
};

static int demo_i2c_probe(struct i2c_client *client)
{
	struct demo_i2c *d;
	int val, ret;

	dev_info(&client->dev, "probe 被调用：地址 0x%02x，适配器 %s\n",
		 client->addr, client->adapter->name);

	/*
	 * 真实场景这里是“读芯片 ID 确认型号”的固定动作。
	 * 返回值必须判负：-ENXIO 表示该地址上没有器件应答，
	 * -EREMOTEIO 表示应答了但数据不完整。
	 */
	val = i2c_smbus_read_byte_data(client, DEMO_REG_ID);
	if (val < 0) {
		dev_err(&client->dev, "探测失败（器件无应答）: %d\n", val);
		return val;
	}
	dev_info(&client->dev, "寄存器 0x%02x 初值 = 0x%02x\n", DEMO_REG_ID, val);

	d = devm_kzalloc(&client->dev, sizeof(*d), GFP_KERNEL);
	if (!d)
		return -ENOMEM;

	d->client = client;
	d->misc.minor = MISC_DYNAMIC_MINOR;
	d->misc.name = "demo_i2c";
	d->misc.fops = &demo_fops;
	d->misc.parent = &client->dev;

	ret = misc_register(&d->misc);
	if (ret) {
		dev_err(&client->dev, "misc_register 失败: %d\n", ret);
		return ret;
	}

	i2c_set_clientdata(client, d);	/* 取出时用 i2c_get_clientdata */
	dev_info(&client->dev, "就绪：/dev/%s\n", d->misc.name);
	return 0;
}

/* 6.11 起 remove 回调改为返回 void（老代码返回 int，面试常问这个变化） */
static void demo_i2c_remove(struct i2c_client *client)
{
	struct demo_i2c *d = i2c_get_clientdata(client);

	misc_deregister(&d->misc);
	dev_info(&client->dev, "remove：设备已注销\n");
}

static const struct i2c_device_id demo_i2c_id[] = {
	{ "demo_i2c", DEMO_I2C_ADDR },
	{ }
};
MODULE_DEVICE_TABLE(i2c, demo_i2c_id);

static struct i2c_driver demo_i2c_driver = {
	.driver = {
		.name = "demo_i2c",	/* 名字必须与 new_device 里给的一致 */
	},
	.probe		= demo_i2c_probe,
	.remove		= demo_i2c_remove,
	.id_table	= demo_i2c_id,
};

module_i2c_driver(demo_i2c_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("hades");
MODULE_DESCRIPTION("T4: minimal i2c client driver bound to i2c-stub fake chip");
