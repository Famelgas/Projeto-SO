// Filipe David Amado Mendes, 2020218797
// Miguel Ângelo Graça Meneses, 2020221791

#include "declarations.h"


// ---------- Processes ---------- //


void Task_Manager() {
    task_queue = malloc(sizeof(Task) * QUEUE_POS);
    for (int i = 0; i < QUEUE_POS; ++i) {
        task_queue[i].task_id = -1;
        task_queue[i].priority = -1;
        task_queue[i].instruction_number = 0;
        task_queue[i].max_execution_time = 1000000;
    }
    
    pthread_t threads[2];
    int thread_id[2];

    sem_wait(shared_var_sem);
    if ((shared_var = (EdgeServer *) shmat(shmid, NULL, 0)) == (EdgeServer *) - 1) {
        write_log("Shmat error");
        exit(1);
    }
    sem_post(shared_var_sem);


    for (int i = 0; i < EDGE_SERVER_NUMBER; ++i) {
        if (fork() == 0) {
            Edge_Server(i);

        }
        else {
            write_log("Error starting Edge Server process");
        }
        
        sem_wait(shared_var_sem);
        sem_post(shared_var_sem);

    }

    pthread_create(&threads[0], NULL, thread_scheduler, &thread_id[0]);
    pthread_create(&threads[1], NULL, thread_scheduler, &thread_id[1]);


    // task_pipe read only
    if ((fd_task_pipe = open(TASK_PIPE, O_RDONLY)) < 0) {
        perror("Error opening TASK_PIPE for reading");
        exit(0);
    }

    while (end_processes == 1) {
        Task task;
        char *str;
        char *ptr;

        while (read(fd_task_pipe, &str, BUFFER_LEN)) {
            if (str == NULL) {
                write_log("Error reading from TASK_PIPE");
                break;
            }
            if (strcmp(str, EXIT) == 0) {
                signal(SIGINT, sigint);
                break;
            }
            if (strcmp(str, STATS) == 0) {
                signal(SIGTSTP, statistics);
            }

            int t = 0;
            char *str_task[3];
            char *token = strtok(str, ";");
            
            while (token != NULL) {
                str_task[t] = token;
                token = strtok(str, ";");
            }

            task.task_id = atoi(str_task[0]);
            task.instruction_number = strtol(str_task[1], &ptr, 10);
            task.max_execution_time = strtol(str_task[2], &ptr, 10);
            
            for (int i = 0; i < QUEUE_POS; ++i) {
                if (sizeof(task_queue) == QUEUE_POS) {
                    break;
                }
                if (task_queue[i].task_id == -1) {
                    task_queue[i] = task;
                }
            }
            
            str = "";
        }
        
        sem_wait(shared_var_sem);
        for (int i = 0; i < EDGE_SERVER_NUMBER; ++i) {
            pipe(shared_var[i].fd_unnamed);
            close(shared_var[i].fd_unnamed[0]);

            write(shared_var[i].fd_unnamed[1], &task_queue[0], sizeof(Task));
            task_queue[0].task_id = -1;
            task_queue[0].priority = -1;
            task_queue[0].instruction_number = 0;
            task_queue[0].max_execution_time = 1000000;
            
            close(shared_var[i].fd_unnamed[1]);
        }
        sem_post(shared_var_sem);
    }

    pthread_join(threads[0], NULL);
    pthread_join(threads[1], NULL);
    exit(0);
}


