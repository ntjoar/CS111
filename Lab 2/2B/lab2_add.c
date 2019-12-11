#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int iterc = 1;
static int yieldFlag = 0;
static char syncOpt = 'u';
static long long valTotal = 0;

static pthread_mutex_t mutexLock;
static int spinLock;

void add(long long *pointer, long long value) {
    long long sum = *pointer + value;
    if (yieldFlag)
        sched_yield();
    *pointer = sum;
}

void add2(long long val) {
    int i;
    long long old, new;
    for(i = 0; i < iterc; i++) { // Val is added x times
        switch(syncOpt){
            case 'u': // No Sync  
                add(&valTotal, val);
                break;
            case 'm': // Mutex protection
                pthread_mutex_lock(&mutexLock);
                add(&valTotal, val);
                pthread_mutex_unlock(&mutexLock);
                break;
            case 's': // Spin-lock protection
                while(__sync_lock_test_and_set(&spinLock, 1)) 
                    continue;
                add(&valTotal, val);
                __sync_lock_release(&spinLock);
                break;
            case 'c': // Compare and Swap protection
                for(;;) {
                    old = valTotal;
                    new = old + val;
                    if(__sync_val_compare_and_swap(&valTotal, old, new) == old)
                        break;
                }
                break;
            default:
                break;
        }
    }
}

void* run() {
    add2(1);
    add2(-1);
    return NULL;
}

int main(int argc, char *argv[]) {
    int c; // long_opt
    int i=0; // iterator
    int thrc = 1;

    struct timespec timeStart;
    struct timespec timeEnd;
    long long totalTime;

    char opt[16] = "add";
    while(1) {
        static struct option long_options[] =
        {
            {"threads", required_argument, 0, 't'},
            {"iterations", required_argument, 0, 'i'},
            {"sync", required_argument, 0, 'n'},
            {"yield", no_argument, 0, 'y'},
            {0, 0, 0, 0}
        };

        int option_index = 0;
        c = getopt_long (argc, argv, "", long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch(c) {
            case 't': // threads
                thrc = atoi(optarg);
                break;
            case 'i': // iterations
                iterc = atoi(optarg);
                break;
            case 'n': // sync
                syncOpt = optarg[0];
                if(syncOpt != 'm' && syncOpt != 's' && syncOpt != 'c') {
                    perror("Syntax Error: Unrecognized argument for sync.\n");
                    exit(1);
                }
                if(syncOpt == 'm') // Initialize mutex lock
                    if(pthread_mutex_init(&mutexLock, NULL)) {
                        perror("Error: pthread_mutex_init failed.");
                        exit(1);
                    }
                break;
            case 'y': // yield
                yieldFlag = 1;
                break;
            default: 
                fprintf(stderr, "Unrecognized argument. Usage: --threads --iterations --sync --yield only\n");
                exit(1);
                break;
        }
    }

    if(yieldFlag == 1)
        strcat(opt, "-yield");

    if(syncOpt == 'u')
        strcat(opt, "-none");
    else
        sprintf(opt, "%s-%c", opt, syncOpt);
    
    clock_gettime(CLOCK_MONOTONIC, &timeStart);
    pthread_t* tid = (pthread_t*) malloc(sizeof(pthread_t) * thrc);
    if(tid == NULL) {
        perror("Error: Thread allocation failed");
        exit(1);
    }

    for(i = 0; i < thrc; i++) // Create
        if(pthread_create(&tid[i], NULL, run, NULL)) {
            perror("Error: Thread creation failed");
            free(tid);
            exit(1);
        }

    for(i = 0; i < thrc; i++) // Join
        if(pthread_join(tid[i], NULL)) {
            perror("Error: Thread joining failed");
            free(tid);
            exit(1);
        }
    
    clock_gettime(CLOCK_MONOTONIC, &timeEnd);
    totalTime = (timeEnd.tv_sec - timeStart.tv_sec) * 1000000000 + (timeEnd.tv_nsec - timeStart.tv_nsec);
    long long opc = thrc * iterc * 2;

    printf("%s,%d,%d,%lld,%lld,%lld,%lld\n", opt, thrc, iterc, opc, totalTime, totalTime/opc, valTotal);

    free(tid);
    exit(valTotal != 0);
}