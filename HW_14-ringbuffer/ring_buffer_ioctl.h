#ifndef RING_BUFFER_IOCTL_H
#define RING_BUFFER_IOCTL_H

#include <linux/ioctl.h>

// IOCTL команды
#define RB_MAGIC 'R'
#define RB_SET_SIZE _IOW(RB_MAGIC, 1, size_t)
#define RB_GET_SIZE _IOR(RB_MAGIC, 2, size_t)
#define RB_GET_USED _IOR(RB_MAGIC, 3, size_t)
#define RB_GET_FREE _IOR(RB_MAGIC, 4, size_t)
#define RB_CLEAR _IO(RB_MAGIC, 5)
#define RB_SET_BLOCKING _IOW(RB_MAGIC, 6, int)

// Структура статистики
struct rb_stats {
    size_t size;
    size_t used;
    size_t free;
    unsigned long writes;
    unsigned long reads;
    unsigned long overruns;
};

#define RB_GET_STATS _IOR(RB_MAGIC, 7, struct rb_stats)

#endif // RING_BUFFER_IOCTL_H
