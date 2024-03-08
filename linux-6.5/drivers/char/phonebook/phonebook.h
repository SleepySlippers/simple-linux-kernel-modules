#define MAX_USER_DATA_LEN ((size_t)32)

struct user_data {
    char surname[MAX_USER_DATA_LEN];
    char name[MAX_USER_DATA_LEN];
    int age;
    char phone_number[MAX_USER_DATA_LEN];
    char email[MAX_USER_DATA_LEN];
};