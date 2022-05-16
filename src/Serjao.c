//Miguel Filipe de Andrade Sérgio
//2020225643

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/unistd.h>
#include <time.h>
#include <sys/shm.h>
#include <signal.h>
#include <semaphore.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <unistd.h>


#define PIPE_NAME "TASK_PIPE"
#define     BUFLEN 1024


typedef struct EdgeServer {
    int fd[2];
    pthread_t slow_thr, fast_thr;
    int id[2];
    int n_instructions;
    int capacidade_processamento1;
    int capacidade_processamento2;
    int tempo_proxima_tarefa1;
    int tempo_proxima_tarefa2;
    int performance;
    int n_t_tarefas;
    int n_t_manutencao;
} EdgeServer;

typedef struct Tarefa {
    int task_id;
    int t_execucao;
    int n_instrucoes;
} Tarefa;

typedef struct Fila {
    char string[BUFLEN];
    struct Fila *next;
    struct Fila *previus;
} Fila;

typedef struct Fila_Task_Manager {
    struct Tarefa *tarefa;
    struct Fila_Task_Manager *next;
    struct Fila_Task_Manager *previus;
} Fila_Task_Manager;

typedef struct Estatisticas {
    int n_t_tarefas;
    int n_tarefas_por_executar;
} Estatisticas;

int QUEUE_POS;
int MAX_WAIT;
int EDGE_SERVER_NUMBER;

int shmid;
struct EdgeServer *shared_var;
struct Fila *fila;
struct Fila_Task_Manager *fila_task_manager;
struct Estatisticas estatisticas;
struct tm *data;

int end;
int shmid;
FILE *log_file;
pid_t task_manager_id, maintenance_manager_id, monitor_id;

pthread_mutex_t slow_vCPU_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t fast_vCPU_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t thread_dispatcher_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t thread_scheduler_mutex = PTHREAD_MUTEX_INITIALIZER;


void inssere_fila(struct Tarefa *tarefa) {
    fila_task_manager->tarefa = tarefa;
    fila_task_manager->previus = fila_task_manager;
    fila_task_manager = fila_task_manager->next;
}

void remove_fila(struct Tarefa *tarefa) {
    while (fila_task_manager->previus != NULL) {
        fila_task_manager = fila_task_manager->previus;
    }
    while (fila_task_manager->next != NULL) {
        if (fila_task_manager->tarefa == tarefa && fila_task_manager->previus != NULL &&
            fila_task_manager->next != NULL) {
            fila_task_manager->next->previus = fila_task_manager->previus;
            fila_task_manager->previus->next = fila_task_manager->next;
            free(fila_task_manager);
        }
    }
}

void writ_log_ecra(char *message) {
    log_file = fopen("Log.txt", "w+");
    char message_assist[BUFLEN];
    time_t t;
    time(&t);
    data = localtime(&t);
    strcat(message_assist, message);
    strcat(message_assist, "\n");
    printf("%d:%d:%d %s", data->tm_hour, data->tm_min, data->tm_sec, message_assist);
    fprintf(log_file, "%s", message_assist);
    fclose(log_file);
}

void statistics() {
    writ_log_ecra("Estatisticas:");
    writ_log_ecra("Número total de tarefas executadas");
    char *aux = "";
    sprintf(aux, "%d", estatisticas.n_t_tarefas);
    writ_log_ecra(aux);
    for (int i = 0; i < EDGE_SERVER_NUMBER; i++) {
        writ_log_ecra("Numero de tarefas executadas pelo Edge_Server ");
        sprintf(aux, "%d", i);
        writ_log_ecra(aux);
        sprintf(aux, "%d", shared_var[i].n_instructions);
        writ_log_ecra(aux);
        writ_log_ecra("Numero de matucencoes ao Edge_Server ");
        sprintf(aux, "%d", i);
        writ_log_ecra(aux);
        sprintf(aux, "%d", shared_var[i].n_t_manutencao);
        writ_log_ecra(aux);
    }
    writ_log_ecra("Numero de tarefas que nao chegaram a ser executadas :");
    sprintf(aux, "%d", estatisticas.n_tarefas_por_executar);
    writ_log_ecra(aux);
}

