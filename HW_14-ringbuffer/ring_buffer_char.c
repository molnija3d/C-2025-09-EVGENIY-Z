#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/ioctl.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/version.h>
#include <linux/moduleparam.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("EZ1337");
MODULE_DESCRIPTION("Кольцевой буфер - символьное устройство");
MODULE_VERSION("1.0");

#define DEVICE_NAME "ringbuffer"
#define CLASS_NAME "ringbuf"
#define DEFAULT_BUFFER_SIZE 4096
#define MAX_BUFFER_SIZE 65536
#define MIN_BUFFER_SIZE 256

// IOCTL команды
#define RB_MAGIC 'R'
#define RB_SET_SIZE _IOW(RB_MAGIC, 1, size_t)
#define RB_GET_SIZE _IOR(RB_MAGIC, 2, size_t)
#define RB_GET_USED _IOR(RB_MAGIC, 3, size_t)
#define RB_GET_FREE _IOR(RB_MAGIC, 4, size_t)
#define RB_CLEAR _IO(RB_MAGIC, 5)
#define RB_SET_BLOCKING _IOW(RB_MAGIC, 6, int)
#define RB_GET_STATS _IOR(RB_MAGIC, 7, struct rb_stats)

// Структура статистики
struct rb_stats {
    size_t size;
    size_t used;
    size_t free;
    unsigned long writes;
    unsigned long reads;
    unsigned long overruns;
};

// Структура кольцевого буфера
struct ring_buffer {
    char *buffer;
    size_t size;
    size_t head;
    size_t tail;
    size_t count;
    
    struct mutex lock;
    wait_queue_head_t read_queue;
    wait_queue_head_t write_queue;
    
    // Статистика
    unsigned long writes;
    unsigned long reads;
    unsigned long overruns;
    
    // Режимы работы
    int blocking;
};

// Глобальные переменные драйвера
static struct ring_buffer *rb;
static int major_number = 0;
static struct class *rb_class = NULL;
static struct device *rb_device = NULL;
static struct cdev rb_cdev;

// Прототипы функций
static int rb_open(struct inode *inode, struct file *file);
static int rb_release(struct inode *inode, struct file *file);
static ssize_t rb_read(struct file *file, char __user *buf, size_t len, loff_t *offset);
static ssize_t rb_write(struct file *file, const char __user *buf, size_t len, loff_t *offset);
static loff_t rb_llseek(struct file *file, loff_t offset, int whence);
static unsigned int rb_poll(struct file *file, poll_table *wait);
static long rb_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

// Структура file_operations
static const struct file_operations rb_fops = {
    .owner = THIS_MODULE,
    .open = rb_open,
    .release = rb_release,
    .read = rb_read,
    .write = rb_write,
    .llseek = rb_llseek,
    .poll = rb_poll,
    .unlocked_ioctl = rb_ioctl,
    .compat_ioctl = rb_ioctl,
};

// Функции кольцевого буфера
static int ring_buffer_init(struct ring_buffer *rb, size_t size)
{
    if (size < MIN_BUFFER_SIZE || size > MAX_BUFFER_SIZE)
        return -EINVAL;
    
    rb->buffer = kmalloc(size, GFP_KERNEL);
    if (!rb->buffer)
        return -ENOMEM;
    
    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    rb->blocking = 1; // По умолчанию блокирующий режим
    
    mutex_init(&rb->lock);
    init_waitqueue_head(&rb->read_queue);
    init_waitqueue_head(&rb->write_queue);
    
    rb->writes = 0;
    rb->reads = 0;
    rb->overruns = 0;
    
    return 0;
}

static void ring_buffer_free(struct ring_buffer *rb)
{
    if (rb->buffer) {
        kfree(rb->buffer);
        rb->buffer = NULL;
    }
}

