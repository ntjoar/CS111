#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "SortedList.h"

void SortedList_insert(SortedList_t *list, SortedListElement_t *element) {
    if(list == NULL || element == NULL)
        return;

    SortedList_t *before = list;
    SortedList_t *cur = before->next;
    if(before->key != NULL)
        return;
    
    while(cur != list) { // Iterate 
        if (strcmp(cur->key, element->key) < 0)
            break;
        before = cur;
        cur = cur->next;
    }

    if(opt_yield & INSERT_YIELD)
        pthread_yield();

    element->next = before->next;
    element->prev = before;
    before->next = element;
    element->next->prev = element;
}

int SortedList_delete(SortedListElement_t *element) {
    if(element == NULL || element->key == NULL)
        return 1; 
    if(element->prev->next != element || element->next->prev != element)
        return 1;

    if(opt_yield & DELETE_YIELD)
        pthread_yield();

    element->prev->next = element->next;
    element->next->prev = element->prev;
    return 0;
}

SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key) {
    if(list == NULL || list->key != NULL) 
        return NULL; // Head is missing
    
    SortedList_t *cur = list->next;

    while(cur != list) {
        if(!strcmp(key, cur->key))
            return cur;
        if(opt_yield & INSERT_YIELD)
            pthread_yield();
        cur = cur->next;
    }

    return NULL;
}

int SortedList_length(SortedList_t *list) {
    if(list == NULL || list->key != NULL) 
        return -1; // Head is missing
    SortedList_t *cur = list->next;
    int i = 0;

    while(cur != list) {
        if(cur->prev->next != cur || cur->next->prev != cur)
            return -1; // Corruption
        i++;
        if(opt_yield & LOOKUP_YIELD)
            pthread_yield();
        cur = cur->next;
    }
    return i;
}