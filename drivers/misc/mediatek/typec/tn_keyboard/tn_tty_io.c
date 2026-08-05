#include <linux/types.h>
#include <linux/major.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/fcntl.h>
#include <linux/sched/signal.h>
#include <linux/sched/task.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/devpts_fs.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/console.h>
#include <linux/timer.h>
#include <linux/ctype.h>
#include <linux/kd.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/seq_file.h>
#include <linux/serial.h>
#include <linux/ratelimit.h>

#include <linux/uaccess.h>

#include <linux/kbd_kern.h>
#include <linux/vt_kern.h>
#include <linux/selection.h>

#include <linux/kmod.h>
#include <linux/nsproxy.h>
#include <linux/tn_common.h>
#include "tn_keyboard.h"


/*static int tty_paranoia_check(struct tty_struct *tty, struct inode *inode,
                  const char *routine)
{
    pr_err("TN_KB:tty_paranoia_check1\n");
#ifdef TTY_PARANOIA_CHECK
    pr_err("TN_KB:tty_paranoia_check2\n");
    if (!inode) {
        pr_warn("tty_paranoia_check inode NULL\n");
        return 1;
    }
    if (!tty) {
        pr_warn("(%d:%d): %s: NULL tty\n",
            imajor(inode), iminor(inode), routine);
        return 1;
    }
    if (tty->magic != TTY_MAGIC) {
        pr_warn("(%d:%d): %s: bad magic number\n",
            imajor(inode), iminor(inode), routine);
        return 1;
    }
#endif
    return 0;
}*/



static int tty_write_lock(struct tty_struct *tty, int ndelay)
{
    if (!mutex_trylock(&tty->atomic_write_lock)) {
        if (ndelay)
            return -EAGAIN;
        if (mutex_lock_interruptible(&tty->atomic_write_lock))
            return -ERESTARTSYS;
    }
    return 0;
}
static inline struct tty_struct *file_tty(struct file *file)
{
    struct tty_struct *temp = NULL;
    if(file == NULL){
        pr_err("TN_KB:file is NULL!file:%p\n",file);
        return NULL;
    }
    if(file->private_data == NULL){
        pr_err("TN_KB:file->private_data is NULL!file:%p\n",file);
        return NULL;
    }
    pr_err("TN_KB:file_tty file->private_data != NULL!\n");
    if(((struct tty_file_private *)file->private_data) == NULL){
        pr_err("TN_KB:file->private_data is NULL2!file:%p\n",file);
        return NULL;
    }else{
        temp = ((struct tty_file_private *)file->private_data)->tty;
        pr_err("TN_KB:file_tty ok!\n");
    }
    return temp;
}
static void tty_write_unlock(struct tty_struct *tty)
{
    mutex_unlock(&tty->atomic_write_lock);
    wake_up_interruptible_poll(&tty->write_wait, EPOLLOUT);
}

static void tty_update_time(struct timespec64 *time)
{
    time64_t sec = ktime_get_real_seconds();

    /*
     * We only care if the two values differ in anything other than the
     * lower three bits (i.e every 8 seconds).  If so, then we can update
     * the time of the tty device, otherwise it could be construded as a
     * security leak to let userspace know the exact timing of the tty.
     */
    if ((sec ^ time->tv_sec) & ~7)
        time->tv_sec = sec;
}
/*
 * Split writes up in sane blocksizes to avoid
 * denial-of-service type attacks
 */
