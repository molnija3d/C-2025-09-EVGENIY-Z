#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <dirent.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include "parser.h"

long long total_bytes = 0;
UrlHashTable* global_url_table = NULL;
RefererHashTable* global_referer_table = NULL;
pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;
/*
 * Calculate hash of the string
 */
static inline unsigned long hash_string(const char* str) {
    unsigned long hash = 5381;
    unsigned char c;
    
    while ((c = (unsigned char)*str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    
    return hash;
}
/*
 * Creating hash table for url
 */
UrlHashTable* create_url_table(int size) {
    UrlHashTable* table = malloc(sizeof(UrlHashTable));
    if (!table) return NULL;
    
    table->size = size;
    table->buckets = calloc(size, sizeof(UrlNode*));
    if (!table->buckets) {
        free(table);
        return NULL;
    }
    
    return table;
}
/*
 * Creating hash table for referer
 */
RefererHashTable* create_referer_table(int size) {
    RefererHashTable* table = malloc(sizeof(RefererHashTable));
    if (!table) return NULL;
    
    table->size = size;
    table->buckets = calloc(size, sizeof(RefererNode*));
    if (!table->buckets) {
        free(table);
        return NULL;
    }
    
    return table;
}

/*
 * Update url table from thread
 */
static inline void update_url_table(UrlHashTable* table, const char* url, long long bytes) {
    if (!url || !url[0]) return;
    
    unsigned long hash = hash_string(url) % table->size;
    UrlNode* current = table->buckets[hash];
    
    while (current) {
        if (strcmp(current->url, url) == 0) {
            current->total_bytes += bytes;
            return;
        }
        current = current->next;
    }
    
    UrlNode* new_node = malloc(sizeof(UrlNode));
    if (!new_node) return;
    
    new_node->url = strdup(url);
    new_node->total_bytes = bytes;
    new_node->next = table->buckets[hash];
    table->buckets[hash] = new_node;
}

/*
 * Update referer data from thread
 */
static inline void update_referer_table(RefererHashTable* table, const char* referer) {
    if (!referer || !referer[0] || strcmp(referer, "-") == 0) return;
    
    unsigned long hash = hash_string(referer) % table->size;
    RefererNode* current = table->buckets[hash];
    
    while (current) {
        if (strcmp(current->referer, referer) == 0) {
            current->count++;
            return;
        }
        current = current->next;
    }
    
    RefererNode* new_node = malloc(sizeof(RefererNode));
    if (!new_node) return;
    
    new_node->referer = strdup(referer);
    new_node->count = 1;
    new_node->next = table->buckets[hash];
    table->buckets[hash] = new_node;
}

/*
 * Merge one url table with another
 */
void merge_url_tables(UrlHashTable* src, UrlHashTable* dst) {
    for (int i = 0; i < src->size; i++) {
        UrlNode* current = src->buckets[i];
        while (current) {
            update_url_table(dst, current->url, current->total_bytes);
            current = current->next;
        }
    }
}

/*
 * Merge one referer table with another
 */
void merge_referer_tables(RefererHashTable* src, RefererHashTable* dst) {
    for (int i = 0; i < src->size; i++) {
        RefererNode* current = src->buckets[i];
        while (current) {
            for (int j = 0; j < current->count; j++) {
                update_referer_table(dst, current->referer);
            }
            current = current->next;
        }
    }
}

/*
 * free url table after usage
 */
void free_url_table(UrlHashTable* table) {
    if (!table) return;
    
    for (int i = 0; i < table->size; i++) {
        UrlNode* current = table->buckets[i];
        while (current) {
            UrlNode* next = current->next;
            free(current->url);
            free(current);
            current = next;
        }
    }
    
    free(table->buckets);
    free(table);
}

/*
 * free referer table after usage
 */
void free_referer_table(RefererHashTable* table) {
    if (!table) return;
    
    for (int i = 0; i < table->size; i++) {
        RefererNode* current = table->buckets[i];
        while (current) {
            RefererNode* next = current->next;
            free(current->referer);
            free(current);
            current = next;
        }
    }
    
    free(table->buckets);
    free(table);
}

/*
 * To sort results fast  we need to convert table to array array of pointers
 */
UrlNode** url_table_to_array(UrlHashTable* table, int* count) {
    *count = 0;
    for (int i = 0; i < table->size; i++) {
        UrlNode* current = table->buckets[i];
        while (current) {
            (*count)++;
            current = current->next;
        }
    }
    
    UrlNode** array = malloc(*count * sizeof(UrlNode*));
    if (!array) return NULL;
    
    int index = 0;
    for (int i = 0; i < table->size; i++) {
        UrlNode* current = table->buckets[i];
        while (current) {
            array[index++] = current;
            current = current->next;
        }
    }
    
    return array;
}

/*
 * To sort results fast  we need to convert table to array of pointers
 */
RefererNode** referer_table_to_array(RefererHashTable* table, int* count) {
    *count = 0;
    for (int i = 0; i < table->size; i++) {
        RefererNode* current = table->buckets[i];
        while (current) {
            (*count)++;
            current = current->next;
        }
    }
    
    RefererNode** array = malloc(*count * sizeof(RefererNode*));
    if (!array) return NULL;
    
    int index = 0;
    for (int i = 0; i < table->size; i++) {
        RefererNode* current = table->buckets[i];
        while (current) {
            array[index++] = current;
            current = current->next;
        }
    }
    
    return array;
}

/*
 * a function to compare urls with qsort
 */
int compare_urls(const void* a, const void* b) {
    const UrlNode* u1 = *(const UrlNode**)a;
    const UrlNode* u2 = *(const UrlNode**)b;
    
    if (u2->total_bytes > u1->total_bytes) return 1;
    if (u2->total_bytes < u1->total_bytes) return -1;
    return 0;
}

/*
 * a function to compare urls with qsort
 */
int compare_referers(const void* a, const void* b) {
    const RefererNode* r1 = *(const RefererNode**)a;
    const RefererNode* r2 = *(const RefererNode**)b;
    
    if (r2->count > r1->count) return 1;
    if (r2->count < r1->count) return -1;
    return 0;
}

/*
 * function to parse a one log line
 */
int parse_log_line(char* line, char** url, long long* bytes, char** referer) {
    char* p = line;
    
    while (*p && !isspace((unsigned char)*p)) p++;
    if (!*p) return 0;
    
    while (*p && isspace((unsigned char)*p)) p++;
    
    while (*p && !isspace((unsigned char)*p)) p++;
    if (!*p) return 0;
    
    while (*p && isspace((unsigned char)*p)) p++;
    
    while (*p && !isspace((unsigned char)*p)) p++;
    if (!*p) return 0;
    
    while (*p && isspace((unsigned char)*p)) p++;
    
    if (*p != '[') return 0;
    p = strchr(p, ']');
    if (!p) return 0;
    p++;
    
    while (*p && isspace((unsigned char)*p)) p++;
    
    if (*p != '"') return 0;
    p++;
    
    while (*p && !isspace((unsigned char)*p)) p++;
    if (!*p) return 0;
    
    while (*p && isspace((unsigned char)*p)) p++;
    
    char* url_start = p;
    while (*p && !isspace((unsigned char)*p) && *p != '"') p++;
    if (p == url_start) return 0;
    
    size_t url_len = p - url_start;
    *url = malloc(url_len + 1);
    if (!*url) return 0;
    strncpy(*url, url_start, url_len);
    (*url)[url_len] = '\0';
    
    while (*p && *p != '"') p++;
    if (*p != '"') {
        free(*url);
        *url = NULL;
        return 0;
    }
    p++; 
    
    while (*p && isspace((unsigned char)*p)) p++;
    
    while (*p && !isspace((unsigned char)*p)) p++;
    if (!*p) {
        free(*url);
        *url = NULL;
        return 0;
    }
    
    while (*p && isspace((unsigned char)*p)) p++;
    
    char* bytes_start = p;
    while (*p && !isspace((unsigned char)*p)) p++;
    if (p == bytes_start) {
        free(*url);
        *url = NULL;
        return 0;
    }
    
    char bytes_str[32];
    size_t bytes_len = p - bytes_start;
    if (bytes_len >= sizeof(bytes_str)) bytes_len = sizeof(bytes_str) - 1;
    strncpy(bytes_str, bytes_start, bytes_len);
    bytes_str[bytes_len] = '\0';
    *bytes = atoll(bytes_str);
    
    while (*p && isspace((unsigned char)*p)) p++;
    
    if (*p != '"') {
        *referer = NULL;
        return 1;
    }
    p++;
    
    char* ref_start = p;
    while (*p && *p != '"') p++;
    if (*p != '"') {
        free(*url);
        *url = NULL;
        return 0;
    }
    
    size_t ref_len = p - ref_start;
    if (ref_len > 0) {
        int is_dash = 1;
        for (size_t i = 0; i < ref_len; i++) {
            if (ref_start[i] != '-') {
                is_dash = 0;
                break;
            }
        }
        
        if (!is_dash) {
            *referer = malloc(ref_len + 1);
            if (*referer) {
                strncpy(*referer, ref_start, ref_len);
                (*referer)[ref_len] = '\0';
            }
        } else {
            *referer = NULL;
        }
    } else {
        *referer = NULL;
    }
    
    return 1;
}

/*
 * Processing whole file
 */
void process_file(const char* filename, ThreadData* data) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Warning: Cannot open file %s: %s\n", filename, strerror(errno));
        return;
    }
    
    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    
    while ((read = getline(&line, &len, file)) != -1) {
        if (read > 0 && line[read - 1] == '\n') {
            line[read - 1] = '\0';
        }
        
        if (read <= 1) continue;
        
        char* url = NULL;
        char* referer = NULL;
        long long bytes = 0;
        
        if (parse_log_line(line, &url, &bytes, &referer)) {
            data->thread_total_bytes += bytes;
            
            if (url && url[0]) {
                update_url_table(data->url_table, url, bytes);
                free(url);
            }
            
            if (referer && referer[0]) {
                update_referer_table(data->referer_table, referer);
                free(referer);
            }
        } else {
            static int debug_count = 0;
            if (debug_count < 5) {
                fprintf(stderr, "Failed to parse line: %s\n", line);
                debug_count++;
            }
        }
    }
    
    free(line);
    fclose(file);
}

