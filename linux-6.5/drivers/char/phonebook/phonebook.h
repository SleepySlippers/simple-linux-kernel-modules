#include <linux/types.h>

#define MAX_USER_DATA_LEN ((size_t)32)

struct user_data {
  char surname[MAX_USER_DATA_LEN];
  char name[MAX_USER_DATA_LEN];
  int age;
  char phone_number[MAX_USER_DATA_LEN];
  char email[MAX_USER_DATA_LEN];
};

long __get_user_from_phonebook(const char *surname, unsigned int len,
                               struct user_data *output_data);
long __add_user_to_phonebook(const struct user_data *input_data);
long __del_user_from_phonebook(const char *surname, unsigned int len);