static ssize_t ring_buffer_write(struct ring_buffer *rb, const char *data, size_t len)
{
    size_t free_space;
    size_t first_chunk;
    size_t second_chunk;
    ssize_t written = 0;
    
    mutex_lock(&rb->lock);
    
    free_space = rb->size - rb->count;
    
    // Если буфер полный
    if (free_space == 0) {
        rb->overruns++;
        mutex_unlock(&rb->lock);
        return -ENOSPC;
    }
    
    // Ограничиваем записываемое количество
    len = min(len, free_space);
    
    // Запись первого куска (до конца буфера)
    first_chunk = min(len, rb->size - rb->head);
    memcpy(rb->buffer + rb->head, data, first_chunk);
    
    // Если есть второй кусок (перенос через границу)
    if (len > first_chunk) {
        second_chunk = len - first_chunk;
        memcpy(rb->buffer, data + first_chunk, second_chunk);
        rb->head = second_chunk;
    } else {
        rb->head = (rb->head + len) % rb->size;
    }
    
    rb->count += len;
    rb->writes++;
    written = len;
    
    mutex_unlock(&rb->lock);
    
    // Будим читателей
    wake_up_interruptible(&rb->read_queue);
    
    return written;
}

static ssize_t ring_buffer_read(struct ring_buffer *rb, char *data, size_t len)
{
    size_t available;
    size_t first_chunk;
    size_t second_chunk;
    ssize_t read = 0;
    
    mutex_lock(&rb->lock);
    
    if (rb->count == 0) {
        mutex_unlock(&rb->lock);
        return 0;
    }
    
    available = min(len, rb->count);
    
    // Чтение первого куска (до конца буфера)
    first_chunk = min(available, rb->size - rb->tail);
    memcpy(data, rb->buffer + rb->tail, first_chunk);
    
    // Если есть второй кусок (перенос через границу)
    if (available > first_chunk) {
        second_chunk = available - first_chunk;
        memcpy(data + first_chunk, rb->buffer, second_chunk);
        rb->tail = second_chunk;
    } else {
        rb->tail = (rb->tail + available) % rb->size;
    }
    
    rb->count -= available;
    rb->reads++;
    read = available;
    
    mutex_unlock(&rb->lock);
    
    // Будим писателей
    wake_up_interruptible(&rb->write_queue);
    
    return read;
}

static void ring_buffer_clear(struct ring_buffer *rb)
{
    mutex_lock(&rb->lock);
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    mutex_unlock(&rb->lock);
    
    wake_up_interruptible(&rb->write_queue);
}

static int ring_buffer_resize(struct ring_buffer *rb, size_t new_size)
{
    char *new_buffer;
    struct ring_buffer new_rb = {0};
    
    if (new_size < MIN_BUFFER_SIZE || new_size > MAX_BUFFER_SIZE)
        return -EINVAL;
    
    new_buffer = kmalloc(new_size, GFP_KERNEL);
    if (!new_buffer)
        return -ENOMEM;
    
    mutex_lock(&rb->lock);
    
    // Инициализируем новый буфер
    new_rb.buffer = new_buffer;
    new_rb.size = new_size;
    new_rb.head = 0;
    new_rb.tail = 0;
    new_rb.count = 0;
    mutex_init(&new_rb.lock);
    new_rb.blocking = rb->blocking;
    
    // Копируем данные из старого буфера, если они есть
    if (rb->count > 0) {
        size_t to_copy = min(rb->count, new_size);
        char *temp_buf = kmalloc(to_copy, GFP_KERNEL);
        
        if (temp_buf) {
            // Читаем из старого буфера
            size_t first = min(to_copy, rb->size - rb->tail);
            memcpy(temp_buf, rb->buffer + rb->tail, first);
            
            if (to_copy > first) {
                size_t second = to_copy - first;
                memcpy(temp_buf + first, rb->buffer, second);
            }
            
            // Пишем в новый
            memcpy(new_rb.buffer, temp_buf, to_copy);
            new_rb.count = to_copy;
            new_rb.head = to_copy % new_size;
            
            kfree(temp_buf);
        }
    }
    
    // Сохраняем статистику
    new_rb.writes = rb->writes;
    new_rb.reads = rb->reads;
    new_rb.overruns = rb->overruns;
    
    // Заменяем старый буфер новым
    kfree(rb->buffer);
    *rb = new_rb;
    
    // Инициализируем очереди ожидания
    init_waitqueue_head(&rb->read_queue);
    init_waitqueue_head(&rb->write_queue);
    
    mutex_unlock(&rb->lock);
    
    // Будим всех ожидающих
    wake_up_interruptible(&rb->read_queue);
    wake_up_interruptible(&rb->write_queue);
    
    return 0;
}

// Функции драйвера
static int rb_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "ringbuffer: устройство открыто\n");
    return 0;
}

static int rb_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "ringbuffer: устройство закрыто\n");
    return 0;
}

