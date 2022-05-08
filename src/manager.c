// Filipe David Amado Mendes, 2020218797
// Miguel Ângelo Graça Meneses, 2020221791


#include "declarations.h"



void Task_Manager(long QUEUE_POS, long EDGE_SERVER_NUMBER, char *edge_server[EDGE_SERVER_NUMBER][3]) {
    char *stack[QUEUE_POS];
    int fd;
    int fd_unnamed[EDGE_SERVER_NUMBER][2];
    char string[BUFFER_LEN];
    char *SHM;

    if ((shared_var = (EdgeServer *) shmat(shmid, NULL, 0) == (EdgeServer *) - 1)) {
        write_log("Shmat error");
        exit(1);
    }


    // task_pipe read only
    if ((fd_task_pipe = open(TASK_PIPE, O_RDONLY)) < 0) {
        perror("Error opening TASK_PIPE for reading");
        exit(0);
    }
    
    for (int i = 0; i < QUEUE_POS; ++i) {
        stack[i] = NULL;
    }

    for (int i = 0; i < EDGE_SERVER_NUMBER; ++i) {
        pipe(fd_unnamed[i]);

        if ((fd = fork()) ==  0) {
            dup2(fd_unnamed[i][1], fd);
            close(fd_unnamed[i][0]);
            close(fd_unnamed[i][1]);
            execlp("ls", "ls", NULL);
        }
        else {
            dup2(fd_unnamed[i][0], fd);
            close(fd_unnamed[i][0]);
            close(fd_unnamed[i][1]);
            execlp("ls", "ls", NULL);
        }
        pid_t pid;

        if ((pid = fork()) < 0) {
            write_log("Fork error");
        } 
        if (pid == 0) {
            Edge_Server(fd_unnamed[i][0], edge_server[i], shared_var);
        }
    
    }

}


void Edge_Server(int fd, char *edge_server[3], EdgeServer *SHM) {
    int performance = 0;
    char *edge_server_name;
    long processing_power_vCPU1;
    long processing_power_vCPU2;
    char *ptr;
    pthread_t slow_thr, fast_thr;

    edge_server_name = edge_server[0];
    processing_power_vCPU1 = strtol(edge_server[1], &ptr, 10);
    processing_power_vCPU2 = strtol(edge_server[1], &ptr, 10);

    SHM->processing_power_vCPU1 = processing_power_vCPU1;
    SHM->processing_power_vCPU2 = processing_power_vCPU2;

    char *string;
    char *stringassist = "READY";
    string = (char *) (edge_server_name + *stringassist);
    write_log(string);
    
    while (read(fd, NULL, sizeof(NULL)) > 0) {
        if (read(fd, NULL, sizeof(NULL)) > 0) {
            pthread_create(&slow_thr, NULL, slowvCPU, NULL);
            pthread_join(slow_thr, NULL);
        }
        if (read(fd, NULL, sizeof(NULL)) > 0) {
            pthread_create(&fast_thr, NULL, fastvCPU, NULL);
            pthread_join(fast_thr, NULL);
        } else {
            continue;
        }
    }


    pthread_exit(NULL);
}


static void *slowvCPU(void *instrucao) {
    sem_wait(writing_sem);
    write_log("Slow vCPU");
    sem_post(writing_sem);
    pthread_exit(&instrucao);
}


static void *fastvCPU(void *instrucao) {
    sem_wait(writing_sem);
    write_log("Fast vCPU");
    sem_post(writing_sem);
    pthread_exit(&instrucao);
}


void Monitor();


void Maintenance_Manager() {
    pthread_mutex_lock(&mutex);

    while (num_servers_down == EDGE_SERVER_NUMBER - 1) {
        pthread_cond_signal(&servers_down);
        pthread_cond_wait(&servers_up, &mutex);
    }

    int maintenance_time = rand() % 5 + 1;
    

    // alterar condição para verificar se as tarefas ja acabaram todas 
    while (1) {
        pthread_cond_wait(&maintenance_ready, &mutex);
    }

    sleep(maintenance_time);


    pthread_mutex_unlock(&mutex);
}
