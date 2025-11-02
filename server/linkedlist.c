#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>


typedef struct Node {
    int value;
    bool *hasReturned;
    struct Node* next;
} Node;

typedef struct {
    Node* head;
    unsigned int length;
} List;

bool list_push(List* l, int value, bool *hasReturned) {
    Node* n = (Node*)malloc(sizeof(Node));
    if (!n) return false;
    n->value = value;
    n->hasReturned = hasReturned;
    n->next  = l->head;
    l->head  = n;
    l->length++;
    return true;
}

bool list_pop(List* l, int *value, bool **hasReturned) {
    if (!l->head) return false;
    Node* n = l->head;
    if (value) *value = n->value;
    if (hasReturned) *hasReturned = n->hasReturned;

    l->head = n->next;
    free(n);
    l->length--;
    return true;
}

bool list_delete(List* l, int target) {
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
    while (n) {
        Node* next = n->next;
        free(n);
        n = next;
    }
    l->head = NULL;
}