static ssize_t rb_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
    ssize_t bytes_read;
    char *kbuf;
    int ret;
    
    if (!buf)
        return -EFAULT;
    
    // Выделяем временный буфер в ядре
    kbuf = kmalloc(len, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;
    
    if (rb->blocking) {
        // Блокирующий режим: ждем данных
        ret = wait_event_interruptible(rb->read_queue, rb->count > 0);
        if (ret)
            return ret;
    } else {
        // Неблокирующий режим
        if (rb->count == 0)
            return -EAGAIN;
    }
    
    bytes_read = ring_buffer_read(rb, kbuf, len);
    
    if (bytes_read > 0) {
        // Копируем в пользовательское пространство
        if (copy_to_user(buf, kbuf, bytes_read)) {
            kfree(kbuf);
            return -EFAULT;
        }
    }
    
    kfree(kbuf);
    return bytes_read;
}

static ssize_t rb_write(struct file *file, const char __user *buf, size_t len, loff_t *offset)
{
    ssize_t bytes_written;
    char *kbuf;
    int ret;
    
    if (!buf)
        return -EFAULT;
    
    // Выделяем временный буфер в ядре
    kbuf = kmalloc(len, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;
    
    // Копируем из пользовательского пространства
    if (copy_from_user(kbuf, buf, len)) {
        kfree(kbuf);
        return -EFAULT;
    }
    
    if (rb->blocking) {
        // Блокирующий режим: ждем свободного места
        ret = wait_event_interruptible(rb->write_queue, 
                                      (rb->size - rb->count) >= len);
        if (ret) {
            kfree(kbuf);
            return ret;
        }
    }
    
    bytes_written = ring_buffer_write(rb, kbuf, len);
    
    kfree(kbuf);
    return bytes_written;
}

static loff_t rb_llseek(struct file *file, loff_t offset, int whence)
{
    // Не поддерживаем seek для кольцевого буфера
    return -ESPIPE;
}

static unsigned int rb_poll(struct file *file, poll_table *wait)
{
    unsigned int mask = 0;
    
    poll_wait(file, &rb->read_queue, wait);
    poll_wait(file, &rb->write_queue, wait);
    
    mutex_lock(&rb->lock);
    
    if (rb->count > 0)
        mask |= POLLIN | POLLRDNORM;  // Можно читать
    
    if (rb->count < rb->size)
        mask |= POLLOUT | POLLWRNORM; // Можно писать
    
    mutex_unlock(&rb->lock);
    
    return mask;
}

static long rb_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    size_t tmp;
    struct rb_stats stats;
    int blocking;
    
    switch (cmd) {
        case RB_SET_SIZE:
            if (copy_from_user(&tmp, (size_t __user *)arg, sizeof(size_t)))
                return -EFAULT;
            
            ret = ring_buffer_resize(rb, tmp);
            if (ret)
                return ret;
            
            printk(KERN_INFO "ringbuffer: размер изменен на %zu байт\n", tmp);
            break;
            
        case RB_GET_SIZE:
            tmp = rb->size;
            if (copy_to_user((size_t __user *)arg, &tmp, sizeof(size_t)))
                return -EFAULT;
            break;
            
        case RB_GET_USED:
            mutex_lock(&rb->lock);
            tmp = rb->count;
            mutex_unlock(&rb->lock);
            
            if (copy_to_user((size_t __user *)arg, &tmp, sizeof(size_t)))
                return -EFAULT;
            break;
            
        case RB_GET_FREE:
            mutex_lock(&rb->lock);
            tmp = rb->size - rb->count;
            mutex_unlock(&rb->lock);
            
            if (copy_to_user((size_t __user *)arg, &tmp, sizeof(size_t)))
                return -EFAULT;
            break;
            
        case RB_CLEAR:
            ring_buffer_clear(rb);
            printk(KERN_INFO "ringbuffer: буфер очищен\n");
            break;
            
        case RB_SET_BLOCKING:
            if (copy_from_user(&blocking, (int __user *)arg, sizeof(int)))
                return -EFAULT;
            
            mutex_lock(&rb->lock);
            rb->blocking = blocking ? 1 : 0;
            mutex_unlock(&rb->lock);
            
            printk(KERN_INFO "ringbuffer: режим %s\n", 
                   rb->blocking ? "блокирующий" : "неблокирующий");
            break;
            
        case RB_GET_STATS:
            mutex_lock(&rb->lock);
            stats.size = rb->size;
            stats.used = rb->count;
            stats.free = rb->size - rb->count;
            stats.writes = rb->writes;
            stats.reads = rb->reads;
            stats.overruns = rb->overruns;
            mutex_unlock(&rb->lock);
            
            if (copy_to_user((struct rb_stats __user *)arg, &stats, sizeof(stats)))
                return -EFAULT;
            break;
            
        default:
            return -ENOTTY;
    }
    
    return ret;
}

