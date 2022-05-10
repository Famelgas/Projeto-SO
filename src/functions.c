// Filipe David Amado Mendes, 2020218797
// Miguel Ângelo Graça Meneses, 2020221791


#include "declarations.h"


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
}



void statistics(int signum) {
    write_log("SIGSTP signal recieved");


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
    free(shared_var);
    unlink(TASK_PIPE);
    sem_close(writing_sem);
}
