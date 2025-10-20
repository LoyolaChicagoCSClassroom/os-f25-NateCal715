// PAGE FRAME ALLOCATOR

#include "page.h"
#include <stdio.h>

// All the functions that are written for the page allocator are going to call
// the linked list functions to actually link and unlink elements from the list

struct ppage physical_page_array[128]; // 128 pages, each 2mb in length covers 256 megs of memory

void init_pfa_list(void) {
    // will initialze the linked list of free pages
    // It will loop through every element of your 
    // statically allocated physical_page_array and
    // link it into the list
    
}

struct ppage *allocate_physical_pages(unsigned int npages) {
    // allocates one or more physical pages
    // from the list of free pages. The input 
    // to this function npages specifies the 
    // number of pages to allocate. The function
    // will unlink the requested number of pages
    // from the free list and create a new separate
    // list. It will return a pointer to the new list
    // (called allocd_list)
    if (npages == 0) {
        return NULL; // No pages requested
    }

    struct ppage *allocd_list = NULL;



    return allocd_list;
}

void free_physical_pages(struct ppage *ppage_list) {
    // returns a list of physical pages to the 
    // free list. Basically does the opposite of 
    // allocate_physical_pages
    if (ppage_list == NULL) {
        return; // Nothing to free
    }
    
}