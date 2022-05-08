// Filipe David Amado Mendes, 2020218797
// Miguel Ângelo Graça Meneses, 2020221791


#include "declarations.h"

int main(int argc, char *argv[]) {
    MobileNode *mobile_node;

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

    char *ptr;
    mobile_node->total_request_number = (strtol(argv[1],&ptr,10));
    mobile_node->interval_time = (strtol(argv[2],&ptr,10));
    mobile_node->request_instruction_number = (strtol(argv[3],&ptr,10));
    mobile_node -> max_execution_time = (strtol(argv[4],&ptr,10));

    write(fd_task_pipe, &mobile_node, sizeof(MobileNode));


    return 0;
}
