#define TOP_N 10
#define HASH_TABLE_SIZE 2048
#define PATH_LENGTH 1024
#define HASH_BASE 5381

#define SKIP_SPACES(p) while(*(p) && isspace((unsigned char)*(p))){(p)++;}
#define SKIP_CHARS(p)  while(*(p) && !isspace((unsigned char)*(p))){(p)++;}

typedef struct UrlNode {
    char* url;
    long long total_bytes;
    struct UrlNode* next;
} UrlNode;

typedef struct {
    UrlNode** buckets;
    int size;
} UrlHashTable;

typedef struct RefererNode {
    char* referer;
    int count;
    struct RefererNode* next;
} RefererNode;

typedef struct {
    RefererNode** buckets;
    int size;
} RefererHashTable;

typedef struct {
    char** files;
    int* next_file_index;
    int total_files;
    pthread_mutex_t* file_mutex;
    
    UrlHashTable* url_table;
    RefererHashTable* referer_table;
    long long thread_total_bytes;
} ThreadData;
