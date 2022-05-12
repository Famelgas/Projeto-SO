// Filipe David Amado Mendes, 2020218797
// Miguel Ângelo Graça Meneses, 2020221791



#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <semaphore.h>
#include <fcntl.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>


void mobile_node(int fd_task_pipe, char *request_number, char *interval_time, char *instruction_number, char *max_execution_time) {
    int request_num = atoi(request_number);
    int interval = atoi(interval_time);
    for (int i = 0; i < request_num; ++i) {
        int id = i;
        char *task = "";
        sprintf(task, "%d", id);
        strcat(task, ";");
        strcat(task, instruction_number);
        strcat(task, ";");
        strcat(task, max_execution_time);
        write(fd_task_pipe, task, sizeof(task));
        sleep(interval);
    }

    exit(0);
}

int main(int argc, char *argv[]) {
    // task_pipe write only
    int fd_task_pipe;
    if ((fd_task_pipe = open("TASK_PIPE", O_WRONLY)) < 0) {
        perror("Error openibng TASK_PIPE for writing");
        exit(0);
    }


    if (argc !=5) {
        printf("mobile_node {nº pedidos a gerar} {intervalo entre pedidos em ms} "
               "{milhares de instruções de cada pedido} {tempo máximo para execução}\n");
        exit(-1);
    }

    char *request_number = argv[1];
    char *interval_time = argv[2];
    char *instruction_number = argv[3];
    char *max_execution_time = argv[4];

    if (fork() == 0) {
        mobile_node(fd_task_pipe, request_number, interval_time, instruction_number, max_execution_time);
        exit(0);
    }


    return 0;
}