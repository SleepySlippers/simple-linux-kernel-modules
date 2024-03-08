#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include "phonebook.h"

MODULE_LICENSE("MIT");
MODULE_AUTHOR("arseny staroverov.ag@phystech.ru");
MODULE_DESCRIPTION("");
MODULE_VERSION("0.0");

#define MY_MAJOR  266 // hardcoded to it will be possible to insert `mknod -m 666 /dev/phonebook c 266 0` into `init` file into `initrd`
#define MY_MINOR  0
#define MY_DEV_COUNT 1

static dev_t devno = MKDEV(MY_MAJOR, MY_MINOR);

struct cdev cdev;

static struct class *phonebook_class;

struct device *phone_book_dev;

#define PHONEBOOK_MODULE_NAME ("phonebook")

struct User {
    struct user_data data;
    struct list_head list;
};

static LIST_HEAD(users_list);

long __get_user_from_phonebook(const char* surname, unsigned int len, struct user_data * output_data) {
    if (len >= MAX_USER_DATA_LEN) {
        return -EINVAL;
    }
    char found = 0;
    struct list_head *it;
    list_for_each(it, &users_list) {
        struct User* cur = list_entry(it, struct User, list);
        if (strncmp(cur->data.surname, surname, len) == 0) {
            if (strnlen(cur->data.surname, MAX_USER_DATA_LEN) == len) {
                memcpy(output_data, &cur->data, sizeof(struct user_data));
                found = 1;
            }
        }
    }
    if (found) {
        return 0;
    }
    return 1;
}


SYSCALL_DEFINE3(get_user_from_phonebook, const char __user*, surname, unsigned int, len, struct user_data __user *, output_data) {
    if (len >= MAX_USER_DATA_LEN) {
        return -EINVAL;
    }
    char tmp_surname[MAX_USER_DATA_LEN];
    if (copy_from_user(tmp_surname, surname, len)) {
        return -EFAULT;
    }
    struct user_data tmp;
    long err = __get_user_from_phonebook(tmp_surname, len, &tmp);
    if (err != 0)  {
        return err;
    }
    return copy_to_user(output_data, &tmp, sizeof(struct user_data));
}

long __add_user_to_phonebook(const struct user_data* input_data){
    struct User * new_user = kmalloc(sizeof(struct User), GFP_KERNEL);
    memcpy(&new_user->data, input_data, sizeof(struct user_data));
    list_add(&new_user->list, &users_list);
    return 0;
}

SYSCALL_DEFINE1(add_user_to_phonebook, const struct user_data __user *, input_data) {
    struct user_data tmp;
    if (copy_from_user(&tmp, input_data, sizeof(tmp))) {
        return -EFAULT;
    }
    return __add_user_to_phonebook(&tmp);
}

long __del_user_from_phonebook(const char * surname, unsigned int len) {
    struct User *pos, *temp;
    list_for_each_entry_safe(pos, temp, &users_list, list) {
        if (strncmp(pos->data.surname, surname, len) == 0) {
            list_del(&pos->list);
            kfree(pos); // Free memory if needed
        }
    }
    return 0;
}

SYSCALL_DEFINE2(del_user_from_phonebook, const char __user *, surname, unsigned int, len) {
    if (len >= MAX_USER_DATA_LEN) {
        return -EINVAL;
    }
    char tmp_surname[MAX_USER_DATA_LEN];
    if (copy_from_user(tmp_surname, surname, len)){
        return -EFAULT;
    }
    return __del_user_from_phonebook(tmp_surname, len);
}

static char* answers = NULL;
static size_t answers_len = 0;

static DEFINE_MUTEX(device_mutex);

static DEFINE_MUTEX(read_write_mutex);

static int device_open(struct inode *inode, struct file *filp) {
    // To get rid of all concurrency problems
    if (!mutex_trylock(&device_mutex)) {
        printk(KERN_ALERT "Device is already open by another process\n");
        return -EBUSY; // Return busy error code if device is already open
    }
    mutex_unlock(&read_write_mutex); // allow to read and write
    return 0; // Return success
}

