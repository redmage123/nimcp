/*
 * linked_list.c
 * Teaches: a singly-linked list of ints with head/tail insert and full
 * teardown. Shows malloc + free pairing, struct self-reference, and
 * the head-pointer pattern. Intermediate sample.
 * Build: gcc -Wall -o linked_list linked_list.c
 * Run:   ./linked_list
 */

#include <stdio.h>
#include <stdlib.h>

/* `struct node` is self-referential: each node holds an int and a
 * pointer to the next node in the chain (or NULL at the end). */
struct node {
    int value;
    struct node *next;
};

/* Allocate a single node on the heap. Caller is responsible for
 * eventually free()-ing it (typically via list_free below). */
static struct node *node_new(int value)
{
    struct node *n = (struct node *)malloc(sizeof(*n));
    if (n == NULL) {
        /* Out of memory. A teaching-grade program just exits; a real
         * library would propagate an error code. */
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    n->value = value;
    n->next = NULL;
    return n;
}

/* Prepend `value`. The list is identified by a pointer-to-pointer
 * because we may need to update the head itself. */
static void list_insert_head(struct node **head, int value)
{
    struct node *n = node_new(value);
    n->next = *head;   /* old head becomes second node */
    *head = n;
}

/* Append `value`. O(n) because we walk to the last node. */
static void list_insert_tail(struct node **head, int value)
{
    struct node *n = node_new(value);
    if (*head == NULL) {
        *head = n;
        return;
    }
    struct node *cursor = *head;
    while (cursor->next != NULL) {
        cursor = cursor->next;
    }
    cursor->next = n;
}

/* Print the values "[a, b, c]"-style. */
static void list_print(const struct node *head)
{
    printf("[");
    const struct node *cursor = head;
    while (cursor != NULL) {
        printf("%d", cursor->value);
        if (cursor->next != NULL) {
            printf(", ");
        }
        cursor = cursor->next;
    }
    printf("]\n");
}

/* Free every node in the chain. After this call, the caller's head
 * pointer is dangling; we set it to NULL via the pointer-to-pointer. */
static void list_free(struct node **head)
{
    struct node *cursor = *head;
    while (cursor != NULL) {
        struct node *doomed = cursor;
        cursor = cursor->next;
        free(doomed);  /* always pair every malloc with exactly one free */
    }
    *head = NULL;
}

int main(void)
{
    struct node *head = NULL;

    /* Build [1, 2, 3] by appending. */
    list_insert_tail(&head, 1);
    list_insert_tail(&head, 2);
    list_insert_tail(&head, 3);
    printf("after appends: ");
    list_print(head);

    /* Prepend 0 -> [0, 1, 2, 3]. */
    list_insert_head(&head, 0);
    printf("after head insert: ");
    list_print(head);

    /* Free every node so we don't leak. */
    list_free(&head);
    printf("after free: ");
    list_print(head);   /* [] */

    return 0;
}