void sigint() {
    writ_log_ecra("SIGNAL SIGINT RECEIVED");
    unlink(PIPE_NAME);
    for (int k = 0; k < EDGE_SERVER_NUMBER; k++) {
        while (shared_var[k].performance > 0) {
            writ_log_ecra("Tarefa nao concluida");
            estatisticas.n_tarefas_por_executar++;
            pthread_join(shared_var[k].slow_thr, NULL);
            pthread_join(shared_var[k].fast_thr, NULL);
        }
    }
    end = 0;
    statistics();
    fclose(log_file);
    shmdt(shared_var);
    shmctl(shmid, IPC_RMID, NULL);
    exit(0);
}

void *slow_vCPU() {
    while (end == 1) {
        pthread_mutex_lock(&slow_vCPU_mutex);
        sleep((shared_var->n_instructions * 1000) / (shared_var->capacidade_processamento1 * 1000000));
        pthread_mutex_unlock(&slow_vCPU_mutex);
        pthread_exit(NULL);
    }
    return NULL;
}


void *fast_vCPU() {
    while (end == 1) {
        pthread_mutex_lock(&fast_vCPU_mutex);
        sleep((shared_var->n_instructions * 1000) / (shared_var->capacidade_processamento2 * 1000000));
        pthread_mutex_unlock(&fast_vCPU_mutex);
        pthread_exit(NULL);
    }
    return NULL;
}

void Edge_Server(int id) {
    writ_log_ecra("Edge_server run");
    char *task = NULL;
    char *lists[3];
    int i = 0;
    char idstring[BUFLEN];
    sprintf(idstring, "%d", id);
    int a;
    int auxii = 0;
    shared_var[id].performance = 0;
    while (read(shared_var[id].fd[0], &task, sizeof(task)) > 0) {
        /*
        if (msgrcv(mqid, &message_queue, sizeof(MessageQueue), id, IPC_NOWAIT) != -1) {
            int previous_performance;
            while (shared_var[id].vCPU1_full != FREE || shared_var[id].vCPU2_full != FREE) {
                continue;
            }
            message_queue.msg = READY;
            msgsnd(mqid, &message_queue, sizeof(MessageQueue), 0);
            msgrcv(mqid, &message_queue, sizeof(MessageQueue), id, 0);

            previous_performance = shared_var[id].performance;
            shared_var[id].performance = STOPPED;
            servers_down++;
            write_log("Maintenance");
            sleep(rand() % 5 + 1);
            shared_var[id].performance = previous_performance;
            servers_down--;
        }
        */
        if (shared_var[id].tempo_proxima_tarefa1 == 0) {
            continue;
        } else if (shared_var[id].tempo_proxima_tarefa2 == 0 && shared_var[id].performance == 1) {
            shared_var[id].performance = 2;
            char *token = strtok(task, ";");
            while (token != NULL) {
                lists[i] = token;
                token = strtok(NULL, " ");
                i++;
            }
            task = lists[1];
            shared_var[id].n_instructions = atoi(task);
            shared_var[id].tempo_proxima_tarefa2 =
                    (shared_var[id].n_instructions * 1000) / (shared_var[id].capacidade_processamento2 * 1000000);
            shared_var[id].n_t_tarefas++;
            pthread_create(&shared_var[id].fast_thr, NULL, fast_vCPU(), &shared_var[id].id[1]);
            pthread_join(shared_var[id].fast_thr, NULL);
        }
    }
    free(task);
    writ_log_ecra("Edge Server end");
}

void *scheduler(struct Tarefa *task) {
    while (end == 1) {
        struct Fila_Task_Manager aux;
        int assist = 0;
        while (fila_task_manager->previus != NULL) {
            fila_task_manager = fila_task_manager->previus;
        }
        while (fila_task_manager->next != NULL) {
            for (int k = 0; k < EDGE_SERVER_NUMBER; k++) {
                if (fila_task_manager->tarefa->t_execucao > shared_var[k].tempo_proxima_tarefa1) {
                    assist++;
                }
                if (fila_task_manager->tarefa->t_execucao > shared_var[k].tempo_proxima_tarefa2) {
                    assist++;
                }
            }
            if (assist > 0) {
                remove_fila(fila_task_manager->tarefa);
            }
            assist = 0;
        }
        while (fila_task_manager->previus != NULL) {
            fila_task_manager = fila_task_manager->previus;
        }
        while (fila_task_manager->next != NULL) {
            aux.tarefa = task;
            if (fila_task_manager->next->tarefa->t_execucao > task->t_execucao) {
                fila_task_manager->next->previus = &aux;
                fila_task_manager->next = &aux;
                aux.previus = fila_task_manager;
                aux.next = fila_task_manager->next;
            }
        }
    }
    pthread_exit(NULL);
    return NULL;
}