// Инициализация и очистка модуля
static int __init ring_buffer_init_module(void)
{
    dev_t dev_num = 0;
    int ret;
    
    printk(KERN_INFO "Инициализация драйвера кольцевого буфера\n");
    
    // Выделяем память для структуры буфера
    rb = kmalloc(sizeof(struct ring_buffer), GFP_KERNEL);
    if (!rb) {
        printk(KERN_ERR "Не удалось выделить память для ring_buffer\n");
        return -ENOMEM;
    }
    
    // Инициализируем буфер
    ret = ring_buffer_init(rb, DEFAULT_BUFFER_SIZE);
    if (ret < 0) {
        printk(KERN_ERR "Не удалось инициализировать ring_buffer\n");
        kfree(rb);
        return ret;
    }
    
    // Динамически выделяем major number
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ERR "Не удалось выделить номер устройства\n");
        ring_buffer_free(rb);
        kfree(rb);
        return ret;
    }
    major_number = MAJOR(dev_num);
    
    // Инициализируем cdev
    cdev_init(&rb_cdev, &rb_fops);
    rb_cdev.owner = THIS_MODULE;
    
    // Добавляем cdev в систему
    ret = cdev_add(&rb_cdev, dev_num, 1);
    if (ret < 0) {
        printk(KERN_ERR "Не удалось добавить cdev\n");
        unregister_chrdev_region(dev_num, 1);
        ring_buffer_free(rb);
        kfree(rb);
        return ret;
    }
    
        rb_class = class_create(CLASS_NAME);
    
    if (IS_ERR(rb_class)) {
        printk(KERN_ERR "Не удалось создать класс устройства\n");
        cdev_del(&rb_cdev);
        unregister_chrdev_region(dev_num, 1);
        ring_buffer_free(rb);
        kfree(rb);
        return PTR_ERR(rb_class);
    }
    
    // Создаем устройство
    rb_device = device_create(rb_class, NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(rb_device)) {
        printk(KERN_ERR "Не удалось создать устройство\n");
        
        // Исправляем для разных версий
        #if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
            class_destroy(rb_class);
        #else
            // В старых ядрах class_destroy принимает один аргумент
            class_destroy(rb_class);
        #endif
        
        cdev_del(&rb_cdev);
        unregister_chrdev_region(dev_num, 1);
        ring_buffer_free(rb);
        kfree(rb);
        return PTR_ERR(rb_device);
    }
    
    printk(KERN_INFO "Драйвер кольцевого буфера успешно загружен\n");
    printk(KERN_INFO "Устройство: /dev/%s\n", DEVICE_NAME);
    printk(KERN_INFO "Major number: %d\n", major_number);
    printk(KERN_INFO "Размер буфера: %zu байт\n", rb->size);
    
    return 0;
}

static void __exit ring_buffer_exit_module(void)
{
    dev_t dev_num = MKDEV(major_number, 0);
    
    printk(KERN_INFO "Выгрузка драйвера кольцевого буфера\n");
    
    // Удаляем устройство (универсальный вызов)
    if (rb_device)
        device_destroy(rb_class, dev_num);
    
    // Удаляем класс (с учетом версии ядра)
    if (rb_class) {
        #if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
            class_destroy(rb_class);
        #else
            class_destroy(rb_class);
        #endif
    }
    
    // Удаляем cdev
    cdev_del(&rb_cdev);
    
    // Освобождаем номер устройства
    unregister_chrdev_region(dev_num, 1);
    
    // Освобождаем память буфера
    ring_buffer_free(rb);
    kfree(rb);
    
    printk(KERN_INFO "Драйвер кольцевого буфера выгружен\n");
}

module_init(ring_buffer_init_module);
module_exit(ring_buffer_exit_module);
