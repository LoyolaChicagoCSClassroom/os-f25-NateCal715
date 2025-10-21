#include <stdio.h>

struct ppage {
    struct ppage *next;
    struct ppage *prev;
    void *physical_addr;
};

list_add(struct ppage *new_page, struct ppage **head) {
    if (*head == NULL) {
        *head = new_page;
    } else {
        new_page->next = *head;
        (*head)->prev = new_page;
        *head = new_page;
    }
}

list_remove(struct ppage *page, struct ppage **head) {
    if (page->prev) {
        page->prev->next = page->next;
    } else {
        *head = page->next;
    }
    if (page->next) {
        page->next->prev = page->prev;
    }
    page->next = NULL;
    page->prev = NULL;
}
