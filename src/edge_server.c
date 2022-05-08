// Filipe David Amado Mendes, 2020218797
// Miguel Ângelo Graça Meneses, 2020221791


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "declarations.h"


void *slow_thread() {
    while(1) {
        printf("Slowww thread\n");
        sleep(3);
    }
}


void *fast_thread() {
    while(1) {
        printf("Fast thread\n");
        sleep(1);
    }
}


int main() {
    pthread_t slow_thr, fast_thr;

    
    
    pthread_create(&slow_thr, NULL, slow_thread, NULL);
    pthread_create(&fast_thr, NULL, fast_thread, NULL);


    pthread_exit(NULL);
    return 0;
}
