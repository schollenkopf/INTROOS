#include <linux/init.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/math64.h>

#define WINDOW_SIZE 20
#define MAX_LEN 16
#define STD_DEV_MULTIPLIER 8  // Allowable deviations from mean

MODULE_AUTHOR("Ruan de Bruyn");
MODULE_DESCRIPTION("Driver with sliding window and separate input/output files");
MODULE_LICENSE("GPL");

static int values[WINDOW_SIZE]; // Circular buffer for the sliding window
static int window_start = 0;
static int window_count = 0;
static struct proc_dir_entry *proc_file_in;
static struct proc_dir_entry *proc_file_out;
static ktime_t start_time;
static s64 total_processing_time = 0;
static int update_count = 0;

/* Calculate mean of the values in the sliding window */
static int calculate_mean(void) {
    int sum = 0;
    for (int i = 0; i < window_count; i++) {
        sum += values[(window_start + i) % WINDOW_SIZE];
    }
    return sum / window_count;
}

/* Calculate standard deviation of the values in the sliding window */
static int calculate_std_dev(int mean) {
    int sum_sq_diff = 0;
    for (int i = 0; i < window_count; i++) {
        int value = values[(window_start + i) % WINDOW_SIZE];
        int diff = value - mean;
        sum_sq_diff += diff * diff;
    }
    return int_sqrt(sum_sq_diff / window_count);
}

/* Update the circular buffer with a new value */
static void add_value_to_window(int new_value) {
    if (window_count < WINDOW_SIZE) {
        values[window_count++] = new_value;
    } else {
        values[window_start] = new_value;
        window_start = (window_start + 1) % WINDOW_SIZE;
    }
}

/* Show function for output file to display current values in the sliding window */
static int my_proc_show_out(struct seq_file *m, void *v) {
    seq_printf(m, "Sliding window values:\n");
    for (int i = 0; i < window_count; i++) {
        seq_printf(m, "%d ", values[(window_start + i) % WINDOW_SIZE]);
    }
    seq_puts(m, "\n");
    return 0;
}

/* Open function to bind show function to output /proc file */
static int my_proc_open_out(struct inode *inode, struct file *file) {
    return single_open(file, my_proc_show_out, NULL);
}

/* Write function for input file to update the sliding window and check standard deviation */
static ssize_t my_proc_write_in(struct file *file, const char __user *buffer, size_t count, loff_t *f_pos) {
    char kbuf[MAX_LEN];
    long new_value;

    if (count > MAX_LEN - 1)
        return -EINVAL;
    start_time = ktime_get();

    if (copy_from_user(kbuf, buffer, count))
        return -EFAULT;
    
    kbuf[count] = '\0';  // Null-terminate the user input

    if (kstrtol(kbuf, 10, &new_value) < 0)
        return -EINVAL;

    if (window_count >= WINDOW_SIZE) {
        int mean = calculate_mean();
        int std_dev = calculate_std_dev(mean);
        
        /* Check if new value is within the acceptable range */
        if (new_value < mean - STD_DEV_MULTIPLIER * std_dev || new_value > mean + STD_DEV_MULTIPLIER * std_dev) {
            printk(KERN_INFO "New value %ld is outside %d +- %d.\n", new_value,mean,std_dev);
            return -EINVAL;  // Reject the value if it's out of range
        }
    }

    /* Add the new value to the sliding window */
    add_value_to_window(new_value);
    total_processing_time += ktime_to_ns(ktime_sub(ktime_get(), start_time));
    update_count++;

    return count;
}

static void print_average_time(void) {
    if (update_count > 0) {
        // Calculate the average processing time in microseconds without floating-point arithmetic
        s64 avg_time_us = total_processing_time / (update_count * 1000);
        printk(KERN_INFO "Average processing time per update: %lld microseconds\n", avg_time_us);
    }
}


/* Proc operations for the input file */
static const struct proc_ops my_proc_fops_in = {
    .proc_write = my_proc_write_in,
};

/* Proc operations for the output file */
static const struct proc_ops my_proc_fops_out = {
    .proc_open = my_proc_open_out,
    .proc_read = seq_read,
    .proc_release = single_release,
};

/* Module initialization */
static int __init my_module_init(void) {
    proc_file_in = proc_create("my_data_in", 0666, NULL, &my_proc_fops_in); // Input file
    if (!proc_file_in)
        return -ENOMEM;

    proc_file_out = proc_create("my_data_out", 0444, NULL, &my_proc_fops_out); // Output file
    if (!proc_file_out) {
        proc_remove(proc_file_in);
        return -ENOMEM;
    }

    return 0;
}

/* Module cleanup */
static void __exit my_module_exit(void) {
    print_average_time();
    proc_remove(proc_file_in);
    proc_remove(proc_file_out);
}

module_init(my_module_init);
module_exit(my_module_exit);
