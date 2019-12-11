#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "SortedList.h"

/* Global Constants */
const unsigned int LEN_RAND = 3; // Adjust here later 

/* Sorted List Objects */
static SortedList_t list;
static SortedList_t *listArr;

/* Global Variables */
long long count;
static unsigned int iterc = 1;
static unsigned int elec = 0;
static int listc = 1;
static char syncOpt = 'u';
static int *stPos;
int opt_yield = 0;

/* Locking variables */
static pthread_mutex_t mutexLock;
static int spinLock;

void init(unsigned int thrc) {
    unsigned int i, j;
    srand((unsigned int) time(NULL));
    for(i = 0; i < elec; i++) {
        char* key = malloc(sizeof(char) * (LEN_RAND + 1));
        for(j = 0; j < LEN_RAND; j++)
            key[j] = (char) rand() % 26 + 'A';
        key[LEN_RAND] = '\0';
        listArr[i].key = key;
    }

    stPos = malloc(sizeof(int) * thrc);
    for(i = 0, j = 0; i < thrc; i++, j += iterc)
        stPos[i] = j;
}

void chDel(unsigned int pos) {
    SortedListElement_t *e = SortedList_lookup(&list, listArr[pos].key);
    if(e == NULL || SortedList_delete(e) != 0) {
        fprintf(stderr, "Error: SortedList_lookup & delete: list is corrupted.\n");
        exit(EXIT_FAILURE);
    }
}

