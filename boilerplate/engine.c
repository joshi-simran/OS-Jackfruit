// engine.c (WORKING MINIMAL VERSION)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include "monitor_ioctl.h"

#define MAX_CONTAINERS 100
#define STATE_FILE "containers.db"

typedef struct {
    char id[32];
    pid_t pid;
    char state[16];
    char log_file[128];
} container_t;

container_t containers[MAX_CONTAINERS];
int count = 0;

void save_state() {
    FILE *f = fopen(STATE_FILE, "w");
    for (int i = 0; i < count; i++) {
        fprintf(f, "%s %d %s %s\n",
            containers[i].id,
            containers[i].pid,
            containers[i].state,
            containers[i].log_file);
    }
    fclose(f);
}

void load_state() {
    FILE *f = fopen(STATE_FILE, "r");
    if (!f) return;

    count = 0;
    while (fscanf(f, "%s %d %s %s",
        containers[count].id,
        &containers[count].pid,
        containers[count].state,
        containers[count].log_file) == 4) {
        count++;
    }
    fclose(f);
}

void register_monitor(pid_t pid) {
    int fd = open("/dev/container_monitor", O_RDWR);
    if (fd < 0) return;

    struct monitor_request req;
    req.pid = pid;
    req.soft_limit = 40 * 1024 * 1024;
    req.hard_limit = 64 * 1024 * 1024;
    strcpy(req.container_id, "demo");

    ioctl(fd, MONITOR_REGISTER, &req);
    close(fd);
}

void start_container(char *id, char *cmd) {
    pid_t pid = fork();

    if (pid == 0) {
        char log_file[128];
        sprintf(log_file, "logs_%s.txt", id);

        int fd = open(log_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        close(fd);

        execlp(cmd, cmd, NULL);
        perror("exec failed");
        exit(1);
    } else {
        strcpy(containers[count].id, id);
        containers[count].pid = pid;
        strcpy(containers[count].state, "running");

        sprintf(containers[count].log_file, "logs_%s.txt", id);

        register_monitor(pid);

        count++;
        save_state();

        printf("[engine] Container '%s' started (PID %d)\n", id, pid);
    }
}

void list_containers() {
    load_state();

    printf("ID\tPID\tSTATE\n");
    for (int i = 0; i < count; i++) {
        printf("%s\t%d\t%s\n",
            containers[i].id,
            containers[i].pid,
            containers[i].state);
    }
}

void show_logs(char *id) {
    load_state();

    for (int i = 0; i < count; i++) {
        if (strcmp(containers[i].id, id) == 0) {
            char cmd[256];
            sprintf(cmd, "cat %s", containers[i].log_file);
            system(cmd);
            return;
        }
    }

    printf("Container not found\n");
}

void stop_container(char *id) {
    load_state();

    for (int i = 0; i < count; i++) {
        if (strcmp(containers[i].id, id) == 0) {
            kill(containers[i].pid, SIGKILL);
            strcpy(containers[i].state, "stopped");
            save_state();

            printf("[engine] Container '%s' stopped\n", id);
            return;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: engine <cmd>\n");
        return 1;
    }

    if (strcmp(argv[1], "supervisor") == 0) {
        printf("[engine] Supervisor running...\n");
        while (1) sleep(10);
    }

    else if (strcmp(argv[1], "start") == 0) {
        if (argc < 4) {
            printf("Usage: engine start <id> <cmd>\n");
            return 1;
        }
        load_state();
        start_container(argv[2], argv[3]);
    }

    else if (strcmp(argv[1], "ps") == 0) {
        list_containers();
    }

    else if (strcmp(argv[1], "logs") == 0) {
        if (argc < 3) return 1;
        show_logs(argv[2]);
    }

    else if (strcmp(argv[1], "stop") == 0) {
        if (argc < 3) return 1;
        stop_container(argv[2]);
    }

    return 0;
}
