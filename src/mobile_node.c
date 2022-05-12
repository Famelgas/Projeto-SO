// Filipe David Amado Mendes, 2020218797
// Miguel Ângelo Graça Meneses, 2020221791


#include "declarations.h"

int main(int argc, char *argv[]) {
    // task_pipe write only
    if ((fd_task_pipe = open(TASK_PIPE, O_WRONLY)) < 0) {
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
        mobile_node(request_number, interval_time, instruction_number, max_execution_time);
        exit(0);
    }


    return 0;
}

void mobile_node(char *request_number, char *interval_time, char *instruction_number, char *max_execution_time) {
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