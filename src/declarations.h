// Filipe David Amado Mendes, 2020218797
// Miguel Ângelo Graça Meneses, 2020221791


#ifndef DECLARATIONS_H
#define DECLARATIONS_H


// ---------- Includes ---------- //

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



// ---------- Defines ---------- //

#define TASK_PIPE "TASK_PIPE"

#define BUFFER_LEN 1024

#define FREE 0
#define OCCUPIED 1

#define TSKMG_TO_EDSV 0
#define EDSV_TO_TSKMG 1

#define STOPPED 0
#define NORMAL 1
#define HIGH 2



// ---------- Structs ---------- //

typedef struct EdgeServer {
    int performance;
    long processing_power_vCPU1;
    long next_task_time_vCPU1;
    char performance_edge_server1;
    long processing_power_vCPU2;
    long next_task_time_vCPU2;
    char performance_edge_server2;
} EdgeServer;

typedef struct Task {
    int task_id;
    long total_request_number;
    long interval_time;
    long request_instruction_number;
    long max_execution_time;
} Task;

typedef struct Message {

} Message;

// ---------- Global Variables ---------- //

long QUEUE_POS;
long MAX_WAIT;
long EDGE_SERVER_NUMBER;

EdgeServer *shared_var;

int num_servers_down = 0;
int shmid;
int sem_id;
sem_t *writing_sem;
pid_t task_manager_id, maintenance_manager_id, monitor_id, thread_sch_id;
int fd_task_pipe;

char *config_file_name;
char *log_file_name = "log_file.txt";


FILE *log_file;
FILE *config_file;

pthread_mutex_t mutex;
pthread_cond_t servers_down = PTHREAD_COND_INITIALIZER;
pthread_cond_t servers_up = PTHREAD_COND_INITIALIZER;
pthread_cond_t maintenance_ready = PTHREAD_COND_INITIALIZER;


// ---------- Processes ---------- //

void Task_Manager(long QUEUE_POS, long EDGE_SERVER_NUMBER, char *edge_server[EDGE_SERVER_NUMBER][3]);

void Monitor();

void Maintenance_Manager();

void Edge_Server(int fd, char *edge_server[3], EdgeServer *SHM);


// ---------- Functions ---------- //

void write_log(char *str);

void clean_resources();

void print_stats();

#endif //DECLARATIONS_H