void Edge_Server(int id) {
    sem_wait(shared_var_sem);
    if ((shared_var = (EdgeServer *) shmat(shmid, NULL, 0)) == (struct EdgeServer *) -1) {
        write_log("Shmat error!");
        exit(1);
    }

    shared_var[id].tasks_completed = 0;
    shared_var[id].vCPU1_full = FREE;
    shared_var[id].vCPU2_full = FREE;

    sem_post(shared_var_sem);

    char id_string[BUFFER_LEN];
    sprintf(id_string, "%d", id);

    while (end_processes == 1) {
        sem_wait(shared_var_sem);
        Task task;
        close(shared_var[id].fd_unnamed[1]);

        while (read(shared_var[id].fd_unnamed[0], &task, sizeof(task)) > 0) {
            if (message_queue != NULL && message_queue->string == id_string) {
                message_queue = message_queue->previous;
                int previous_performance;

                while (shared_var[id].vCPU1_full != FREE || shared_var[id].vCPU2_full != FREE) {
                    continue;
                }

                if (shared_var[id].vCPU1_full == FREE && shared_var[id].vCPU2_full == FREE) {
                    message_queue->string = "ready";
                    
                    if (message_queue->previous != NULL) {
                        message_queue = message_queue->previous;
                    }
                    else {
                        free(message_queue);
                    }
                    
                    if (message_queue->next != NULL) {
                        message_queue = message_queue->next;
                    }
                    else {
                        free(message_queue);
                    }
                    message_queue->previous = message_queue;

                    previous_performance = shared_var[id].performance;
                    shared_var[id].performance = STOPPED;
                    write_log("Maintenance");
                    sleep(rand() % 5 + 1);

                    shared_var[id].performance = previous_performance;

                    if (message_queue->previous != NULL) {
                        message_queue = message_queue->previous;
                    }
                    else {
                        free(message_queue);
                    }
                    
                    if (message_queue->next != NULL) {
                        message_queue = message_queue->next;
                    }
                    else {
                        free(message_queue);
                    }

                }
            }

            if (shared_var[id].performance == STOPPED) {
                continue;
            }

            else if (shared_var[id].performance == NORMAL) {
                shared_var[id].instruction_number = task.instruction_number;
                shared_var[id].next_task_time_vCPU1 = (task.instruction_number * 1000) / (shared_var[id].processing_power_vCPU1 * 1000000);
                
                if (shared_var[id].next_task_time_vCPU1 == 0) {
                    pthread_create(&shared_var[id].slow_thread, NULL, slow_vCPU(id), NULL);
                    pthread_join(shared_var[id].slow_thread, NULL);
                    shared_var[id].tasks_completed++;
                    stats.tasks_completed++;
                }
                else {
                    continue;
                }
            }

            else if (shared_var[id].performance == HIGH) {
                shared_var[id].instruction_number = task.instruction_number;
                shared_var[id].next_task_time_vCPU1 = (task.instruction_number * 1000) / (shared_var[id].processing_power_vCPU1 * 1000000);
                shared_var[id].next_task_time_vCPU2 = (task.instruction_number * 1000) / (shared_var[id].processing_power_vCPU2 * 1000000);
                
                if (task.max_execution_time < shared_var[id].next_task_time_vCPU1) {
                    if (shared_var[id].next_task_time_vCPU1 == 0) {
                        pthread_create(&shared_var[id].slow_thread, NULL, slow_vCPU(id), NULL);
                        pthread_join(shared_var[id].slow_thread, NULL);    
                        shared_var[id].tasks_completed++;
                        stats.tasks_completed++;
                    }
                    else if (shared_var[id].next_task_time_vCPU2 == 0){
                        pthread_create(&shared_var[id].fast_thread, NULL, fast_vCPU(id), NULL);
                        pthread_join(shared_var[id].fast_thread, NULL);
                        shared_var[id].tasks_completed++;
                        stats.tasks_completed++;
                    }
                    else {
                        continue;
                    }
                }
                
                else if (task.max_execution_time > shared_var[id].next_task_time_vCPU1 && task.max_execution_time < shared_var[id].next_task_time_vCPU2) {
                    if (shared_var[id].next_task_time_vCPU2 == 0){
                        pthread_create(&shared_var[id].fast_thread, NULL, fast_vCPU(id), NULL);
                        pthread_join(shared_var[id].fast_thread, NULL);
                    }
                    else {
                        continue;
                    }
                }

                else {
                    continue;
                }
            }

            shared_var[id].instruction_number = 0;
            shared_var[id].next_task_time_vCPU1 = 0;
            shared_var[id].next_task_time_vCPU2 = 0;

        }
        sem_post(shared_var_sem);

    }
    exit(0);
}


void Monitor() {
    while (end_processes == 1) {
        sem_wait(shared_var_sem);
        if ((sizeof(task_queue) / QUEUE_POS) > (0.8 * QUEUE_POS) && task_queue[0].max_execution_time > MAX_WAIT) {
            while ((sizeof(task_queue) / QUEUE_POS) > (0.2 * QUEUE_POS)) {
                for (int i = 0; i < EDGE_SERVER_NUMBER; ++i) {
                    shared_var[i].performance = HIGH;
                }
            }
        }
        else if ((sizeof(task_queue) / QUEUE_POS) < (0.2 * QUEUE_POS)) {
            for (int i = 0; i < EDGE_SERVER_NUMBER; ++i) {
                    shared_var[i].performance = NORMAL;
                }
        }

        else {
            continue;
        }
        sem_post(shared_var_sem);
    }
    exit(0);
}


