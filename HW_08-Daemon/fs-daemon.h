#ifndef FILE_SIZE_DAEMON_H
#define FILE_SIZE_DAEMON_H

#include <sys/types.h>
#include <signal.h>

#define CONFIG_FILE "./fs-daemon.conf"
#define DEFAULT_SOCKET_PATH "./fs-daemon.sock"
#define DEFAULT_FILE_PATH "./testfile.txt"
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

/* Config ctruct*/
typedef struct {
    char *file_path;
    char *socket_path;
    int daemonize;
    uid_t run_as_uid;
    gid_t run_as_gid;
} config_t;

/* Main loop signal flag*/
extern volatile sig_atomic_t running;

void signal_handler(int sig);
void setup_signals(void);
int create_daemon(void);
void drop_privileges(uid_t uid, gid_t gid);

config_t *parse_config(const char *config_file);
void free_config(config_t *config);
void print_config(const config_t *config);

int setup_socket(const char *socket_path, uid_t uid, gid_t gid);
void handle_client(int client_fd, const char *filepath);

long get_file_size(const char *filepath);
const char *get_error_string(int errnum);

void daemon_log(const char *format, ...);
int check_file_access(const char *filepath);

#endif /* FILE_SIZE_DAEMON_H */
