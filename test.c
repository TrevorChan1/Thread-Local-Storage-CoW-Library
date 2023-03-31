#include "tls.h"
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

void* test(void* arg){
    if (tls_create(100)) printf("bruh\n");
    if (tls_write(0, 7, "hello!")) printf("bruh\n");
    char m[7];
    if (tls_read(0, 7, m)) printf("bruh\n");
    printf("%s\n", m);

    return NULL;
}


int main(){
    pthread_t tid = NULL;
    if (pthread_create(&tid, NULL, test, NULL) != 0){
        perror("Error: ");
    }
    sleep(5);

}