static inline ssize_t do_tty_write(
    ssize_t (*write)(struct tty_struct *, struct file *, const unsigned char *, size_t),
    struct tty_struct *tty,
    struct file *file,
    const char __user *buf,
    size_t count)
{
    ssize_t ret, written = 0;
    unsigned int chunk;

    ret = tty_write_lock(tty, file->f_flags & O_NDELAY);
    if (ret < 0){
        pr_err("TN_KB:do_tty_write tty_write_lock err\n");
        return ret;
    }

    /*
     * We chunk up writes into a temporary buffer. This
     * simplifies low-level drivers immensely, since they
     * don't have locking issues and user mode accesses.
     *
     * But if TTY_NO_WRITE_SPLIT is set, we should use a
     * big chunk-size..
     *
     * The default chunk-size is 2kB, because the NTTY
     * layer has problems with bigger chunks. It will
     * claim to be able to handle more characters than
     * it actually does.
     *
     * FIXME: This can probably go away now except that 64K chunks
     * are too likely to fail unless switched to vmalloc...
     */
    chunk = 2048;
    if (test_bit(TTY_NO_WRITE_SPLIT, &tty->flags))
        chunk = 65536;
    if (count < chunk)
        chunk = count;

    /* write_buf/write_cnt is protected by the atomic_write_lock mutex */
    if (tty->write_cnt < chunk) {
        unsigned char *buf_chunk;

        if (chunk < 1024)
            chunk = 1024;

        buf_chunk = kmalloc(chunk, GFP_KERNEL);
        if (!buf_chunk) {
            ret = -ENOMEM;
            pr_err("TN_KB:do_tty_write kmalloc err\n");
            goto out;
        }
        kfree(tty->write_buf);
        tty->write_cnt = chunk;
        tty->write_buf = buf_chunk;
    }
    /* Do the write .. */
    for (;;) {
        size_t size = count;
        if (size > chunk)
            size = chunk;
        ret = -EFAULT;
        memcpy(tty->write_buf, buf, size);
        ret = write(tty, file, tty->write_buf, size);
        if (ret <= 0){
            pr_err("TN_KB:do_tty_write write err\n");
            break;
        }
            
        written += ret;
        buf += ret;
        count -= ret;
        if (!count)
            break;
        ret = -ERESTARTSYS;
        if (signal_pending(current)){
            pr_err("TN_KB:do_tty_write signal_pending err\n");
            break;
        }
        cond_resched();
    }
    if (written) {
        tty_update_time(&file_inode(file)->i_mtime);
        ret = written;
    }
out:
    tty_write_unlock(tty);
    return ret;
}
/*

const char *tty_driver_name(const struct tty_struct *tty)
{
    if (!tty || !tty->driver)
        return "";
    return tty->driver->name;
}*/
ssize_t tn_tty_write(struct file *file, const char __user *buf,
                        size_t count, loff_t *ppos)
{
    struct tty_struct *tty;
    struct tty_ldisc *ld;
    ssize_t ret;
    pr_err("TN_KB:tn_tty_write file_tty begin!\n");
    tty = file_tty(file);
    pr_err("TN_KB:tn_tty_write file_tty end!\n");
    if (tty == NULL){
        pr_err("TN_KB:tn_tty_write error!tty == NULL!\n");
        return -EIO;
    }
    if (tty->ops == NULL){
        pr_err("TN_KB:tn_tty_write error!tty->ops == NULL!\n");
        return -EIO;
    }
    if (tty->ops->write == NULL){
        pr_err("TN_KB:tn_tty_write error!tty->ops->write == NULL!\n");
        return -EIO;
    }
    if (tty_io_error(tty)){
        pr_err("TN_KB:tn_tty_write error!tty_io_error!\n");
        return -EIO;
    }
    pr_err("TN_KB:tn_tty_write tty_io_error(tty) ok!\n");
    if (tty->ops->write_room == NULL){
        pr_err("TN_KB:tn_tty_write error!tty->ops->write_room == NULL!\n");
        return -EIO;
    }

    pr_err("TN_KB:tn_tty_write tty_ldisc_ref\n");
    ld = tty_ldisc_ref(tty);
    if (!ld || !ld->ops){
        pr_err("TN_KB:tn_tty_write tty_ldisc_ref err!\n");
        return -EIO;
    }
    pr_err("TN_KB:tn_tty_write ld->ops->write1 ret:%d\n");
    if (!ld->ops->write){
        ret = -EIO;
        pr_err("TN_KB:tn_tty_write ld->ops->write == null ret:%d\n",ret);
        return ret;
    }else{
        ret = do_tty_write(ld->ops->write, tty, file, buf, count);
    }
    pr_err("TN_KB:tn_tty_write ld->ops->write2 ret:%d\n",ret);
    tty_ldisc_deref(ld);
    return ret;
}