void *dispacher() {
    while (end == 1) {
        int assist = 0;
        while (fila_task_manager->previus != NULL) {
            fila_task_manager = fila_task_manager->previus;
        }
        while (fila_task_manager->next != NULL) {
            for (int k = 0; k < EDGE_SERVER_NUMBER; k++) {
                if (fila_task_manager->tarefa->t_execucao > shared_var[k].tempo_proxima_tarefa1) {
                    assist++;
                }
                if (fila_task_manager->tarefa->t_execucao > shared_var[k].tempo_proxima_tarefa2) {
                    assist++;
                }
            }
            if (assist > 0) {
                remove_fila(fila_task_manager->tarefa);
            }
            assist = 0;
            writ_log_ecra("Pedido de tarefa eleminado");
        }
        assist = 0;
    }
    return NULL;
}

void Task_Manager() {
    for (int i = 0; i < EDGE_SERVER_NUMBER; i++) {
        if (fork() == 0) {
            Edge_Server(i);
            exit(0);
        }
    }
    while (end == 1) {
        writ_log_ecra("Task_Manager run");
        int fd;
        pthread_t thr[2];
        int id[2];
        char *task_str;
        char *task_arr_str[3];
        struct Tarefa task;
        int auxi = 0;

        if ((fd = open(PIPE_NAME, O_RDWR)) < 0) {
            writ_log_ecra("Cannot open pipe for reading: ");
            exit(0);
        }

        for (int i = 0; i < EDGE_SERVER_NUMBER; i++) {
            pipe(shared_var[i].fd);
            if (fork() == 0) {
                dup2(shared_var[i].fd[1], fileno(stdout));
                close(shared_var[i].fd[0]);
                close(shared_var[i].fd[1]);
                execlp("ls", "ls", NULL);
                exit(0);
            } else {
                dup2(shared_var[i].fd[0], fileno(stdin));
                close(shared_var[i].fd[0]);
                close(shared_var[i].fd[1]);
                execlp("ls", "ls", NULL);
            }
        }

        pthread_create(&thr[0], NULL, scheduler(&task), &id[0]);
        pthread_create(&thr[1], NULL, dispacher, &id[1]);

        while (read(fd, &task_str, BUFLEN)) {
            estatisticas.n_t_tarefas++;
            if (strcmp(task_str, "EXIT") != 0) {
                writ_log_ecra("SIGNAL SIGINT RECEIVED");
                unlink(PIPE_NAME);
                for (int k = 0; k < EDGE_SERVER_NUMBER; k++) {
                    while (shared_var[k].performance > 0) {
                        writ_log_ecra("Tarefa nao concluida");
                        estatisticas.n_tarefas_por_executar++;
                        pthread_join(shared_var[k].slow_thr, NULL);
                        pthread_join(shared_var[k].fast_thr, NULL);
                    }
                }
                end = 0;
                statistics();
                fclose(log_file);
                shmdt(shared_var);
                shmctl(shmid, IPC_RMID, NULL);
                exit(0);
            } else if (strcmp(task_str, "STATS") != 0) {
                writ_log_ecra("Estatisticas:");
                writ_log_ecra("Número total de tarefas executadas");
                char *aux = "";
                sprintf(aux, "%d", estatisticas.n_t_tarefas);
                writ_log_ecra(aux);
                for (int i = 0; i < EDGE_SERVER_NUMBER; i++) {
                    writ_log_ecra("Numero de tarefas executadas pelo Edge_Server ");
                    sprintf(aux, "%d", i);
                    writ_log_ecra(aux);
                    sprintf(aux, "%d", shared_var[i].n_instructions);
                    writ_log_ecra(aux);
                    writ_log_ecra("Numero de matucencoes ao Edge_Server ");
                    sprintf(aux, "%d", i);
                    writ_log_ecra(aux);
                    sprintf(aux, "%d", shared_var[i].n_t_manutencao);
                    writ_log_ecra(aux);
                }
                writ_log_ecra("Numero de tarefas que nao chegaram a ser executadas :");
                sprintf(aux, "%d", estatisticas.n_tarefas_por_executar);
                writ_log_ecra(aux);
            } else if (task_str != NULL) {
                char *token = strtok(task_str, ";");
                while (token != NULL) {
                    task_arr_str[auxi] = token;
                    token = strtok(NULL, " ");
                    auxi++;
                }
                task.n_instrucoes = atoi(task_arr_str[0]);
                task.n_instrucoes = atoi(task_arr_str[1]);
                task.t_execucao = atoi(task_arr_str[2]);
                while (fila_task_manager->previus != NULL) {
                    fila_task_manager = fila_task_manager->previus;
                }
                inssere_fila(&task);
            } else {
                writ_log_ecra("Fila cheia:");
            }
        }
    }
}

