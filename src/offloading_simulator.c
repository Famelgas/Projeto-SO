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


    for (int i = 0; i < EDGE_SERVER_NUMBER; ++i) {
        if (fork() == 0) {
            Edge_Server(i);

        }
        else {
            write_log("Error starting Edge Server process");
        }
        



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
                token = strtok(NULL, ";");
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

    }

    pthread_join(threads[0], NULL);
    pthread_join(threads[1], NULL);
    exit(0);
}


void Edge_Server(int id) {


    shared_var[id].tasks_completed = 0;
    shared_var[id].vCPU1_full = FREE;
    shared_var[id].vCPU2_full = FREE;



    char id_string[BUFFER_LEN];
    sprintf(id_string, "%d", id);

    while (end_processes == 1) {

        Task task;
        close(shared_var[id].fd_unnamed[1]);

        while (read(shared_var[id].fd_unnamed[0], &task, sizeof(task)) > 0) {
            
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


    }
    exit(0);
}


void Monitor() {
    while (end_processes == 1) {

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

    }
    exit(0);
}


void Maintenance_Manager() {
    int msg = rand() % EDGE_SERVER_NUMBER + 1;
    message_queue.msg = msg;

    while (end_processes == 1) {
        while (servers_down == EDGE_SERVER_NUMBER - 1) {
            continue;
        }
        
        msgsnd(mqid, &message_queue, sizeof(MessageQueue), 0);

        msgrcv(mqid, &message_queue, sizeof(MessageQueue), READY, 0);

        message_queue.msg = msg;
        msgsnd(mqid, &message_queue, sizeof(MessageQueue), 0);


        msg = rand() % EDGE_SERVER_NUMBER + 1;
        message_queue.msg = msg;
        sleep(rand() % 5 + 1);
 
    }
    exit(0);
}


// ---------- Threads ---------- //


void *slow_vCPU(int id) {
    while(1) {
        pthread_mutex_lock(&slow_vCPU_mutex);
        long wait = (shared_var[id].instruction_number * 1000) / (shared_var[id].processing_power_vCPU1 * 1000000);
        shared_var[id].vCPU1_full = FULL;
        sleep(wait);
        shared_var[id].vCPU1_full = FREE;
        pthread_mutex_unlock(&slow_vCPU_mutex);
        pthread_exit(NULL);
    }

    return NULL;
}


void *fast_vCPU(int id) {
    while(1) {
        pthread_mutex_lock(&fast_vCPU_mutex);
        long wait = (shared_var[id].instruction_number * 1000) / (shared_var[id].processing_power_vCPU2 * 1000000);
        shared_var[id].vCPU2_full = FULL;
        sleep(wait);
        shared_var[id].vCPU2_full = FREE;
        pthread_mutex_unlock(&fast_vCPU_mutex);
        pthread_exit(NULL);
    }

    return NULL;
}


void *thread_scheduler() {
    while (end_processes == 1) {
        for (int i = 0; i < QUEUE_POS; ++i) {
            if (task_queue[i].priority == 1) {
                int b = 1;

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

                for (int e = 0; e < EDGE_SERVER_NUMBER; ++e) {
                    if (task_queue[i].max_execution_time > shared_var[e].next_task_time_vCPU1) {
                        b = 0;
                    }

                    if (task_queue[i].max_execution_time > shared_var[e].next_task_time_vCPU2) {
                        b = 0;
                    }
                }

                
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

    for (int i = 0; i < EDGE_SERVER_NUMBER; ++i) {
        while (shared_var[i].performance > 0) {
            write_log("Task not completed");
            pthread_join(shared_var[i].slow_thread, NULL);
            pthread_join(shared_var[i].fast_thread, NULL);
        }
    }


    end_processes = 0;
    statistics(SIGTSTP);
    clean_resources();
    exit(0);
}



void statistics(int signum) {



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




}


void write_log(char *str) {
    fprintf(log_file, "%s\n", str);
    fflush(log_file);
}


void clean_resources() {
    fclose(config_file);
    fclose(log_file);

    shmdt(shared_var);
    shmctl(shmid, IPC_RMID, NULL);

    free(task_queue);

    msgctl(mqid, IPC_RMID, 0);
    unlink(TASK_PIPE);

    kill(0, SIGTERM);
    exit(0);
}


// ---------- Init SHM ---------- //

void init_shm() {
    if ((shmid = shmget(IPC_PRIVATE, (sizeof(EdgeServer) * EDGE_SERVER_NUMBER), IPC_CREAT | 0766)) < 0) {
        write_log("Shmget error");
        exit(0);
    }


    if ((shared_var = (EdgeServer *) shmat(shmid, NULL, 0)) == (EdgeServer *) - 1) {
        write_log("Shmat error");
        exit(1);
    }

}



// ---------- Main ---------- //



int main() {
    signal(SIGINT, sigint);
    signal(SIGTSTP, statistics);

    end_processes = 1;
    servers_down = 0;

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


    // ---------- Message Queue ---------- //

    
    if ((mqid = msgget(IPC_PRIVATE, IPC_CREAT|0777)) < 0) {
      write_log("Error creating message queue");
      exit(0);
    }



    // ---------- Leitura config file ---------- //
    
    char *buf = NULL;
    size_t len = 0;
    int count = 0;

    if ((config_file = fopen(config_file_name, "r")) == NULL) {
        write_log("Error config config_file doesn't exist");
    }

    //Leitura do config file
    while (getline(&buf, &len, config_file) != -1) {
        int i = 0;

        if (count < 3) {
            switch (count) {
                case 0:
                    QUEUE_POS = atoi(buf);
                    break;
                case 1:
                    MAX_WAIT = atoi(buf);
                    break;

                case 2:
                    EDGE_SERVER_NUMBER = atoi(buf);
                    if (EDGE_SERVER_NUMBER < 2) {
                        write_log("Error in config config_file: EDGE_SERVER_NUMBER < 2");
                        exit(1);
                    }

                    init_shm();
                    break;

                default:
                    write_log("Error reading config_file");
                    exit(1);
            }
        }
        else if (count >= 3) {
            char *token = strtok(buf, ",");
            while (token != NULL) {
                switch (i) {
                    case 0:
                        shared_var[i].name = token;
                        break;
                        
                    case 1:
                        shared_var[i].processing_power_vCPU1 = atoi(token);
                        break;

                    case 2:
                        shared_var[i].processing_power_vCPU2 = atoi(token);
                        break;

                    default:
                        write_log("Error reading config_file: server info");
                        exit(1);
                }
                i++;
                token = strtok(NULL, ",");
            }
        }
        else {
            break;
        }
        count++;
    }

    
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

    while (end_processes == 1) {
        signal(SIGINT, sigint);
        signal(SIGTSTP, statistics);
    }

    return 0;
}
