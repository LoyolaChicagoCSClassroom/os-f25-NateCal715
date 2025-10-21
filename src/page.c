// PAGE FRAME ALLOCATOR

// In our operating system, we are going to divide physical memory up into 2 mbyte blocks,
// each of which can be assigned to a particular process. We will track each physical page in 
// memory using a data structure that points to its physical addresss. Those data structures
// will initially be linked together in a linked list called free_physical_pages which will track
// the physical pages that are not allocated to any process.

#include "page.h"
#include <stdio.h>

#include <stddef.h>

// All the functions that are written for the page allocator are going to call
// the linked list functions to actually link and unlink elements from the list

// Statically Allocate an array of struct ppages
struct ppage physical_page_array[128]; // 128 pages, each 2mb in length covers 256 megs of memory

// Head of the free list
static struct ppage *free_list_head = NULL;

// Initializes the list of free physical page structures
void init_pfa_list(void) {
    // will initialze the linked list of free pages
    // It will loop through every element of your 
    // statically allocated physical_page_array and
    // link it into the list
    
    free_list_head = NULL;

    for (int i = 0; i < 128; i++) {
        physical_page_array[i].physical_addr = (void *)(i * 0x200000); // Each page is 2MB
        physical_page_array[i].next = NULL;
        physical_page_array[i].prev = NULL;
    
        // Add to free list
        list_add(&physical_page_array[i], &free_list_head);
    }
    
    
}

// Frees one or more physical pages back to the free list
void free_physical_pages(struct ppage *ppage_list) {
    // returns a list of physical pages to the 
    // free list. Basically does the opposite of 
    // allocate_physical_pages
    
    if (ppage_list == NULL) {
        return; // Nothing to free
    }

    // Iterate through the provided list and add each page back to the free list
    struct ppage *current_page = ppage_list;

    // Unlink each page from ppage_list and add to free_list_head
    while (current_page != NULL) {
        // Store next page before unlinking
        struct ppage *next_page = current_page->next;
        // Unlink from ppage_list
        list_remove(current_page, &ppage_list);
        // Add to free list
        list_add(current_page, &free_list_head);
        // Move to next page
        current_page = next_page;
    }

}

// Allocates one or more physical pages from the free list
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

    for (unsigned int i = 0; i < npages; i++) {
        if (free_list_head == NULL) {
            // Not enough pages available, free already allocated pages
            if (allocd_list != NULL) {
                // Return previously allocated pages back to free list
                free_physical_pages(allocd_list);
            }
            return NULL; // Allocation failed
        }  
        
        // Remove from free list and add to allocated list
        struct ppage *page_to_alloc = free_list_head;
        list_remove(page_to_alloc, &free_list_head);
        list_add(page_to_alloc, &allocd_list);
    }

    return allocd_list;
}
