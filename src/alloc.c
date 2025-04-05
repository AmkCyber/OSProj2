#include "alloc.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define ALIGNMENT 16 /**< The alignment of the memory blocks */

static free_block *HEAD = NULL; /**< Pointer to the first element of the free list */

/**
 * Split a free block into two blocks
 *
 * @param block The block to split
 * @param size The size of the first new split block
 * @return A pointer to the first block or NULL if the block cannot be split
 */
void *split(free_block *block, int size) {
    if((block->size < size + sizeof(free_block))) {
        return NULL;
    }

    void *split_pnt = (char *)block + size + sizeof(free_block);
    free_block *new_block = (free_block *) split_pnt;

    new_block->size = block->size - size - sizeof(free_block);
    new_block->next = block->next;

    block->size = size;

    return block;
}

/**
 * Find the previous neighbor of a block
 *
 * @param block The block to find the previous neighbor of
 * @return A pointer to the previous neighbor or NULL if there is none
 */
free_block *find_prev(free_block *block) {
    free_block *curr = HEAD;
    while(curr != NULL) {
        char *next = (char *)curr + curr->size + sizeof(free_block);
        if(next == (char *)block)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

/**
 * Find the next neighbor of a block
 *
 * @param block The block to find the next neighbor of
 * @return A pointer to the next neighbor or NULL if there is none
 */
free_block *find_next(free_block *block) {
    char *block_end = (char*)block + block->size + sizeof(free_block);
    free_block *curr = HEAD;

    while(curr != NULL) {
        if((char *)curr == block_end)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

/**
 * Remove a block from the free list
 *
 * @param block The block to remove
 */
void remove_free_block(free_block *block) {
    free_block *curr = HEAD;
    if(curr == block) {
        HEAD = block->next;
        return;
    }
    while(curr != NULL) {
        if(curr->next == block) {
            curr->next = block->next;
            return;
        }
        curr = curr->next;
    }
}

/**
 * Coalesce neighboring free blocks
 *
 * @param block The block to coalesce
 * @return A pointer to the first block of the coalesced blocks
 */
void *coalesce(free_block *block) {
    if (block == NULL) {
        return NULL;
    }

    free_block *prev = find_prev(block);
    free_block *next = find_next(block);

    // Coalesce with previous block if it is contiguous.
    if (prev != NULL) {
        char *end_of_prev = (char *)prev + prev->size + sizeof(free_block);
        if (end_of_prev == (char *)block) {
            prev->size += block->size + sizeof(free_block);

            // Ensure prev->next is updated to skip over 'block', only if 'block' is directly next to 'prev'.
            if (prev->next == block) {
                prev->next = block->next;
            }
            block = prev; // Update block to point to the new coalesced block.
        }
    }

    // Coalesce with next block if it is contiguous.
    if (next != NULL) {
        char *end_of_block = (char *)block + block->size + sizeof(free_block);
        if (end_of_block == (char *)next) {
            block->size += next->size + sizeof(free_block);

            // Ensure block->next is updated to skip over 'next'.
            block->next = next->next;
        }
    }

    return block;
}

/**
 * Call sbrk to get memory from the OS
 *   Note: sbrk is a system call that adjusts where the end of the heap is
 * @param size The amount of memory to allocate
 * @return A pointer to the allocated memory
 */
void *do_alloc(size_t size) {
    // gonna have to do alignment void* p =sbrk(0); 
    // use & to see what last digit is, if zero make no adjustments, if not zero (!0) Alignment - align, to keep the memory contiguous and not spread across multiple pages
//Alignment
    void *p = sbrk(0);// top of the heap
    intptr_t align = (intptr_t)p&(ALIGNMENT-1); //determines where out of the 16 we are misaligned. Treat pointer p as integer, subtract from 16, since its 0-15 (fetches last digit of memory address) & is a bitwise mask that gets the last digit
    intptr_t adjustment;
    if (align == 0) {
        adjustment = 0;
    }
    else {
        adjustment = ALIGNMENT-align; //hexadecimal subtraction, finding how far needed to move to get to next page
    }

//Processing
    void* mem_block = sbrk(size+sizeof(header)+adjustment);  //Creation of a block of memory considering the following: The sizeof(header) is kind of like padding for the OS to do stuff, so its size user requested+OS space+ whatever else space needed to reach the end/start of the next thing
    if (mem_block == (void *) -1) { //Error checking, Note: memory_block does not have the * because it's actual value has to be compared; the * is used to modify the values stored at that address. The general pointer having a value of -1 is what sbrk returns if it fails
        return NULL;
    }

//Free list management for our block
    void* header_start = (void*)((intptr_t)mem_block+adjustment); //adjustment is an intptr and mem_block is a void, so convert it to intptr first to add them together however header_start was expecting to void so typecast the sum to void
    //chunk of data, blank padding of adjustment, header, user data
    header* head = (header*)(header_start);//creation of the header/ start of header
    head->size = size;
    head->magic = 0x1602848;
    
    return header_start+sizeof(header); // @return A pointer to the allocated memory/user data, so skip over the header
}

/**
 * Allocates memory for the end user
 *
 * @param size The amount of memory to allocate
 * @return A pointer to the requested block of memory
 */
void *tumalloc(size_t size) { 
//The call in main.c is: int *thing = tumalloc(5*sizeof(int));

//If there isn't any memory space what do I do? Behave like realloc or print an error?

// !Look for available space in free list (IN NEXT FIT GOTTA GET THAT BONUS PNTS)

// !Memory

    if (HEAD == NULL) {
        void* ptr = do_alloc(size);
        return ptr;
    }else{   
        free_block* curr_node = HEAD; // this will change a bit for next fit
        
        while (curr_node != NULL) {
            if (size + sizeof(header) <= curr_node->size) {// Is there enough space in this free block
                curr_node = curr_node->next;
                curr_node=split(curr_node, size + sizeof(header)); //is this right?
                remove_free_block(curr_node);

                //Create a new header for this block of memory; line 196 not specified by pseudocode but I dont understand how it'd work without it
                header* block_header = (header*)curr_node;
                block_header->size = size;
                block_header->magic = 0x1602848;

                return (void*)((char*)curr_node + sizeof(header));
            }
        }
        
        if (size + sizeof(header) > curr_node->size){// check with schrick if this is right, I forgot to add this line when I was doing valguard
            // No suitable free block found, allocate from OS
            void* ptr = do_alloc(size);
            return ptr;
        }

    }
}

/**
 * Allocates and initializes a list of elements for the end user
 *
 * @param num How many elements to allocate
 * @param size The size of each element
 * @return A pointer to the requested block of initialized memory
 */
void *tucalloc(size_t num, size_t size) {
    size_t total_size = num * size;  // Total memory size
    void *ptr = tumalloc(total_size);  // Call malloc 

    if (ptr != NULL) {
        memset(ptr, 0, total_size);  // Initialize all bytes to zero
    }
    return ptr;
}

/**
 * Reallocates a chunk of memory with a bigger size
 *
 * @param ptr A pointer to an already allocated piece of memory
 * @param new_size The new requested size to allocate
 * @return A new pointer containing the contents of ptr, but with the new_size
 */
void *turealloc(void *ptr, size_t new_size) {
//create new block with desired size, transfer data, free memory from old block, return new block pointer
    
    if (ptr == NULL) { //Error catching incase ptr isnt given, basically turns into malloc
        return tumalloc(new_size);
    }

    if (new_size == 0) { //Another error catch, frees data if size is null
        tufree(ptr);
        return NULL;
    }

    //Get old header to get old data and check magic number

    //void* new_ptr = tumalloc(new_size);  // Call malloc


    return NULL;
}

/**
 * Removes used chunk of memory and returns it to the free list
 *
 * @param ptr Pointer to the allocated piece of memory
 */
void tufree(void *ptr) {
    if (ptr == NULL){
        return;
    }

    //prt was at end of header, move it back to start of header
    header* free_header = (header*)((char*)ptr-sizeof(header)); //similar to line 200
    
    if (free_header->magic == 0x1602848){
        free_block* free_blk = (free_block*)free_header;
        free_blk->size = free_header->size;
        free_blk->next = HEAD;
        HEAD = free_blk;
        coalesce(free_blk);
    
    } else {
        fprintf(stderr, "MEMORY CORRUPTION DETECTED\n");//had chat give me the proper syntax for writing this error message
        abort();
    }
}