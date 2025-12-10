#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "fs-daemon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <libgen.h>
#include <pwd.h>
#include <grp.h>
#include <stdarg.h>

volatile sig_atomic_t running = 1;

void daemon_log(const char *format, ...) {
    va_list args;
    char buffer[256];
    time_t now;
    struct tm *tm_info;
    
    time(&now);
    tm_info = localtime(&now);
    
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    printf("[%s] ", buffer);
    
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    
    printf("\n");
    fflush(stdout);
}

void signal_handler(int sig) {
    daemon_log("Signal %d recieved. Exiting...", sig);
    running = 0;
}

void setup_signals(void) {
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);
}

const char *get_error_string(int errnum) {
    switch(errnum) {
        case ENOENT: return "File not found";
        case EACCES: return "Permission denied";
        case EPERM: return "Operation not permitted";
        case EIO: return "I/O error";
        case ENOSPC: return "No space left on device";
        case EISDIR: return "Is a directory";
        case ELOOP: return "Too many symbolic links";
        case ENAMETOOLONG: return "File name too long";
        case ENOTDIR: return "Not a directory";
        case EROFS: return "Read-only file system";
        case EINVAL: return "Invalid argument";
        default: return "Unknown error";
    }
}

int check_file_access(const char *filepath) {
    if (access(filepath, F_OK) != 0) {
        return -errno;
    }
    if (access(filepath, R_OK) != 0) {
        return -errno;
    }
    return 0;
}

config_t *parse_config(const char *config_file) {
    FILE *fp = fopen(config_file, "r");
    config_t *config = malloc(sizeof(config_t));
    
    if (!config) {
        daemon_log("Memory allocation error");
        return NULL;
    }
    
    config->file_path = strdup(DEFAULT_FILE_PATH);
    config->socket_path = strdup(DEFAULT_SOCKET_PATH);
    config->daemonize = 1;
    config->run_as_uid = getuid();
    config->run_as_gid = getgid();
    
    if (!fp) {
        daemon_log("WARNING: config file %s not found. Using defaults.", config_file);
        return config;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char *key = strtok(line, " \t=\n");
        char *value = strtok(NULL, " \t=\n");
        char *comment;
        
        if (!key || key[0] == '#') continue;
        
        if ((comment = strchr(key, '#')) != NULL) {
            *comment = '\0';
        }
        if (value && (comment = strchr(value, '#')) != NULL) {
            *comment = '\0';
        }
        
        if (strcmp(key, "file") == 0) {
            free(config->file_path);
            config->file_path = strdup(value);
        } else if (strcmp(key, "socket") == 0) {
            free(config->socket_path);
            config->socket_path = strdup(value);
        } else if (strcmp(key, "daemonize") == 0) {
            config->daemonize = (strcmp(value, "yes") == 0 || strcmp(value, "1") == 0 || 
                                strcmp(value, "true") == 0 || strcmp(value, "on") == 0);
        } else if (strcmp(key, "user") == 0) {
            struct passwd *pw = getpwnam(value);
            if (pw) {
                config->run_as_uid = pw->pw_uid;
            } else {
                daemon_log("WARNING: USER %s not found.", value);
            }
        } else if (strcmp(key, "group") == 0) {
            struct group *gr = getgrnam(value);
            if (gr) {
                config->run_as_gid = gr->gr_gid;
            } else {
                daemon_log("WARNING: GROUP %s not found", value);
            }
        }
    }
    
    fclose(fp);
    return config;
}

void print_config(const config_t *config) {
    daemon_log("Daemon config:");
    daemon_log("  Lookup file: %s", config->file_path);
    daemon_log("  UNIX Socket path: %s", config->socket_path);
    daemon_log("  Daemon mode: %s", config->daemonize ? "yes" : "no");
    daemon_log("  UID/GID: %d/%d", config->run_as_uid, config->run_as_gid);
}

void free_config(config_t *config) {
    if (config) {
        free(config->file_path);
        free(config->socket_path);
        free(config);
    }
}

int create_daemon(void) {
    pid_t pid = fork();
    
    if (pid < 0) {
        daemon_log("fork error: %s", strerror(errno));
        return -1;
    }
    
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }
    
    umask(0);
    
    if (setsid() < 0) {
        daemon_log("setsid error: %s", strerror(errno));
        return -1;
    }
    
    if (chdir("/") < 0) {
        daemon_log("chdir error: %s", strerror(errno));
        return -1;
    }
    
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_WRONLY);
    open("/dev/null", O_WRONLY);
    
    return 0;
}