void* run(void* pv) {
    int r;
    unsigned int i = 0;
    unsigned int st = *((int *) pv);
    unsigned int ub = st + iterc;

    /* Insert */
    for(i = st; i < ub; i++) { // Val is added x times
        switch(syncOpt){
            case 'u': // No Sync  
                SortedList_insert(&list, &listArr[i]);
                break;
            case 'm': // Mutex protection
                pthread_mutex_lock(&mutexLock);
                SortedList_insert(&list, &listArr[i]);
                pthread_mutex_unlock(&mutexLock);
                break;
            case 's': // Spin-lock protection
                while(__sync_lock_test_and_set(&spinLock, 1)) 
                    continue;
                SortedList_insert(&list, &listArr[i]);
                __sync_lock_release(&spinLock);
                break;
            default:
                break;
        }
    }

    /* List Length */
    switch(syncOpt){
        case 'u': // No Sync  
            r = SortedList_length(&list);
            if(r < 0) {
                fprintf(stderr, "Error: List corruption.\n");
                exit(EXIT_FAILURE);
            }
            break;
        case 'm': // Mutex protection
            pthread_mutex_lock(&mutexLock);
            r = SortedList_length(&list);
            pthread_mutex_unlock(&mutexLock);
            if(r < 0) {
                fprintf(stderr, "Error: List corruption.\n");
                exit(EXIT_FAILURE);
            }
            break;
        case 's': // Spin-lock protection
            while(__sync_lock_test_and_set(&spinLock, 1)) 
                continue;
            r = SortedList_length(&list);
            __sync_lock_release(&spinLock);
            if(r < 0) {
                fprintf(stderr, "Error: List corruption.\n");
                exit(EXIT_FAILURE);
            }
            break;
        default:
            break;
    }

    /* Delete */
    for(i = st; i < ub; i++) { // Val is added x times
        switch(syncOpt){
            case 'u': // No Sync  
                chDel(i);
                break;
            case 'm': // Mutex protection
                pthread_mutex_lock(&mutexLock);
                chDel(i);
                pthread_mutex_unlock(&mutexLock);
                break;
            case 's': // Spin-lock protection
                while(__sync_lock_test_and_set(&spinLock, 1)) 
                    continue;
                chDel(i);
                __sync_lock_release(&spinLock);
                break;
            default:
                break;
        }
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    int c; // long_opt
    unsigned int i, j; // iterator
    unsigned int thrc = 1;

    struct timespec timeStart;
    struct timespec timeEnd;
    long long totalTime;

    char opt[32] = "list";

    while(1) {
        static struct option long_options[] =
        {
            {"threads", required_argument, 0, 't'},
            {"iterations", required_argument, 0, 'i'},
            {"sync", required_argument, 0, 'n'},
            {"yield", required_argument, 0, 'y'},
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
                if(syncOpt != 'm' && syncOpt != 's') {
                    fprintf(stderr, "Syntax Error: Unrecognized argument for sync.\n");
                    exit(1);
                }
                if(syncOpt == 'm') // Initialize mutex lock
                    if(pthread_mutex_init(&mutexLock, NULL)) {
                        perror("Error: pthread_mutex_init failed.\n");
                        exit(1);
                    }
                break;
            case 'y': // yield
                j = strlen(optarg);
                if(j > 3) {
                    fprintf(stderr, "Error: Yield argument not recognized %s", optarg);
                    exit(EXIT_FAILURE);
                }
                for(i = 0; i < j; i++) {
                    if(optarg[i] == 'i') {
                        opt_yield = opt_yield | INSERT_YIELD;
                    } else if(optarg[i] == 'd') {
                        opt_yield = opt_yield | DELETE_YIELD;
                    } else if(optarg[i] == 'l') {
                        opt_yield = opt_yield | LOOKUP_YIELD;
                    } else {
                        fprintf(stderr, "Error: Yield argument not recognized: %c\n", optarg[i]);
                        exit(EXIT_FAILURE);
                    }
                }
                break;
            default: 
                fprintf(stderr, "Unrecognized argument. Usage: --threads --iterations --sync --yield only\n");
                exit(1);
                break;
        }
    }

    /* String dev */
    switch (opt_yield) {
        case INSERT_YIELD:
            strcat(opt, "-i");
            break;
        case DELETE_YIELD:
            strcat(opt, "-d");
            break;
        case LOOKUP_YIELD:
            strcat(opt, "-l");
            break;
        case INSERT_YIELD | DELETE_YIELD:
            strcat(opt, "-id");
            break;
        case INSERT_YIELD | LOOKUP_YIELD:
            strcat(opt, "-il");
            break;
        case DELETE_YIELD | LOOKUP_YIELD:
            strcat(opt, "-dl");
            break;
        case INSERT_YIELD | DELETE_YIELD | LOOKUP_YIELD:
            strcat(opt, "-idl");
            break;
        default:
            strcat(opt, "-none");
            break;
    }
    if(syncOpt == 'u')
        strcat(opt, "-none");
    else
        sprintf(opt, "%s-%c", opt, syncOpt);
      
    /* Allocate memory */
    SortedList_t* head = &list;
    list.key = NULL;
    list.next = head;
    list.prev = head;

    /* Allocate memory */
    elec = thrc * iterc;
    listArr = malloc(sizeof(SortedListElement_t) * elec);

    /* Initialize and start timer */
    init(thrc);
    clock_gettime(CLOCK_MONOTONIC, &timeStart);

    /* Process */
    pthread_t* tid = (pthread_t*) malloc(sizeof(pthread_t) * thrc);
    if(tid == NULL) {
        fprintf(stderr, "Error: Thread allocation failed\n");
        free(listArr);
        free(stPos);
        exit(EXIT_FAILURE);
    }

    for(i = 0, j = 0; i < thrc; i++, j += iterc) { // Create
        stPos[i] = j;
        if(pthread_create(&tid[i], NULL, run, (void *) &stPos[i])) {
            fprintf(stderr, "Error: Thread creation failed\n");
            free(tid);
            free(listArr);
            free(stPos);
            exit(EXIT_FAILURE);
        }
    }

    for(i = 0; i < thrc; i++) // Join
        if(pthread_join(tid[i], NULL)) {
            fprintf(stderr, "Error: Thread joining failed\n");
            free(tid);
            free(listArr);
            free(stPos);
            exit(EXIT_FAILURE);
        }

    /* Time Calc */
    clock_gettime(CLOCK_MONOTONIC, &timeEnd);
    totalTime = (timeEnd.tv_sec - timeStart.tv_sec) * 1000000000 + (timeEnd.tv_nsec - timeStart.tv_nsec);
    long long opc = thrc * iterc * 3;

    /* Error Checking */
    if(SortedList_length(&list) != 0) {
        if(SortedList_length(&list) > 0)
            fprintf(stderr, "Error: List not fully deleted\n");
        else
            fprintf(stderr, "Error: List corrupted\n");
        free(tid);
        free(listArr);
        free(stPos);
        exit(EXIT_FAILURE);
    }

    /* Print Val */
    printf("%s,%d,%d,%d,%lld,%lld,%lld\n", opt, thrc, iterc, listc, opc, totalTime, totalTime/opc);

    /* Free Memory */
    free(tid);
    free(listArr);
    free(stPos);

    exit(EXIT_SUCCESS);
}