void Monitor() {
    while (end == 1) {
        writ_log_ecra("Monitor run");

        int aux_80 = (int) 0.8 * QUEUE_POS;
        int aux_20 = (int) 0.2 * QUEUE_POS;
        int aux = 0;
        while (fila_task_manager->previus != NULL) {
            fila_task_manager = fila_task_manager->previus;
        }
        while (fila_task_manager->next != NULL) {
            aux++;
        }
        if (aux > aux_80) {
            for (int i = 0; i < EDGE_SERVER_NUMBER; i++) {
                shared_var[i].performance = 2;
            }
        } else if (aux < aux_20) {
            for (int i = 0; i < EDGE_SERVER_NUMBER; i++) {
                shared_var[i].performance = 1;
            }
        }

    }
}

void Maintenance_Manager() {
    while (end == 1) {
        writ_log_ecra("Maintenance_Manager run");
        if (fila == NULL) {
            if (fila->previus != NULL && fila->next != NULL) {
                sprintf(fila->string, "%d", rand() % EDGE_SERVER_NUMBER + 1);
                fila->previus = fila;
                fila = fila->next;
            }
        } else {
            if (fila->previus != NULL && fila->next != NULL) {
                strcpy(fila->string, "continua");
                fila->previus = fila;
                fila = fila->next;
            }
        }
        sleep(rand() % 5 + 1);
    }
}

int main() {
    writ_log_ecra("OFFLOAD SIMULATOR STARTING");

    FILE *arq = fopen("Configuracoes.txt", "r");

    size_t len = 0;
    char *line = NULL;

    getline(&line, &len, arq);
    QUEUE_POS = atoi(line);

    getline(&line, &len, arq);
    MAX_WAIT = atoi(line);

    getline(&line, &len, arq);
    EDGE_SERVER_NUMBER = atoi(line);

    shmid = shmget(IPC_PRIVATE, sizeof(struct EdgeServer) * EDGE_SERVER_NUMBER + end, 0644 | IPC_CREAT | IPC_EXCL);
    if (shmid < 0) {
        writ_log_ecra("shmid < 0");
        exit(0);
    }
    shared_var = (struct EdgeServer *) shmat(shmid, NULL, 0);
    if (shared_var < (struct EdgeServer *) 1) {
        exit(0);
    }

    end = 1;

    for (int i = 0; i < EDGE_SERVER_NUMBER; i++) {
        getline(&line, &len, arq);
        char *token = strtok(line, ",");
        token = strtok(NULL, ",");
        shared_var[i].capacidade_processamento1 = atoi(token);
        token = strtok(NULL, ",");
        shared_var[i].capacidade_processamento2 = atoi(token);
    }
    fclose(arq);

    if ((mkfifo(PIPE_NAME, O_CREAT | O_EXCL | 0600) < 0) && (errno != EEXIST)) {
        perror("Cannot create pipe: ");
        exit(0);
    }

    if (fork() == 0) {
        Task_Manager();
        exit(0);
    }

    if (fork() == 0) {
        Maintenance_Manager();
        exit(0);
    }

    if (fork() == 0) {
        Monitor();
        exit(0);
    }
}