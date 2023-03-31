#include "tls.h"
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

void* test(void* arg){
    if (tls_create(100)) printf("bruh\n");
    if (tls_write(0, 7, "hello!")) printf("bruh\n");
    char m[7];
    if (tls_read(0, 7, m)) printf("bruh\n");
    printf("thread 1: %s\n", m);

    sleep(2);

    if (tls_read(0, 7, m)) printf("bruh\n");
    printf("thread 1: %s\n", m);

    return NULL;
}

void* test2(void* arg){
    sleep(1);
    pthread_t * pid = arg;
    if (tls_clone(*pid)) printf("bruh\n");
    char m[7];
    if (tls_read(0, 7, m)) printf("bruh\n");
    printf("thread 2:%s\n", m);

    if (tls_write(0, 7, "hewwo?")) printf("bruh\n");
    if (tls_read(0, 7, m)) printf("bruh\n");
    printf("thread 2:%s\n", m);


    return NULL;
}


int main(){
    pthread_t tid = NULL;
    pthread_t tid2 = NULL;
    if (pthread_create(&tid2, NULL, test2, &tid) != 0){
        perror("Error: ");
    }

    if (pthread_create(&tid, NULL, test, NULL) != 0){
        perror("Error: ");
    }

    sleep(5);

}