void Maintenance_Manager() {
    while (end_processes == 1) {
        if (message_queue == NULL) {
            sprintf(message_queue->string, "%ld", rand() % EDGE_SERVER_NUMBER + 1);
            message_queue->previous = message_queue;
            message_queue = message_queue->next;
        }

        else {
            message_queue->string = "continue";
            message_queue->previous = message_queue;
            message_queue = message_queue->next;
        }

        sleep(rand() % 5 + 1);
    }
    exit(0);
}


// ---------- Threads ---------- //


void *slow_vCPU(int id) {
    while(1) {
        pthread_mutex_lock(shared_var[id].slow_vCPU_mutex);
        long wait = (shared_var[id].instruction_number * 1000) / (shared_var[id].processing_power_vCPU1 * 1000000);
        shared_var[id].vCPU1_full = FULL;
        sleep(wait);
        shared_var[id].vCPU1_full = FREE;
        pthread_mutex_unlock(shared_var[id].slow_vCPU_mutex);
        pthread_exit(NULL);
    }

    return NULL;
}


void *fast_vCPU(int id) {
    while(1) {
        pthread_mutex_lock(shared_var[id].fast_vCPU_mutex);
        long wait = (shared_var[id].instruction_number * 1000) / (shared_var[id].processing_power_vCPU2 * 1000000);
        shared_var[id].vCPU2_full = FULL;
        sleep(wait);
        shared_var[id].vCPU2_full = FREE;
        pthread_mutex_unlock(shared_var[id].fast_vCPU_mutex);
        pthread_exit(NULL);
    }

    return NULL;
}


void *thread_scheduler() {
    while (end_processes == 1) {
        for (int i = 0; i < QUEUE_POS; ++i) {
            if (task_queue[i].priority == 1) {
                int b = 1;
                sem_wait(shared_var_sem);
                for (int e = 0; e < EDGE_SERVER_NUMBER; ++e) {
                    if (shared_var->performance == STOPPED) {
                        continue;
                    }
                    if (task_queue[i].max_execution_time > shared_var[e].next_task_time_vCPU1) {
                        b = 0;
                    }

                    if (task_queue[i].max_execution_time > shared_var[e].next_task_time_vCPU2) {
                        b = 0;
                    }
                }
                sem_post(shared_var_sem);
                
                if (b == 0) {
                    write_log("Task deleted");
                    task_queue[i].task_id = -1;
                    task_queue[i].priority = -1;
                    task_queue[i].instruction_number = 0;
                    task_queue[i].max_execution_time = 0;
                }
            }
        }

        Task key;
        int j;
        for (int i = 1; i < QUEUE_POS; ++i) {
            key = task_queue[i];
            j = i - sizeof(Task);

            while (j >= 0 && task_queue[j].max_execution_time > key.max_execution_time) {
                task_queue[j + 1] = task_queue[j];
                j = j - 1;
            }
            task_queue[j + 1] = key;
        }

        int p = 0;
        for (int i = 0; i < QUEUE_POS; ++i) {
            task_queue[i].priority = p;
        }
    }

    pthread_exit(NULL);
    return NULL;
}


void *thread_dispatcher() {
    while (end_processes == 1) {
        for (int i = 0; i < QUEUE_POS; ++i) {
            if (task_queue[i].priority == 1) {
                int b = 1;
                sem_wait(shared_var_sem);
                for (int e = 0; e < EDGE_SERVER_NUMBER; ++e) {
                    if (task_queue[i].max_execution_time > shared_var[e].next_task_time_vCPU1) {
                        b = 0;
                    }

                    if (task_queue[i].max_execution_time > shared_var[e].next_task_time_vCPU2) {
                        b = 0;
                    }
                }
                sem_post(shared_var_sem);
                
                if (b == 0) {
                    write_log("Task deleted");
                    task_queue[i].task_id = -1;
                    task_queue[i].priority = -1;
                    task_queue[i].instruction_number = 0;
                    task_queue[i].max_execution_time = 0;
                }
            }
        }
    }
    pthread_exit(NULL);
    return NULL;
}



// ---------- Functions ---------- //



void sigint(int signum) {
    write_log("SIGINT signal recieved");
    unlink(TASK_PIPE);
    sem_wait(shared_var_sem);
    for (int i = 0; i < EDGE_SERVER_NUMBER; ++i) {
        while (shared_var[i].performance > 0) {
            write_log("Task not completed");
            pthread_join(shared_var[i].slow_thread, NULL);
            pthread_join(shared_var[i].fast_thread, NULL);
        }
    }
    sem_post(shared_var_sem);

    end_processes = 0;
    statistics(SIGTSTP);
    clean_resources();
    exit(0);
}