static int device_release(struct inode *inode, struct file *filp)
{
    mutex_lock(&read_write_mutex); // forbid to read and write
    kfree(answers);
    answers = NULL;
    answers_len = 0;
    mutex_unlock(&device_mutex);
    return 0;
}

static char* queries = NULL;
static size_t queries_len = 0;
static size_t to_handle_ind = 0;

static ssize_t __device_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
    char * new_queries = krealloc(queries, queries_len + count, GFP_KERNEL);
    if (!new_queries) {
        return -ENOMEM;
    }
    queries = new_queries;
    int err = copy_from_user(queries + queries_len, buf, count);
    if (err) {
        return -EFAULT;
    }
    queries_len += count;
    return count;

    // size_t maxdatalen = MAX_USER_DATA_LEN, ncopied;
    // uint8_t databuf[MAX_USER_DATA_LEN+1];

    // if (count < maxdatalen) {
    //     maxdatalen = count;
    // }

    // ncopied = copy_from_user(databuf, buf, maxdatalen);

    // if (ncopied == 0) {
    //     printk("Copied %zd bytes from the user\n", maxdatalen);
    // } else {
    //     printk("Could't copy %zd bytes from the user\n", ncopied);
    // }

    // databuf[maxdatalen] = 0;

    // printk("Data from the user: %s\n", databuf);

    // return count;
}

static ssize_t device_write(struct file *file, const char __user *buf, size_t count, loff_t *offset){
    // To get rid of all concurrency problems
    if (!mutex_trylock(&read_write_mutex)) {
        return -EBUSY;
    }
    ssize_t res = __device_write(file, buf, count, offset);
    mutex_unlock(&read_write_mutex);
    return res;
}

static char is_space(char x) {
    return (x == ' ') || (x == '\n');
}

static long append_to_str(char **dest, size_t *dest_len_ptr, const char *str, int len) {
    char * new_dest = krealloc(*dest, *dest_len_ptr + len, GFP_KERNEL);
    if (!new_dest) {
        return -ENOMEM;
    }
    *dest = new_dest;
    memcpy(*dest + *dest_len_ptr, str, len);
    *dest_len_ptr += len;
    return 0;
}

static void append_user_data_to_answers(struct user_data data) {
    char * user_str_repr = NULL;
    size_t user_str_len = 0;

    {
        const char* str_to_append = data.surname;
        size_t str_len = strnlen(str_to_append, MAX_USER_DATA_LEN);
        if (append_to_str(&user_str_repr, &user_str_len, str_to_append, str_len)) {
            goto free_user_str_repr;
        }
        if (append_to_str(&user_str_repr, &user_str_len, " ", 1)) {
            goto free_user_str_repr;
        }
    }
    {
        const char* str_to_append = data.name;
        size_t str_len = strnlen(str_to_append, MAX_USER_DATA_LEN);
        if (append_to_str(&user_str_repr, &user_str_len, str_to_append, str_len)) {
            goto free_user_str_repr;
        }
        if (append_to_str(&user_str_repr, &user_str_len, " ", 1)) {
            goto free_user_str_repr;
        }
    }
    {
        if (data.age >= 1000) {
            goto free_user_str_repr;
        }
        char age[] = "   ";
        if (data.age >= 100) {
            age[0] = '0' + data.age / 100 % 10;
        }
        if (data.age >= 10) {
            age[1] = '0' + data.age / 10 % 10;
        }
        age[2] = '0' + data.age % 10;
        const char* str_to_append = age;
        size_t str_len = 3;
        if (append_to_str(&user_str_repr, &user_str_len, str_to_append, str_len)) {
            goto free_user_str_repr;
        }
        if (append_to_str(&user_str_repr, &user_str_len, " ", 1)) {
            goto free_user_str_repr;
        }
    }
    {
        const char* str_to_append = data.phone_number;
        size_t str_len = strnlen(str_to_append, MAX_USER_DATA_LEN);
        if (append_to_str(&user_str_repr, &user_str_len, str_to_append, str_len)) {
            goto free_user_str_repr;
        }
        if (append_to_str(&user_str_repr, &user_str_len, " ", 1)) {
            goto free_user_str_repr;
        }
    }
    {
        const char* str_to_append = data.email;
        size_t str_len = strnlen(str_to_append, MAX_USER_DATA_LEN);
        if (append_to_str(&user_str_repr, &user_str_len, str_to_append, str_len)) {
            goto free_user_str_repr;
        }
        if (append_to_str(&user_str_repr, &user_str_len, "\n", 1)) {
            goto free_user_str_repr;
        }
    }

    append_to_str(&answers, &answers_len, user_str_repr, user_str_len);

free_user_str_repr:
    kfree(user_str_repr);
    return;
}

