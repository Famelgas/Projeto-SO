// Filipe David Amado Mendes, 2020218797
// Miguel Ângelo Graça Meneses, 2020221791

#include "declarations.h"
#include <errno.h>


int main(int argc, char *argv[]) {

    config_file_name = argv[1];
    // ---------- Named Semaphore ---------- //

    sem_unlink("WRITING");
    writing_sem = sem_open("WRITING", O_CREAT | O_EXCL, 0700, 1);

    // open log file
    log_file = fopen(log_file_name, "a");
    if (log_file == NULL)
    {
        perror("Error opening log file\n");
        exit(1);
    }

    // ---------- Task Pipe ---------- //

    unlink(TASK_PIPE);
    if ((mkfifo(TASK_PIPE, O_CREAT|O_EXCL|0600)<0) && (errno!= EEXIST)) { 
        perror("Error creating TASK_PIPE");
        exit(0);
    }


    // ---------- Mutex Semaphore ---------- //

    mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;


    // ---------- Leitura config file ---------- //
    char *line;
    char *ptr;
    config_file = fopen(config_file_name, "r");
    
    if (config_file == NULL) {
        write_log("Error config config_file doesn't exist");
        exit(-1);
    }

    if (fgets(line, BUFFER_LEN, config_file) != NULL) {
        QUEUE_POS = strtol(line, &ptr, 10);
    }
    else {
        write_log("Error in config config_file: QUEUE_POS");
        exit(1);
    }

    if (fgets(line, BUFFER_LEN, config_file) != NULL) {
        MAX_WAIT = strtol(line, &ptr, 10);
    }
    else {
        write_log("Error in config config_file: MAX_WAIT");
        exit(1);
    }

    if (fgets(line, BUFFER_LEN, config_file) != NULL) {
        EDGE_SERVER_NUMBER = strtol(line, &ptr, 10);
    }
    else {
        write_log("Error in config config_file: EDGE_SERVER_NUMBER");
        exit(1);
    }

    if (EDGE_SERVER_NUMBER < 2) {
        write_log("Error in config config_file: EDGE_SERVER_NUMBER < 2");
        exit(1);
    }

    char *edge_server[EDGE_SERVER_NUMBER][3];
    char *temp_edge_server[EDGE_SERVER_NUMBER];


    // ---------- Create shared memory ---------- //
    
    EdgeServer *temp_shared_var = malloc(sizeof(EdgeServer));
    shared_var = realloc(temp_shared_var, EDGE_SERVER_NUMBER * sizeof(EdgeServer));
    shmid = shmget(IPC_PRIVATE, sizeof(&shared_var), IPC_CREAT); 

    // ---------- Reading server info ---------- //

    for (int i = 0; (fgets(line, 100, config_file)) != NULL; ++i) {
        temp_edge_server[i] = line;
    }

    int t = 0;
    for (int i = 0; i < EDGE_SERVER_NUMBER; ++i) {
        char *token = strtok(temp_edge_server[i], " ");
        while (token != NULL) {
            edge_server[i][t] = token;
            ++t;
        }
        t = 0;
    }
    
    free(temp_shared_var);

    
    // ---------- Start task manager ---------- //

    if ((task_manager_id = fork()) == 0) {
        write_logfile("TASK MANAGER STARTING");
        Task_Manager(QUEUE_POS, EDGE_SERVER_NUMBER, edge_server);
    }
    else if (task_manager_id == -1) {
        write_logfile("ERROR STARTING TASK MANAGER PROCESS");
        exit(1);
    }

    // ---------- Start maintenance manager ---------- //

    if ((maintenance_manager_id = fork()) == 0) {
        write_logfile("MAINTENANCE MANAGER STARTING");
        Maintenance_Manager();
    }
    else if (maintenance_manager_id == -1) {
        write_logfile("ERROR CREATING MAINTENANCE MANAGER PROCESS");
        exit(1);
    }
    
    // ---------- Start monitor ---------- //

    if ((monitor_id = fork()) == 0) {
        write_logfile("MONITOR STARTING");
        Maintenance_Manager();
    }
    else if (monitor_id == -1) {
        write_logfile("ERROR CREATING MONITOR PROCESS");
        exit(1);
    }

    // ---------- Start thread scheduler ---------- //

    if ((thread_sch_id = fork()) == 0) {
        write_logfile("TRHEAD SCHEDULER STARTING");
        Maintenance_Manager();
    }
    else if (thread_sch_id == -1) {
        write_logfile("ERROR CREATING TRHEAD SCHEDULER PROCESS");
        exit(1);
    }


    clean_resources();



    return 0;
}
