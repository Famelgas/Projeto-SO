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
            exit(0);
        }
    }

    // task_pipe read only
    if ((fd_task_pipe = open(TASK_PIPE, O_RDWR | O_NONBLOCK)) < 0) {
        write_log("Error opening TASK_PIPE for reading");
        exit(0);
    }

    pthread_create(&threads[0], NULL, thread_scheduler, &thread_id[0]);
    pthread_create(&threads[1], NULL, thread_dispatcher, &thread_id[1]);

    while (end_processes == 1) {
        Task task;
        char *str;
        char *ptr;
	
        while (read(fd_task_pipe, &str, BUFFER_LEN) > 0) {
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
                    write_log("Task Queue is full");
                    break;
                }
                if (task_queue[i].task_id == -1) {
                    task_queue[i] = task;
                }
            }
            
            str = "";
        }
        
        
	    write_screen("Edge Server running");
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

    write_log("Edge Server created");

    shared_var[id].tasks_completed = 0;
    shared_var[id].vCPU1_full = FREE;
    shared_var[id].vCPU2_full = FREE;
	
    statistics(SIGTSTP);

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
	printf("ep: %d", end_processes);
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
    
    char total[128];
    sprintf(total, "%ld", stats.tasks_completed);
    char str1[] = "Total tasks completed: ";
    strcat(str1, total);
    write_screen(str1);
    
    
    char average[128];
    sprintf(average, "%ld", stats.average_response_time);
    char str2[] = "Average task response time: ";
    strcat(str2, average);
    write_screen(str2);


    for (int i = 0; i < EDGE_SERVER_NUMBER; ++i) {
    	char total_es[128];
    	sprintf(total_es, "%ld", shared_var[i].tasks_completed);
    	char str3[] = "Tasks completed by edge server ";
    	strcat(str3, shared_var[i].name);
    	strcat(str3, ": ");
    	strcat(str3, total_es);
        write_screen(str3);
    }
    
    
    char non_completed[128];
    sprintf(non_completed, "%ld", stats.non_completed_tasks);
    char str4[] = "Non completed tasks: ";
    strcat(str4, non_completed);
    write_screen(str4);
}


void write_log(char *str) {
    fprintf(log_file, "%s\n", str);
    fflush(log_file);
}

void write_screen(char *str) {
	fprintf(stdout, "%s\n", str);
}


void clean_resources() {
    fclose(config_file);
    fclose(log_file);

    shmdt(shared_var);
    shmctl(shmid, IPC_RMID, NULL);

    free(task_queue);

    msgctl(mqid, IPC_RMID, 0);
    unlink(TASK_PIPE);

    exit(0);
}


// ---------- Init SHM ---------- //

void init_shm() {
    if ((shmid = shmget(IPC_PRIVATE, (sizeof(EdgeServer) * EDGE_SERVER_NUMBER + end_processes), IPC_CREAT | 0644 | IPC_EXCL)) < 0) {
        write_log("Shmget error");
        exit(0);
    }


    if ((shared_var = (EdgeServer *) shmat(shmid, NULL, 0)) == (EdgeServer *) - 1) {
        write_log("Shmat error");
        exit(1);
    }

}



// ---------- Main ---------- //



int main(int argc, char *argv[]) {
    signal(SIGINT, sigint);
    signal(SIGTSTP, statistics);

    
    servers_down = 0;

    // open log file
    log_file = fopen(log_file_name, "a");
    if (log_file == NULL)
    {
        write_log("Error opening log file\n");
        exit(1);
    }

    // ---------- Default stats ---------- //

    stats.tasks_completed = 0;
    stats.average_response_time = 0;
    stats.non_completed_tasks = 0;

    // ---------- Task Pipe ---------- //

    unlink(TASK_PIPE);
    if ((mkfifo(TASK_PIPE, O_CREAT|O_EXCL|0600)<0) && (errno!= EEXIST)) { 
        write_log("Error creating TASK_PIPE");
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

    if ((config_file = fopen(argv[1], "r")) == NULL) {
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

    end_processes = 1;
    // ---------- Start task manager ---------- //

    if (fork() == 0) {
        Task_Manager();
        exit(0);
    }

    write_log("TASK MANAGER STARTING");
    // ---------- Start maintenance manager ---------- //

    if (fork() == 0) {
        Maintenance_Manager();
        exit(0);
    }
    write_log("MAINTENANCE MANAGER STARTING");
    
    // ---------- Start monitor ---------- //

    if (fork() == 0) {
        Monitor();
        exit(0);
    }
	write_log("MONITOR STARTING");

}