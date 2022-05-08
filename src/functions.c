// Filipe David Amado Mendes, 2020218797
// Miguel Ângelo Graça Meneses, 2020221791


#include "declarations.h"


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