/*
 * Thread function. There is a one thread for one file. If there is not anough files, then thread exit.
 */
void* thread_func(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    
    data->url_table = create_url_table(HASH_TABLE_SIZE);
    data->referer_table = create_referer_table(HASH_TABLE_SIZE);
    data->thread_total_bytes = 0;
    
    if (!data->url_table || !data->referer_table) {
        fprintf(stderr, "Memory allocation failed in thread\n");
        pthread_exit(NULL);
    }
    
    while (1) {
         /* locking a mutex to get access to file counter (index) */
        pthread_mutex_lock(data->file_mutex);
        int file_index = *(data->next_file_index);
        if (file_index < data->total_files) {
            /* udating index*/
            *(data->next_file_index) = file_index + 1;
            char* filename = data->files[file_index];
            /* now other process can use mutex and change counter */
            pthread_mutex_unlock(data->file_mutex);
            
            process_file(filename, data);
        } else {
            /* if there is no any files awailable */
            pthread_mutex_unlock(data->file_mutex);
            break;
        }
    }
    /* to merge global table with local we need to use mutex */
    pthread_mutex_lock(&stats_mutex);
    total_bytes += data->thread_total_bytes;
    merge_url_tables(data->url_table, global_url_table);
    merge_referer_tables(data->referer_table, global_referer_table);
    /* merging complete, unlock mutex to allow other processes handle it*/
    pthread_mutex_unlock(&stats_mutex);
    
    free_url_table(data->url_table);
    free_referer_table(data->referer_table);
    
    pthread_exit(NULL);
}