void drop_privileges(uid_t uid, gid_t gid) {
    if (getuid() == 0) {
        if (setgid(gid) < 0) {
            daemon_log("setgid error: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
        
        if (setuid(uid) < 0) {
            daemon_log("setuid error: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
        
        daemon_log("Drop privilegies to UID/GID: %d/%d", uid, gid);
    }
}

int setup_socket(const char *socket_path, uid_t uid, gid_t gid) {
    int sockfd;
    struct sockaddr_un addr;
    
    unlink(socket_path);
    
    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        daemon_log("Error creating socket %s", strerror(errno));
        return -1;
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        daemon_log("bind error: %s", strerror(errno));
        close(sockfd);
        return -1;
    }
    
    if (chown(socket_path, uid, gid) < 0) {
        daemon_log("WARNING: can't change socket owner: %s", strerror(errno));
    }
    
    if (chmod(socket_path, 0660) < 0) {
        daemon_log("WARNING: can't change socket rights: %s", strerror(errno));
    }
    
    if (listen(sockfd, MAX_CLIENTS) < 0) {
        daemon_log("Listen error: %s", strerror(errno));
        close(sockfd);
        return -1;
    }
    
    daemon_log("Socket created: %s", socket_path);
    return sockfd;
}

long get_file_size(const char *filepath) {
    struct stat st;
    
    if (stat(filepath, &st) < 0) {
        return -errno;
    }
    
    if (!S_ISREG(st.st_mode)) {
        return -EINVAL;
    }
    
    return st.st_size;
}

void handle_client(int client_fd, const char *filepath) {
    char response[BUFFER_SIZE];
    ssize_t bytes_sent;
    long size = get_file_size(filepath);
    
    if (size < 0) {
        snprintf(response, sizeof(response), "ERROR: %s\n", get_error_string((int)-size));
    } else {
        snprintf(response, sizeof(response), "%ld\n", size);
    }
    
    bytes_sent = send(client_fd, response, strlen(response), 0);
    if (bytes_sent < 0) {
        daemon_log("Sending data to client error: %s", strerror(errno));
    }
    
    close(client_fd);
    
#ifdef DEBUG
    daemon_log("Response sent to a client: %s", response);
#endif
}

int main(int argc, char *argv[]) {
    config_t *config;
    int sockfd, client_fd;
    struct sockaddr_un client_addr;
    socklen_t client_len;
    int opt;
    const char *custom_config = CONFIG_FILE;
    
    for (opt = 1; opt < argc; opt++) {
        if (strcmp(argv[opt], "--no-daemon") == 0 || strcmp(argv[opt], "-n") == 0) {
            continue;
        } else if (strcmp(argv[opt], "--config") == 0 || strcmp(argv[opt], "-c") == 0) {
            if (++opt < argc) {
                custom_config = argv[opt];
            }
        } else if (strcmp(argv[opt], "--help") == 0 || strcmp(argv[opt], "-h") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  -c, --config FILE    Use config file specified\n");
            printf("  -n, --no-daemon      Run in debug (no-demon) mode\n");
            printf("  -h, --help           Show help\n");
            return 0;
        }
    }
    
    config = parse_config(custom_config);
    if (!config) {
        return EXIT_FAILURE;
    }
    
    for (opt = 1; opt < argc; opt++) {
        if (strcmp(argv[opt], "--no-daemon") == 0 || strcmp(argv[opt], "-n") == 0) {
            config->daemonize = 0;
        }
    }
    
    print_config(config);
    
    setup_signals();
    
    if (config->daemonize) {
        daemon_log("Running in daemon mode...");
        if (create_daemon() < 0) {
            daemon_log("Creating deamon failed.");
            free_config(config);
            return EXIT_FAILURE;
        }
    }
    
    sockfd = setup_socket(config->socket_path, config->run_as_uid, config->run_as_gid);
    if (sockfd < 0) {
        free_config(config);
        return EXIT_FAILURE;
    }
    
    drop_privileges(config->run_as_uid, config->run_as_gid);
    
    int access_check = check_file_access(config->file_path);
    if (access_check < 0) {
        daemon_log("WATRNING: error access to file '%s': %s", 
                  config->file_path, get_error_string(-access_check));
    } else {
        daemon_log("File '%s' is available", config->file_path);
    }
    
    daemon_log("Daemon is running. Waiting for clients...");
    
    while (running) {
        fd_set readfds;
        struct timeval tv;
        
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int activity = select(sockfd + 1, &readfds, NULL, NULL, &tv);
        
        if (activity < 0 && errno != EINTR) {
            daemon_log("select error: %s", strerror(errno));
            break;
        }
        
        if (activity > 0 && FD_ISSET(sockfd, &readfds)) {
            client_len = sizeof(client_addr);
            client_fd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);
            
            if (client_fd < 0) {
                daemon_log("accept error: %s", strerror(errno));
                continue;
            }
            
            handle_client(client_fd, config->file_path);
        }
    }
    
    daemon_log("Shuting down daemon...");
    close(sockfd);
    unlink(config->socket_path);
    free_config(config);
    
    daemon_log("Daemon shutdown success");
    return EXIT_SUCCESS;
}