void query_substring_without_spaces(size_t* start_ind, size_t* end_ind) {
    while (*start_ind < queries_len && is_space(queries[*start_ind])) {
        ++*start_ind;
    }
    if (*start_ind >= queries_len) {
        *end_ind = queries_len;
        return;
    }
    *end_ind = *start_ind;
    while (*end_ind < queries_len && !is_space(queries[*end_ind])) {
        ++*end_ind;
    }
    return;
}

static void handle_query(void) {
    // carefully handle query
    // учитываем что запросы могли прийти частями и запрос мог быть обрезан посередине

    if (to_handle_ind >= queries_len) {
        return;
    }
    size_t ind = to_handle_ind;

    char operation = '\0';

    while (ind < queries_len) {
        // space must follow the operation
        if (ind + 1 >= queries_len || !is_space(queries[ind + 1])) {    
            ++ind;
            continue;
        }
        char tmp = queries[ind];
        if (tmp == '+' || tmp == '-' || tmp == '?') {
            operation = tmp;
            to_handle_ind = ind; 
            ind += 2; // go over operation and a space
            break;
        }
        ++ind;
    }

    size_t surname_start = ind;
    size_t surname_end = surname_start;
    query_substring_without_spaces(&surname_start, &surname_end);
    if (surname_end >= queries_len) {
        // it means there is no delimiter as `\n` or ` ` met so it means query is not finished
        return;
    }

    if (operation == '-') {
        __del_user_from_phonebook(queries + surname_start, surname_end - surname_start);
        to_handle_ind = surname_end;
        return;
    }
    if (operation == '?') {
        struct user_data tmp;
        int err = __get_user_from_phonebook(queries + surname_start, surname_end - surname_start, &tmp);
        if (err != 0) {
            // user is not found or something bad happened anyway we handled this query :)
            to_handle_ind = surname_end;
            return;
        }
        append_user_data_to_answers(tmp);
        to_handle_ind = surname_end;
        return;
    }

    if (operation != '+') {
        return;
    }

    size_t name_start = surname_end;
    size_t name_end = name_start;
    query_substring_without_spaces(&name_start, &name_end);

    size_t age_start = name_end;
    size_t age_end = age_start;
    query_substring_without_spaces(&age_start, &age_end);

    size_t phone_start = age_end;
    size_t phone_end = phone_start;
    query_substring_without_spaces(&phone_start, &phone_end);

    size_t email_start = phone_end;
    size_t email_end = email_start;
    query_substring_without_spaces(&email_start, &email_end);
    

    if (email_end >= queries_len) {
        return;
    }

    if (age_end - age_start > 3) {
        to_handle_ind = email_end;
        return;
    }
    size_t age_ind = age_end - 1;
    int deg = 1;
    int int_age = 0;
    while (age_ind >= age_start) {
        char tmp = queries[age_ind];
        if (tmp > '9' || tmp < '0') {
            to_handle_ind = email_end;
            return;
        }
        int_age += deg * (tmp - '0');
        deg *= 10;
        --age_ind;
    }

    size_t surname_len = min(surname_end - surname_start, MAX_USER_DATA_LEN - 1);
    size_t name_len    = min(name_end - name_start, MAX_USER_DATA_LEN - 1);
    size_t phone_len   = min(phone_end - phone_start, MAX_USER_DATA_LEN - 1);
    size_t email_len   = min(email_end - email_start, MAX_USER_DATA_LEN - 1);

    struct user_data new_user;
    memset(&new_user, 0, sizeof(struct user_data));
    new_user.age = int_age;
    memcpy(new_user.surname, queries + surname_start, surname_len);
    memcpy(new_user.name, queries + name_start, name_len);
    memcpy(new_user.phone_number, queries + phone_start, phone_len);
    memcpy(new_user.email, queries + email_start, email_len);

    __add_user_to_phonebook(&new_user);

    printk(KERN_INFO "user added: surname: '%.*s', name: '%.*s', age: %d, phone: '%.*s', email '%.*s'\n", 
        (int)surname_len, queries + surname_start,
        (int)name_len, queries + name_start,
        int_age,
        (int)phone_len, queries + phone_start,
        (int)email_len, queries + email_start);

    to_handle_ind = email_end;

    return;
}

