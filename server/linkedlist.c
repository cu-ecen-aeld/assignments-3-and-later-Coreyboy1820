#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct Node {
    pthread_t value;
    bool *hasReturned;
    struct Node* next;
} Node;

typedef struct {
    Node* head;
    unsigned int length;
    pthread_mutex_t *listMutex;
} List;

bool list_push(List* l, pthread_t value, bool *hasReturned) {
    Node* n = (Node*)malloc(sizeof(Node));
    if (!n) return false;
    n->value = value;
    n->hasReturned = hasReturned;
    n->next  = l->head;
    pthread_mutex_lock(l->listMutex);
    l->head  = n;
    l->length++;
    pthread_mutex_unlock(l->listMutex);
    return true;
}

bool list_pop(List* l, pthread_t *value, bool **hasReturned) {
    if (!l->head) return false;
    pthread_mutex_lock(l->listMutex);
    Node* n = l->head;
    if (value) *value = n->value;
    if (hasReturned) *hasReturned = n->hasReturned;

    l->head = n->next;
    free(n);
    l->length--;
    pthread_mutex_unlock(l->listMutex);
    return true;
}

bool list_delete(List* l, pthread_t target) {
    Node** cur = &l->head;
    while (*cur) {
        if ((*cur)->value == target) {
            Node* dead = *cur;
            *cur = dead->next;
            free(dead);
            l->length--;
            return true;
        }
        cur = &(*cur)->next;
    }
    return false;
}

void list_clear(List* l) {
    Node* n = l->head;
    pthread_mutex_lock(l->listMutex);
    while (n) {
        Node* next = n->next;
        free(n);
        n = next;
    }
    l->head = NULL;
    pthread_mutex_unlock(l->listMutex);
}