#include <pthread.h>
#include <stdio.h>
 
void* thread_func(void *arg)
{
    printf("thread id=%lu\n", pthread_self());
    return arg;
}
 
int main(void)
{
    pid_t pid;
    pthread_t tid;
    pid = getpid();
    printf("process id=%d\n", pid);
    pthread_create(&tid, NULL, thread_func, NULL);
    pthread_join(tid,NULL);
    return 0;
}