static ssize_t __device_read(struct file *file, char __user *buf, size_t count, loff_t *offset) {
    size_t prev = to_handle_ind;
    do {
        prev = to_handle_ind;
        handle_query();
    } while(prev != to_handle_ind);

    if (answers_len < *offset) {
        pr_err("phonebook fiasco: offset overrun the content");
        return 0;
    }

    char *answers_it = answers;
    answers_it += *offset;
    count = min(count, (size_t)(answers_len - *offset));

    if (copy_to_user(buf, answers_it, count)) {
        return -EFAULT;
    }

    *offset += count;

    return count;
}

static ssize_t device_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
    // To get rid of all concurrency problems
    if (!mutex_trylock(&read_write_mutex)) {
        return -EBUSY;
    }
    ssize_t res = __device_read(file, buf, count, offset);
    mutex_unlock(&read_write_mutex);
    return res;
}

struct file_operations phonebook_fops = {
    .owner =   THIS_MODULE,
    .open = device_open,
    .release = device_release,  
    .read = device_read,
    .write = device_write,
};

static int phonebook_uevent(const struct device *dev, struct kobj_uevent_env *env)
{
    printk(KERN_INFO "add uevent var");
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

static int __init phonebook_init(void) {


    printk(KERN_INFO "Phonebook started\n");

    mutex_lock(&read_write_mutex); // forbid to read and write it will be unlocked when device is open

    int rc = -EINVAL;

    rc = register_chrdev_region(devno, MY_DEV_COUNT, PHONEBOOK_MODULE_NAME);
    
    // rc = alloc_chrdev_region(&devno, 0, 1, PHONEBOOK_MODULE_NAME); // uncomment this to create device dynamically but it will require to user write `mknod` before usage
    if (rc) {
		pr_err("Unable to create phonebook chrdev: %i\n", rc);
		return rc;
	}
    printk(KERN_INFO "region allocated\n");

    phonebook_class = class_create(PHONEBOOK_MODULE_NAME);
    phonebook_class->dev_uevent = phonebook_uevent;

    cdev_init(&cdev, &phonebook_fops);

    rc = cdev_add(&cdev, devno, 1);
	if (rc) {
		pr_err("cdev_add() failed %d\n", rc);
        return rc;
	}
    printk(KERN_INFO "cdev_added\n");

    phone_book_dev = device_create(phonebook_class, NULL, devno, NULL,
				    "%s-%d", PHONEBOOK_MODULE_NAME, 0);

    if (IS_ERR(phone_book_dev)) {
		rc = PTR_ERR(phone_book_dev);
		pr_err("Unable to create coproc-%d %d\n", MINOR(devno), rc);
        return rc;
	}

    printk(KERN_INFO "device created\n");

    return 0;
}

static void __exit phonebook_exit(void) {
    struct User *pos, *temp;
    list_for_each_entry_safe(pos, temp, &users_list, list) {
        list_del(&pos->list);
        kfree(pos); // Free memory if needed
    }
    kfree(answers);
    printk(KERN_INFO "Phonebook exited\n");
}

module_init(phonebook_init);
module_exit(phonebook_exit);