int find_files(const char* log_dir, char*** files_ptr){
    DIR* dir = opendir(log_dir);
    if (!dir) {
        fprintf(stderr, "Cannot open directory %s: %s\n", log_dir, strerror(errno));
        return 1;
    }
    
    struct dirent* entry;
    int file_count = 0;
    char** files = *files_ptr;
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        char path[PATH_LENGTH];
        snprintf(path, sizeof(path), "%s/%s", log_dir, entry->d_name);
        
        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
            files = realloc(files, (file_count + 1) * sizeof(char*));

            if (!files) {
                fprintf(stderr, "Memory allocation failed\n");
                closedir(dir);
                return -1;
            }
            files[file_count] = strdup(path);
            file_count++;
        }
    }
    
    closedir(dir);
    
    if (file_count == 0) {
        printf("No log files found in directory %s\n", log_dir);
        printf("Total bytes: 0\n");
        printf("Top URLs: (none)\n");
        printf("Top Referers: (none)\n");
        
        for (int i = 0; i < file_count; i++) {
            free(files[i]);
        }
        free(files);
        *files_ptr=NULL; 
        return 0;
    }
    
    *files_ptr = files;

    return file_count;
    
}
int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <log_directory> <num_threads>\n", argv[0]);
        return 1;
    }
    
    const char* log_dir = argv[1];
    int num_threads = atoi(argv[2]);
    
    if (num_threads <= 0) {
        fprintf(stderr, "Number of threads must be positive\n");
        return 1;
    }
    char** files = NULL;
    int file_count = 0;
    file_count = find_files(log_dir, &files);
    for(int i = 0; i < file_count;i++){
            printf("Path %d, %s", i, files[i]);
    }
    printf("Found %d log files\n", file_count);
    printf("Using %d threads\n", num_threads);
    printf("Processing all %d files\n", file_count);
   
    clock_t start_time = clock();
    
    global_url_table = create_url_table(HASH_TABLE_SIZE * 2);
    global_referer_table = create_referer_table(HASH_TABLE_SIZE * 2);
    
    if (!global_url_table || !global_referer_table) {
        fprintf(stderr, "Memory allocation failed for global hash tables\n");
        return 1;
    }
    
    pthread_mutex_t file_mutex;
    pthread_mutex_init(&file_mutex, NULL);
    
    int next_file_index = 0;
    
    pthread_t* threads = malloc(num_threads * sizeof(pthread_t));
    ThreadData* thread_data = malloc(num_threads * sizeof(ThreadData));
    
    if (!threads || !thread_data) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }
    
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].files = files;
        thread_data[i].next_file_index = &next_file_index;
        thread_data[i].total_files = file_count;
        thread_data[i].file_mutex = &file_mutex;
        thread_data[i].url_table = NULL;
        thread_data[i].referer_table = NULL;
        thread_data[i].thread_total_bytes = 0;
    }
    
    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&threads[i], NULL, thread_func, &thread_data[i]) != 0) {
            fprintf(stderr, "Failed to create thread %d\n", i);
            return 1;
        }
    }
    
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    clock_t parse_end_time = clock();
    
    /* creating an array from table of urls to sort*/
    int url_count = 0;
    UrlNode** url_array = url_table_to_array(global_url_table, &url_count);
    
    /* creating an array from table of referers to sort*/
    int referer_count = 0;
    RefererNode** referer_array = referer_table_to_array(global_referer_table, &referer_count);
    
    /* sorting arrays*/
    if (url_array) {
        qsort(url_array, url_count, sizeof(UrlNode*), compare_urls);
    }
    
    if (referer_array) {
        qsort(referer_array, referer_count, sizeof(RefererNode*), compare_referers);
    }
    
    clock_t sort_end_time = clock();
    
    /*printing results*/
    printf("\n=== LOG ANALYSIS RESULTS ===\n\n");
    printf("Total bytes served: %lld\n\n", total_bytes);
    
    printf("Top %d URLs by traffic:\n", TOP_N);
    printf("-------------------------\n");

    for (int i = 0; i < TOP_N && i < url_count; i++) {
        printf("%d. %s - %lld bytes\n", i + 1, url_array[i]->url, url_array[i]->total_bytes);
    }
    if (url_count == 0) {
        printf("(no URLs found)\n");
    }
    printf("\n");
    
    printf("Top %d Referers by frequency:\n", TOP_N);
    printf("-----------------------------\n");
    for (int i = 0; i < TOP_N && i < referer_count; i++) {
        printf("%d. %s - %d requests\n", i + 1, referer_array[i]->referer, referer_array[i]->count);
    }
    if (referer_count == 0) {
        printf("(no referers found)\n");
    }
    
    double parse_time = (double)(parse_end_time - start_time) / CLOCKS_PER_SEC;
    double sort_time = (double)(sort_end_time - parse_end_time) / CLOCKS_PER_SEC;
    double total_time = (double)(sort_end_time - start_time) / CLOCKS_PER_SEC;
    
    printf("\n=== PERFORMANCE STATISTICS ===\n");
    printf("Parsing time: %.3f seconds\n", parse_time);
    printf("Sorting time: %.3f seconds\n", sort_time);
    printf("Total time: %.3f seconds\n", total_time);
    printf("Processed %d files with %d threads\n", file_count, num_threads);
    
    /* free memory and destory mutex */
    for (int i = 0; i < file_count; i++) {
        free(files[i]);
    }
    free(files);
    
    if (url_array) free(url_array);
    if (referer_array) free(referer_array);
    
    free_url_table(global_url_table);
    free_referer_table(global_referer_table);
    
    free(threads);
    free(thread_data);
    
    pthread_mutex_destroy(&file_mutex);
    pthread_mutex_destroy(&stats_mutex);
    
    return 0;
}
