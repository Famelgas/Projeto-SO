// Filipe David Amado Mendes, 2020218797
// Miguel Ângelo Graça Meneses, 2020221791


#include "declarations.h"



void Task_Manager(long QUEUE_POS, long EDGE_SERVER_NUMBER, char *edge_server[EDGE_SERVER_NUMBER][3]) {
    task_queue = malloc(sizeof(Task) * QUEUE_POS);
    for (size_t i = 0; i < sizeof(Task) * QUEUE_POS; i + sizeof(Task)) {
        task_queue[i].task_id = -1;
        task_queue[i].priority = -1;
        task_queue[i].instruction_number = 0;
        task_queue[i].max_execution_time = 1000000;
    }
    
    
    while (end_processes == 1) {
        char *stack[QUEUE_POS];
        int fd;
        int fd_unnamed[EDGE_SERVER_NUMBER][2];
        char string[BUFFER_LEN];
        char *SHM;
        Task task;



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

        char *str;
        while (read(fd_task_pipe, &str, BUFFER_LEN)) {
            if (str == NULL) {
                write_log("Error reading from TASK_PIPE");
            }
            int t = 0;
            char *str_task;
            char *token = strtok(str, ";");
            
            while (token != NULL) {
                str_task[t] = token;
                token = strtok(str, ";");
            }

            task.task_id = str_task[0];
            task.instruction_number = str_task[1];
            task.max_execution_time = str_task[2];
            
            for (size_t i = 0; i < sizeof(Task) * QUEUE_POS; i + sizeof(Task)) {
                if (task_queue[i].task_id == -1) {
                    task_queue[i] = task;
                }
            }
            
            str = "";
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
                Edge_Server(i);
            }
        
        }
    }
}


void Edge_Server(int id) {
    struct timespec wait = {0, 0};
    pthread_cond_t cond; 
    pthread_cond_init(&cond, NULL);
    while (end_processes == 1) {
        char *task;
        char *list[3];
        char *id_string[BUFFER_LEN];
        sprintf(id_string, "%ld", id);

        if ((shared_var = (EdgeServer *) shmat(shmid, NULL, 0)) == (struct EdgeServer *) -1) {
            writ_log_ecra("Shmat error!");
            exit(1);
        }

        shared_var[id].vCPU1_full = FREE;
        shared_var[id].vCPU2_full = FREE;



        while (read(shared_var[id].fd_unnamed[0], NULL, sizeof(NULL)) > 0) {
            if (message_queue != NULL && message_queue->string == id_string) {
                message_queue = message_queue->previous;

                while (shared_var[id].vCPU1_full != FREE || shared_var[id].vCPU2_full != FREE) {
                    continue;
                }

                if (shared_var[id].vCPU1_full == FREE && shared_var[id].vCPU2_full == FREE) {
                    message_queue->string = "ready";
                    message_queue->previous = message_queue;
                    message_queue = message_queue->next;
                    
                    shared_var[id].performance = STOPPED;
                    write_log("Maintenance");
                    wait.tv_sec = time(NULL) + (rand() % 5 + 1);
                    pthread_cond_timedwait(&cond, &shared_var[id].slow_thread, &wait);

                    message_queue->previous = message_queue;
                    message_queue = message_queue->next;

                }
            }

            


            if (read(shared_var[id].fd_unnamed, NULL, sizeof(NULL)) > 0) {
                pthread_create(&shared_var[id].slow_thread, NULL, slow_vCPU, NULL);
                pthread_join(&shared_var[id].slow_thread, NULL);
            }
            if (read(shared_var[id].fd_unnamed, NULL, sizeof(NULL)) > 0) {
                pthread_create(&shared_var[id].fast_thread, NULL, fast_vCPU, NULL);
                pthread_join(shared_var[id].fast_thread, NULL);
            } else {
                continue;
            }
        }


        pthread_exit(NULL);
    }
}


void Monitor() {
    while (end_processes == 1) {
        if ((sizeof(task_queue) / sizeof(Task) * QUEUE_POS) > (0.8 * (sizeof(Task) * QUEUE_POS)) && task_queue[0].max_execution_time > MAX_WAIT) {
            while ((sizeof(task_queue) / sizeof(Task) * QUEUE_POS) > (0.2 * sizeof(Task) * QUEUE_POS)) {
                for (int i = 0; i < EDGE_SERVER_NUMBER; ++i) {
                    shared_var[i].performance = HIGH;
                }
            }
        }
        else if ((sizeof(task_queue) / sizeof(Task) * QUEUE_POS) < (0.2 * (sizeof(Task) * QUEUE_POS)) {
            for (int i = 0; i < EDGE_SERVER_NUMBER; ++i) {
                    shared_var[i].performance = NORMAL;
                }
        }

        else {
            continue;
        }

    }
}


void Maintenance_Manager() {
    while (end_processes == 1) {
        pthread_mutex_lock(&mutex);
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
        pthread_mutex_unlock(&mutex);
        
        //int previous_performance = shared_var[server].performance;
        //shared_var[server].performance = 0;


        // shared_var[server].performance = previous_performance;

        
    }
}



void *slow_vCPU(int id) {
    struct timespec wait = {0, 0};
    pthread_cond_t cond; 
    pthread_cond_init(&cond, NULL);

    while(1) {
        wait.tv_sec = time(NULL) + (shared_var[id].instruction_number * 1000) / (shared_var[id].processing_power_vCPU2 * 1000000);
        shared_var[id].vCPU1_full = FULL;
        pthread_cond_timedwait(&cond, &shared_var[id].slow_thread, &wait);
        shared_var[id].vCPU1_full = FREE;
    }

    pthread_exit(NULL);
}


void *fast_vCPU(int id) {
    struct timespec wait = {0, 0};
    pthread_cond_t cond; 
    pthread_cond_init(&cond, NULL);

    while(1) {
        wait.tv_sec = time(NULL) + (shared_var[id].instruction_number * 1000) / (shared_var[id].processing_power_vCPU2 * 1000000);
        shared_var[id].vCPU2_full = FULL;
        pthread_cond_timedwait(&cond, &shared_var[id].fast_thread, &wait);
        shared_var[id].vCPU2_full = FREE;
    }

    pthread_exit(NULL);
}


void *thread_scheduler() {
    for (size_t i = 0; i < sizeof(Task) * QUEUE_POS; i + sizeof(Task)) {
        if (task_queue[i].priority == 1) {
            int b = 1;
            for (size_t e = 0; e < EDGE_SERVER_NUMBER; e + sizeof(EdgeServer)) {
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
    size_t j;
    for (size_t i = 0 + sizeof(Task); i < sizeof(Task) * QUEUE_POS; i + sizeof(Task)) {
        key = task_queue[i];
        j = i - sizeof(Task);

        while (j >= 0 && task_queue[j].max_execution_time > key.max_execution_time) {
            task_queue[j + sizeof(Task)] = task_queue[j];
            j = j - sizeof(Task);
        }
        task_queue[j + sizeof(Task)] = key;
    }

    int p = 0;
    for (size_t i = 0; i < sizeof(Task) * QUEUE_POS; i + sizeof(Task)) {
        task_queue[i].priority = p;
    }
}


void *thread_dispatcher() {
    for (size_t i = 0; i < sizeof(Task) * QUEUE_POS; i + sizeof(Task)) {
        if (task_queue[i].priority == 1) {
            int b = 1;
            for (size_t e = 0; e < EDGE_SERVER_NUMBER; e + sizeof(EdgeServer)) {
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
