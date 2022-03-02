#include "types.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "stat.h"

#include "ring_buffer.h"
#include "pty.h"

int slave_pty_open(struct inode* ip) {
    return -1;
}

int slave_pty_read(struct inode* ip, char* buf, int n) {
    master_pty* m = lookup_master_pty(ip->minor);
    if (m == NULL) {
        return -1;
    }

    return ring_buffer_read(&m->out, buf, n);
}

int slave_pty_write(struct inode* ip, char* buf, int n) {
    master_pty* m = lookup_master_pty(ip->minor);
    if (m == NULL) {
        return -1;
    }

    return ring_buffer_write(&m->in, buf, n);
}

int slave_pty_ioctl(struct inode* ip, int request, void* argp) {
    return -1;
}

int slave_pty_select_check(struct inode* ip) {
    return -1;
}

int slave_pty_select_block(struct inode* ip, int pid, int fd) {
    return -1;
}

void slave_pty_init() {
    devsw[SLAVE_PTY].open = slave_pty_open;
    devsw[SLAVE_PTY].read = slave_pty_read;
    devsw[SLAVE_PTY].write = slave_pty_write;
    devsw[SLAVE_PTY].ioctl = slave_pty_ioctl;
    devsw[SLAVE_PTY].select_check = slave_pty_select_check;
    devsw[SLAVE_PTY].select_block = slave_pty_select_block;
}