#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include "monitor_ioctl.h"

#define STACK_SIZE (1024 * 1024)
#define CONTAINER_ID_LEN 32
#define CONTROL_PATH "/tmp/mini_runtime.sock"
#define LOG_DIR "logs"
#define LOG_CHUNK 4096

typedef enum { CMD_START, CMD_PS, CMD_STOP } cmd_t;

typedef struct container {
    char id[CONTAINER_ID_LEN];
    pid_t pid;
    int pipe_fd;

    int state;                 // 0 running, 1 stopped
    unsigned long soft;       // bytes
    unsigned long hard;       // bytes

    struct container *next;
} container_t;

typedef struct {
    cmd_t kind;
    char id[CONTAINER_ID_LEN];
    char rootfs[PATH_MAX];
    char command[256];
    unsigned long soft;
    unsigned long hard;
} request_t;

typedef struct {
    char msg[512];
} response_t;

container_t *containers = NULL;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/* ================= CHILD ================= */

int child_fn(void *arg) {
    char **args = (char **)arg;

    int log_fd = atoi(args[2]);

    dup2(log_fd, STDOUT_FILENO);
    dup2(log_fd, STDERR_FILENO);
    close(log_fd);

    chroot(args[0]);
    chdir("/");

    execl("/bin/sh", "sh", "-c", args[1], NULL);
    return 1;
}

/* ================= LOG THREAD ================= */

void *log_thread(void *arg) {
    container_t *c = (container_t *)arg;

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s.log", LOG_DIR, c->id);
    mkdir(LOG_DIR, 0755);

    int fd = open(path, O_CREAT | O_WRONLY | O_APPEND, 0644);

    char buf[LOG_CHUNK];
    while (1) {
        int n = read(c->pipe_fd, buf, sizeof(buf));
        if (n <= 0) break;
        write(fd, buf, n);
    }

    close(fd);
    return NULL;
}

/* ================= START ================= */

void start_container(request_t *req, response_t *res) {
    int pipefd[2];
    pipe(pipefd);

    char fd_str[16];
    sprintf(fd_str, "%d", pipefd[1]);

    char *args[3];
    args[0] = req->rootfs;
    args[1] = req->command;
    args[2] = fd_str;

    void *stack = malloc(STACK_SIZE);

    pid_t pid = clone(child_fn, (char *)stack + STACK_SIZE,
                      CLONE_NEWPID | CLONE_NEWNS | SIGCHLD, args);

    if (pid < 0) {
        sprintf(res->msg, "Failed");
        return;
    }

    close(pipefd[1]);

    /* ================= KERNEL MONITOR REGISTER ================= */
    int mfd = open("/dev/container_monitor", O_RDWR);
    if (mfd >= 0) {
        struct monitor_req m;
        m.pid = pid;
        m.soft_limit_bytes = req->soft;
        m.hard_limit_bytes = req->hard;
        strncpy(m.id, req->id, 31);
        m.id[31] = '\0';
        ioctl(mfd, MONITOR_REGISTER, &m);
        close(mfd);
    }

    /* ================= STORE METADATA ================= */
    container_t *c = malloc(sizeof(*c));

    strcpy(c->id, req->id);
    c->pid = pid;
    c->pipe_fd = pipefd[0];

    c->state = 0;
    c->soft = req->soft;
    c->hard = req->hard;

    pthread_mutex_lock(&lock);
    c->next = containers;
    containers = c;
    pthread_mutex_unlock(&lock);

    pthread_t t;
    pthread_create(&t, NULL, log_thread, c);
    pthread_detach(t);

    sprintf(res->msg, "Started %s (PID %d)", req->id, pid);
}

/* ================= STOP ================= */

void stop_container(request_t *req, response_t *res) {
    pthread_mutex_lock(&lock);
    container_t *c = containers;

    while (c) {
        if (strcmp(c->id, req->id) == 0) {
            kill(c->pid, SIGKILL);

            c->state = 1;

            int mfd = open("/dev/container_monitor", O_RDWR);
            if (mfd >= 0) {
                ioctl(mfd, MONITOR_UNREGISTER, &c->pid);
                close(mfd);
            }

            sprintf(res->msg, "Stopped %s", c->id);
            pthread_mutex_unlock(&lock);
            return;
        }
        c = c->next;
    }

    pthread_mutex_unlock(&lock);
    sprintf(res->msg, "Not found");
}

/* ================= PS (UPGRADED) ================= */

void list_containers(response_t *res) {
    pthread_mutex_lock(&lock);
    container_t *c = containers;

    strcpy(res->msg, "ID\tPID\tSTATE\tSOFT(MB)\tHARD(MB)\n");

    while (c) {
        char buf[256];

        const char *state = (c->state == 0) ? "RUNNING" : "STOPPED";

        sprintf(buf, "%s\t%d\t%s\t%lu\t%lu\n",
                c->id,
                c->pid,
                state,
                c->soft >> 20,
                c->hard >> 20);

        strcat(res->msg, buf);
        c = c->next;
    }

    pthread_mutex_unlock(&lock);
}

/* ================= SUPERVISOR ================= */

void run_supervisor() {
    int sfd = socket(AF_UNIX, SOCK_STREAM, 0);

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, CONTROL_PATH);

    unlink(CONTROL_PATH);
    bind(sfd, (struct sockaddr *)&addr, sizeof(addr));
    listen(sfd, 5);

    printf("Supervisor running...\n");

    while (1) {
        int cfd = accept(sfd, NULL, NULL);

        request_t req;
        read(cfd, &req, sizeof(req));

        response_t res = {0};

        if (req.kind == CMD_START)
            start_container(&req, &res);
        else if (req.kind == CMD_PS)
            list_containers(&res);
        else if (req.kind == CMD_STOP)
            stop_container(&req, &res);

        write(cfd, &res, sizeof(res));
        close(cfd);
    }
}

/* ================= CLIENT ================= */

void send_request(request_t *req) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, CONTROL_PATH);

    connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    write(fd, req, sizeof(*req));

    response_t res;
    read(fd, &res, sizeof(res));

    printf("%s\n", res.msg);
    close(fd);
}

/* ================= MAIN ================= */

int main(int argc, char *argv[]) {
    if (argc < 2) return 1;

    if (strcmp(argv[1], "supervisor") == 0) {
        run_supervisor();
        return 0;
    }

    request_t req = {0};

    if (strcmp(argv[1], "start") == 0) {
        req.kind = CMD_START;
        strcpy(req.id, argv[2]);
        strcpy(req.rootfs, argv[3]);
        strcpy(req.command, argv[4]);

        req.soft = 40UL << 20;
        req.hard = 64UL << 20;

    } else if (strcmp(argv[1], "ps") == 0) {
        req.kind = CMD_PS;

    } else if (strcmp(argv[1], "stop") == 0) {
        req.kind = CMD_STOP;
        strcpy(req.id, argv[2]);
    }

    send_request(&req);
    return 0;
}

