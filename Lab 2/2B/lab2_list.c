#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "SortedList.h"

typedef struct {
    SortedList_t list;
    pthread_mutex_t lock;
    int spinlock;
} SubList_t;

/* Global Constants */
const int LEN_RAND = 3; // Adjust here later 

/* Sorted List Objects */
static SubList_t *list;
static SortedList_t *listArr;

/* Global Variables */
static int iterc = 1;
static unsigned int elec = 0;
static int listc = 1;
static char syncOpt = 'u';
static int *stPos = NULL;
static long long *waitTime = NULL;
int opt_yield = 0;

//  djb2 hash function: adopted from:
//  http://www.cse.yorku.ca/~oz/hash.html
unsigned long hash(const char *key) {
    unsigned long val = 5381;
    for (int i = 0; i < LEN_RAND; i++)
        val = ((val << 5) + val) + key[i];
    return val;
}

long long time_diff(struct timespec *start, struct timespec *end) {
    long long total = (end->tv_sec - start->tv_sec) * 1000000000;
    total += end->tv_nsec;
    total -= start->tv_nsec;
    return total;
}

void init(int thrc) {
    if ((list = malloc(sizeof(SubList_t) * listc)) == NULL) {
        perror("*** Error: malloc for list failed.\n");
        exit(EXIT_FAILURE);
    }

    //  Initialize empty sublist
    for (int i = 0; i < listc; i++) {
        SubList_t *sublist = &list[i];
        SortedList_t* list = &sublist->list;
        list->key = NULL;
        list->next = list;
        list->prev = list;
        if (syncOpt == 's') {
            sublist->spinlock = 0;
        } else if (syncOpt == 'm') {  //  Init lock
            if (pthread_mutex_init(&sublist->lock, NULL)) {
                perror("*** Error: pthread_mutex_init failed.\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    //  Create an thrc * iterc number of list elements
    elec = thrc * iterc;
    if ((listArr = malloc(sizeof(SortedListElement_t) * elec)) == NULL) {
        perror("*** Error: malloc for listArr failed.\n");
        exit(EXIT_FAILURE);
    }

    //  Generate keys
    srand((unsigned int) time(NULL));
    for (unsigned int i = 0; i < elec; i++) {
        char *key = malloc(sizeof(char) * (LEN_RAND + 1));  //  This is not freed
        for (int i = 0; i < LEN_RAND; i++)
            key[i] = (char) rand() % 26 + 'A';  //  random ASCII chars
        key[LEN_RAND] = '\0';
        listArr[i].key = key;
    }

    stPos = malloc(sizeof(int) * thrc);
    for (int i = 0, val = 0; i < thrc; i++, val += iterc)
        stPos[i] = val;

    if (syncOpt == 'm') {
        //  Create wait time array
        waitTime = malloc(sizeof(long long) * thrc);
        for (int i = 0; i < thrc; i++)
            waitTime[i] = 0;
    }
}

void chDel(SortedList_t *list, const char *key) {
    SortedListElement_t *e = SortedList_lookup(list, key);
    if(e == NULL || SortedList_delete(e) != 0) {
        fprintf(stderr, "Error: SortedList_lookup & delete: list is corrupted.\n");
        exit(EXIT_FAILURE);
    }
}

void* run(void* pv) {
    unsigned int start_pos = *((int *) pv);
    int tid = start_pos / iterc;
    unsigned int upper_bound = start_pos + iterc;
    struct timespec start_time, end_time;
    SortedListElement_t *ele;
    SubList_t *sublist;
    pthread_mutex_t *lock;
    int *spinlock;

    for (unsigned int i = start_pos; i < upper_bound; i++) {
        ele = &listArr[i];
        const char *key = ele->key;
        sublist = &list[hash(key) % listc];
        switch (syncOpt) {
            case 'u':   SortedList_insert(&sublist->list, ele);   break;
            case 'm':
                lock = &sublist->lock;
                clock_gettime(CLOCK_MONOTONIC, &start_time);
                pthread_mutex_lock(lock);
                clock_gettime(CLOCK_MONOTONIC, &end_time);
                waitTime[tid] += time_diff(&start_time, &end_time);
                SortedList_insert(&sublist->list, ele);
                pthread_mutex_unlock(lock);
                break;
            case 's':
                spinlock = &sublist->spinlock;
                while (__sync_lock_test_and_set(spinlock, 1))
                    continue;  //  spining
                SortedList_insert(&sublist->list, ele);
                __sync_lock_release(spinlock);
                break;
            default:
                break;
        }
    }

    //  List Length
    int r = 0;
    switch (syncOpt) {
        case 'u':
            for (int i = 0; i < listc; i++) {
                if ((r = SortedList_length(&list[i].list)) < 0)
                    break;
            }
            break;
        case 'm':
            clock_gettime(CLOCK_MONOTONIC, &start_time);
            for (int i = 0; i < listc; i++)
                pthread_mutex_lock(&list[i].lock);
            clock_gettime(CLOCK_MONOTONIC, &end_time);
            waitTime[tid] += time_diff(&start_time, &end_time);
            for (int i = 0; i < listc; i++)
                if ((r = SortedList_length(&list[i].list)) < 0)
                    break;
            for (int i = 0; i < listc; i++)
                pthread_mutex_unlock(&list[i].lock);
            break;
        case 's':
            for (int i = 0; i < listc; i++) {
                while (__sync_lock_test_and_set(&list[i].spinlock, 1))
                    continue;
            }
            for (int i = 0; i < listc; i++) {
                if ((r = SortedList_length(&list[i].list)) < 0)
                    break;
            }
            for (int i = 0; i < listc; i++)
                __sync_lock_release(&list[i].spinlock);
            break;
        default:
            break;
    }
    if (r < 0) {  //  Check for list corruption
        fprintf(stderr, "*** Error: SortedList_length: list is corrupted.\n");
        exit(EXIT_FAILURE);
    }

    //  Delete
    for (unsigned int i = start_pos; i < upper_bound; i++) {
        ele = &listArr[i];
        const char *key = ele->key;
        sublist = &list[hash(key) % listc];
        switch (syncOpt) {
            case 'u':   chDel(&sublist->list, key);    break;
            case 'm':
                lock = &sublist->lock;
                clock_gettime(CLOCK_MONOTONIC, &start_time);
                pthread_mutex_lock(lock);
                clock_gettime(CLOCK_MONOTONIC, &end_time);
                waitTime[tid] += time_diff(&start_time, &end_time);
                chDel(&sublist->list, key);
                pthread_mutex_unlock(lock);
                break;
            case 's':
                spinlock = &sublist->spinlock;
                while (__sync_lock_test_and_set(spinlock, 1))
                    continue;  //  spining
                chDel(&sublist->list, key);
                __sync_lock_release(spinlock);
                break;
            default:
                break;
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    int c; // long_opt
    int i, j; // iterator
    int thrc = 1;

    struct timespec timeStart;
    struct timespec timeEnd;
    long long totalTime;

    char opt[32] = "list";

    while(1) {
        static struct option long_options[] =
        {
            {"threads", required_argument, 0, 't'},
            {"iterations", required_argument, 0, 'i'},
            {"lists", required_argument, 0, 'l'},
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
                break;
            case 'l':
                listc = atoi(optarg);
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
    totalTime = time_diff(&timeStart, &timeEnd);
    long long opc = thrc * iterc * 3;

    /* Error Checking */
    int len = 0;
    int r;
    for (i = 0, r = 0; i < listc; i++) {
        r = SortedList_length(&list[i].list);
        if (r < 0) {
            fprintf(stderr, "Error: List corrupted.\n");
            free(tid);
            free(listArr);
            free(stPos);
            free(waitTime);
            exit(EXIT_FAILURE);
        }
        len += r;
    }
    if(len != 0) {
        free(tid);
        free(listArr);
        free(stPos);
        free(waitTime);
        exit(EXIT_FAILURE);
    }

    /* Print Val */
    if(syncOpt != 'm')
        printf("%s,%d,%d,%d,%lld,%lld,%lld,0\n", opt, thrc, iterc, listc, opc, totalTime, totalTime/opc);
    else {
        long long total = 0;
        for (i = 0; i < thrc; i++)
            total += waitTime[i];
        printf("%s,%d,%d,%d,%lld,%lld,%lld,%lld\n", opt, thrc, iterc, 
        listc, opc, totalTime, totalTime/opc, total / ((iterc * 2 + 1) * thrc));
    }

    /* Free Memory */
    free(tid);
    free(listArr);
    free(stPos);
    free(waitTime);

    exit(EXIT_SUCCESS);
}