void statistics(int signum) {
    sem_wait(stats_sem);
    sem_wait(shared_var_sem);

    write_log("SIGTSTP signal recieved");

    if ((shared_var = (EdgeServer *) shmat(shmid, NULL, 0)) == (struct EdgeServer *) -1) {
        write_log("Shmat error!");
        exit(1);
    }

    printf("Total tasks completed: %ld", stats.tasks_completed);

    // implementar tempo medio de execuçao por tarefa
    printf("Average task response time: %ld", stats.average_response_time);


    for (int i = 0; i < EDGE_SERVER_NUMBER; ++i) {
        printf("Tasks completed by edge server %s: %ld", shared_var[i].name, shared_var[i].tasks_completed);
    }
    
    printf("Non completed tasks: %ld", stats.non_completed_tasks);   

    sem_post(shared_var_sem);
    sem_post(stats_sem);

}


void write_log(char *str) {
    sem_wait(writing_sem);
    fprintf(log_file, "%s\n", str);
    fflush(log_file);
    sem_post(writing_sem);
}


void clean_resources() {
    fclose(config_file);
    fclose(log_file);
    shmdt(shared_var);
    free(task_queue);
    unlink(TASK_PIPE);
    sem_unlink("WRITING_SEM");
    sem_unlink("STATS_SEM");
    sem_unlink("SHARED_VAR_SEM");
}



// ---------- Main ---------- //



int main(int argc, char *argv[]) {
    signal(SIGINT, sigint);
    signal(SIGTSTP, statistics);

    end_processes = 1;
    config_file_name = argv[1];
    // ---------- Named Semaphore ---------- //

    sem_unlink("WRITING_SEM");
    writing_sem = sem_open("WRITING_SEM", O_CREAT | O_EXCL, 0700, 1);

    sem_unlink("STATS_SEM");
    stats_sem = sem_open("STATS_SEM", O_CREAT | O_EXCL, 0700, 1);

    sem_unlink("SHARED_VAR_SEM");
    shared_var_sem = sem_open("SIGINT_SEM", O_CREAT | O_EXCL, 0700, 1);



    // open log file
    log_file = fopen(log_file_name, "a");
    if (log_file == NULL)
    {
        perror("Error opening log file\n");
        exit(1);
    }

    // ---------- Default stats ---------- //

    stats.tasks_completed = 0;
    stats.average_response_time = 0;
    stats.non_completed_tasks = 0;

    // ---------- Task Pipe ---------- //

    unlink(TASK_PIPE);
    if ((mkfifo(TASK_PIPE, O_CREAT|O_EXCL|0600)<0) && (errno!= EEXIST)) { 
        perror("Error creating TASK_PIPE");
        exit(0);
    }

    printf("ola\n");

    // ---------- Leitura config file ---------- //
    char line[BUFFER_LEN];
    char *ptr;
    config_file = fopen(config_file_name, "r");
    
    if (config_file == NULL) {
        write_log("Error config config_file doesn't exist");
    }
    printf("a");
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

    printf("shm");
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
        char *token = strtok(temp_edge_server[i], ",");
        while (token != NULL) {
            edge_server[i][t] = token;
            ++t;
        }
        t = 0;
    }
    
    for (int i = 0; i < EDGE_SERVER_NUMBER; ++i) {
        shared_var->processing_power_vCPU1 = strtol(edge_server[i][1], &ptr, 10);
        shared_var->processing_power_vCPU2 = strtol(edge_server[i][2], &ptr, 10);
    }

    free(temp_shared_var);

    printf("meio");
    
    // ---------- Start task manager ---------- //

    if ((task_manager_id = fork()) == 0) {
        write_log("TASK MANAGER STARTING");
        Task_Manager();
    }
    else if (task_manager_id == -1) {
        write_log("ERROR STARTING TASK MANAGER PROCESS");
        exit(1);
    }

    // ---------- Start maintenance manager ---------- //

    if ((maintenance_manager_id = fork()) == 0) {
        write_log("MAINTENANCE MANAGER STARTING");
        Maintenance_Manager();
    }
    else if (maintenance_manager_id == -1) {
        write_log("ERROR CREATING MAINTENANCE MANAGER PROCESS");
        exit(1);
    }
    
    // ---------- Start monitor ---------- //

    if ((monitor_id = fork()) == 0) {
        write_log("MONITOR STARTING");
        Maintenance_Manager();
    }
    else if (monitor_id == -1) {
        write_log("ERROR CREATING MONITOR PROCESS");
        exit(1);
    }


    printf("adeus");

    return 0;
}
