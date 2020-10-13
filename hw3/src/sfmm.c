/**
 * All functions you make for the assignment must be implemented in this file.
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "sfmm.h"
#include <errno.h>

sf_block* pro;
sf_block* epi;

void init_sf_free_list_heads()
{
    for(int i = 0;i<NUM_FREE_LISTS;i++)
    {
        sf_free_list_heads[i].header = 0;
        sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
        sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
    }
}

void insert_free_list(sf_block* head, sf_block* free_list)
{
    if (head ->body.links.next == head && head ->body.links.prev == head)
    {
        head ->body.links.next = free_list;
        head->body.links.prev = free_list;
        free_list ->body.links.prev = head;
        free_list ->body.links.next = head;
    }
    else
    {
        // free_list ->body.links.next = head;
        // free_list ->body.links.prev = head ->body.links.prev;
        // head-> body.links.prev ->body.links.next = free_list;
        // head -> body.links.prev = free_list;
        free_list->body.links.prev = head;
        free_list->body.links.next = head->body.links.next;
        head->body.links.next ->body.links.prev = free_list;
        head->body.links.next = free_list;
    }
}

int position_in_heads(size_t s){
    int m = 64;
    if (s == m) return 0;
    else if(s == 2*m) return 1;
    else if(s == 3*m) return 2;
    else if(s > 3*m && s <= 5*m) return 3;
    else if(s > 5*m && s <= 8*m) return 4;
    else if(s > 8*m && s <= 13*m) return 5;
    else if(s > 13*m && s <= 21*m) return 6;
    else if(s > 21*m && s <= 34*m) return 7;
    else if(s > 34*m) return 8;
    else return -1;
}

void disconnect_free_block(sf_block* target)
{
    target->body.links.prev->body.links.next = target->body.links.next;
    target->body.links.next->body.links.prev = target->body.links.prev;
}

sf_block* find_in_list(sf_block* head, size_t s)
{
    sf_block* target = head->body.links.next;
    while (target != head)
    {
        if ((target->header&BLOCK_SIZE_MASK) >= s) {
            disconnect_free_block(target);
            return target;
        }
        target = target ->body.links.next;
    }
    return NULL;
}

sf_block* merge_free_block(sf_block* free_block)
{
    int isPrevAlloc = free_block -> header & PREV_BLOCK_ALLOCATED;
    size_t bs = free_block->header & BLOCK_SIZE_MASK;
    sf_block* next = (sf_block*) (((void*)free_block) + bs);
    int isNextAlloc = next->header & THIS_BLOCK_ALLOCATED;
    if (isNextAlloc==0)
    {
        disconnect_free_block(next);
        size_t next_size = next->header & BLOCK_SIZE_MASK;
        free_block->header += next_size;
        bs = free_block->header & BLOCK_SIZE_MASK;
        sf_block* new_next = (sf_block*) (((void*)next)+next_size);
        new_next->prev_footer = free_block->header;
    }
    if(isPrevAlloc==0)
    {
        disconnect_free_block(free_block);
        size_t prev_size = free_block->prev_footer & BLOCK_SIZE_MASK;
        sf_block* prev = (sf_block*) (((void*)free_block) - prev_size);
        prev -> header += bs;
        sf_block* new_next = (sf_block*) (((void*)free_block) + bs);
        new_next -> prev_footer = prev -> header;
        free_block = prev;
    }
    return free_block;
}

sf_block* grow_wilder(int n)
{
    // if (epi->header & PREV_BLOCK_ALLOCATED) return epi;
    sf_block* new_block = epi;
    new_block -> header = PAGE_SZ * n + (new_block->header & PREV_BLOCK_ALLOCATED);
    if (n==0) return new_block;
    epi = (sf_block*) (((void*)new_block) + PAGE_SZ * n);
    epi -> header = THIS_BLOCK_ALLOCATED;
    epi -> prev_footer = new_block -> header;
    return merge_free_block(new_block);
}

// s is raw size + 8
sf_block* alloc_free_block(sf_block* free_block, size_t s)
{
    size_t bs = free_block->header & BLOCK_SIZE_MASK;
    size_t leftover = bs - s;
    if (leftover==0)
    {
        free_block->header = (free_block->header | THIS_BLOCK_ALLOCATED);
        sf_block* next = (sf_block*) (((void*)free_block)+s);
        next -> header = (next -> header | PREV_BLOCK_ALLOCATED);
        return free_block;
    }
    else
    {
        free_block->header = s + (free_block->header&PREV_BLOCK_ALLOCATED)
            + THIS_BLOCK_ALLOCATED;
        sf_block* splited_block = (sf_block*) (((void*)free_block)+s);
        splited_block -> header = leftover + PREV_BLOCK_ALLOCATED;
        splited_block = merge_free_block(splited_block);
        leftover = (splited_block->header & BLOCK_SIZE_MASK);
        sf_block* next = (sf_block*) (((void*)splited_block)+leftover);
        next -> prev_footer = splited_block->header;
        if (next!=epi)
        {
            int sb_pos = position_in_heads(leftover);
            insert_free_list(&sf_free_list_heads[sb_pos], splited_block);
        }
        else
        {
            sf_free_list_heads[9].body.links.prev = &sf_free_list_heads[9];
            sf_free_list_heads[9].body.links.next = &sf_free_list_heads[9];
            insert_free_list(&sf_free_list_heads[9], splited_block);
        }
        return free_block;
    }
    
    return NULL;
}

void *sf_malloc(size_t size) {
    // If allocing for 0 bytes
    if (size <= 0)
    {
        return NULL;
    }
    size += 8;
    if (size % 64 != 0)
    {
        size = size - size%64 + 64;
    }
    // If the heap is not initialized 
    if (sf_mem_end() == sf_mem_start())
    {
        init_sf_free_list_heads();
        sf_mem_grow();
        pro = (sf_block*) (sf_mem_start() + 6*8);
        pro -> header = 64 + PREV_BLOCK_ALLOCATED + THIS_BLOCK_ALLOCATED;
        sf_block* first_block = (sf_block*) (sf_mem_start() + 6*8+ 8*8);
        first_block->prev_footer = pro->header;
        size_t proepi_size = 7*8 + 8*8 + 8;
        size_t free_space = PAGE_SZ - proepi_size;
        first_block -> header = free_space + PREV_BLOCK_ALLOCATED;
        int additional_pages = 0;
        if (size > free_space)
        {
            additional_pages = (int) ((size - free_space)/PAGE_SZ);
            if ((size-free_space) % PAGE_SZ != 0)
                additional_pages++;
            size_t i = 0;
            while (i < additional_pages)
            {
                if (sf_mem_grow() == NULL)
                {
                    first_block -> header += PAGE_SZ * i;
                    epi = (sf_block*) (((void*)first_block) + 16 + free_space + PAGE_SZ*i);
                    epi -> prev_footer = first_block -> header;
                    epi -> header = THIS_BLOCK_ALLOCATED;
                    insert_free_list(&sf_free_list_heads[9], first_block);
                    sf_errno = ENOMEM;
                    return NULL;
                }
                i++;
            }
            additional_pages = i;
        }
        size_t leftover = additional_pages*PAGE_SZ + free_space - size;
        first_block->header = size + THIS_BLOCK_ALLOCATED + PREV_BLOCK_ALLOCATED;
        // If no need to split
        if (leftover == 0)
        {
            epi = (sf_block*) (((void*)first_block) + size);
            epi ->prev_footer = first_block->header;
            epi -> header = THIS_BLOCK_ALLOCATED;
            return ((void *) first_block) + 16;
        }
        // split the block
        sf_block* splited_block = (sf_block*) (((void*)first_block)+size);
        //splited_block -> prev_footer = first_block -> header;
        splited_block -> header = leftover + PREV_BLOCK_ALLOCATED;
        epi = (sf_block*) (sf_mem_end()-16);
        epi ->prev_footer = splited_block->header;
        epi -> header = THIS_BLOCK_ALLOCATED;
        // put the splited block in the free list
        // int pos = position_in_heads(leftover);
        insert_free_list(&sf_free_list_heads[9], splited_block);
        return ((void *) first_block) + 16;
    }
    // if the heap already exist
    sf_block* free_block = NULL;
    for (int bpos = position_in_heads(size);bpos < NUM_FREE_LISTS;bpos++)
    {
        free_block = find_in_list(&sf_free_list_heads[bpos],size);
        if (free_block != NULL) break;
    }
    // if no match
    if (free_block == NULL)
    {
        size_t free_block_size = 0;
        if ((epi->prev_footer & THIS_BLOCK_ALLOCATED) == 0)
        {
            free_block_size = epi->prev_footer & BLOCK_SIZE_MASK;
        }
        size_t size_needed = size - free_block_size;
        int addi_pages = (int) (size_needed / PAGE_SZ);
        if ((size_needed % PAGE_SZ) != 0) addi_pages++;
        int k = 0;
        while (k<addi_pages)
        {
            if(sf_mem_grow() == NULL)
            {
                free_block = grow_wilder(k);
                sf_errno = ENOMEM;
                // if (free_block==epi) return NULL;
                sf_free_list_heads[9].body.links.prev = &sf_free_list_heads[9];
                sf_free_list_heads[9].body.links.next = &sf_free_list_heads[9];
                insert_free_list(&sf_free_list_heads[9], free_block);
                return NULL;
            }
            k++;
        }
        free_block = grow_wilder(k);
        sf_free_list_heads[9].body.links.prev = &sf_free_list_heads[9];
        sf_free_list_heads[9].body.links.next = &sf_free_list_heads[9];
        insert_free_list(&sf_free_list_heads[9], free_block);
    }
    free_block = alloc_free_block(free_block, size);
    return ((void *)free_block)+16;
}

int is_alloced_block(sf_block* block)
{
    if(block==NULL) return 0;
    // if this block is not alloced
    if ((block->header&THIS_BLOCK_ALLOCATED) == 0) return 0;
    // if the size is too small
    if ((block->header&BLOCK_SIZE_MASK)<64) return 0;
    size_t block_size = block->header & BLOCK_SIZE_MASK;
    if (block_size%64 != 0) return 0; // if not aligned
    // if out of heap boundary
    if ((((void*)block)< ((void*)pro + 64)) || ((void*)block + block_size > (void*)epi))
        return 0;
    // invalid header
    int prev_footer_alloc = (block->prev_footer & THIS_BLOCK_ALLOCATED);
    int this_header_alloc = (block->header & PREV_BLOCK_ALLOCATED);
    if(this_header_alloc== 0 && prev_footer_alloc == THIS_BLOCK_ALLOCATED)
        return 0;
    return 1;
}

void sf_free(void *pp) {
    if (pp == NULL) abort();
    sf_block* block_to_be_freed = (sf_block*) (pp-16);
    if (!is_alloced_block(block_to_be_freed)) abort();
    size_t block_size = block_to_be_freed->header & BLOCK_SIZE_MASK;
    block_to_be_freed -> header = (block_to_be_freed->header & ~THIS_BLOCK_ALLOCATED);
    int pos = position_in_heads(block_size);
    insert_free_list(&sf_free_list_heads[pos], block_to_be_freed);
    block_to_be_freed = merge_free_block(block_to_be_freed);
    size_t new_block_size = block_to_be_freed->header & BLOCK_SIZE_MASK;
    sf_block* next_block = (sf_block*)((void*)block_to_be_freed+new_block_size);
    next_block->header = (next_block->header & ~PREV_BLOCK_ALLOCATED);
    next_block->prev_footer = block_to_be_freed -> header;
    // if the block now is the wilderness block
    if(block_to_be_freed->header==epi->prev_footer)
    {
        disconnect_free_block(block_to_be_freed);
        sf_free_list_heads[9].body.links.prev = &sf_free_list_heads[9];
        sf_free_list_heads[9].body.links.next = &sf_free_list_heads[9];
        insert_free_list(&sf_free_list_heads[9], block_to_be_freed);
    }
    return;
}

void *sf_realloc(void *pp, size_t rsize) {
    if(pp==NULL)
    {
        sf_errno = EINVAL;
        return NULL;
    }
    sf_block* old_block = (sf_block*) (pp-16);
    if(!is_alloced_block(old_block))
    {
        sf_errno = EINVAL;
        return NULL;
    }
    if(rsize==0)
    {
        sf_free(pp);
        return NULL;
    }
    size_t old_size = old_block->header &BLOCK_SIZE_MASK;
    size_t raw_size = rsize;
    rsize += 8;
    if (rsize%64!=0)    rsize += 64 - (rsize%64);
    if(rsize < old_size)
    {
        old_block = alloc_free_block(old_block, rsize);
        return ((void*)old_block+16);
    }
    else if(rsize == old_size)
    {
        return pp;
    }
    else if(rsize > old_size)
    {
        void* new_pp = sf_malloc(raw_size);
        if (new_pp==NULL) return NULL; //sf_errno is set in malloc
        memcpy(new_pp, pp,(old_size-8));
        sf_free(pp);
        return new_pp;
    }
    return NULL;
}


int is_valid_align(size_t align)
{
    int i = 0;
    int counter = 0;
    while(i<64)
    {
        counter += (align&1);
        if(counter > 1) return 0;
        align >>= 1;
        i++;
    }
    return 1;
}

void *sf_memalign(size_t size, size_t align) {
    size_t mini = 64;
    if (align<mini)
    {
        sf_errno = EINVAL;
        return NULL;
    }
    if (!is_valid_align(align))
    {
        sf_errno = EINVAL;
        return NULL;
    }
    if (size == 0) return NULL;
    size_t total = size + align + mini;
    void* p = sf_malloc(total);
    if (p==NULL) return NULL;
    // printf("%p\n",p);
    // printf("remainder: %li\n", ((size_t)p%align));
    sf_block* large = (sf_block*) (p-16);
    size_t large_size = large->header & BLOCK_SIZE_MASK;
    sf_block* next = (sf_block*) ((void*)large + large_size);
    printf("next: %p", next);
    size_t actual_need = size + 8;
    if (actual_need%64!=0) actual_need += 64 - actual_need%64;
    if (((size_t)p%align) != 0)
    {
        // printf("%p\n",p);
        // printf("remainder: %li\n", ((size_t)p%align));
        void* new_p = p;
        while(((size_t)new_p % align)!=0)
        {
            new_p += mini;
        }
        //printf("new remainder: %li\n", ((size_t)new_p%align));
        size_t init_free_space = new_p - p;
        //printf("init free space: %li\n", init_free_space);
        sf_block* aligned_block = (sf_block*) (new_p -16);
        aligned_block->header = actual_need + THIS_BLOCK_ALLOCATED;
        large->header = init_free_space + (large->header & PREV_BLOCK_ALLOCATED);
        int pos = position_in_heads(init_free_space);
        insert_free_list(&sf_free_list_heads[pos], large);
        large = merge_free_block(large);
        pos = position_in_heads(large->header & BLOCK_SIZE_MASK);
        disconnect_free_block(large);
        insert_free_list(&sf_free_list_heads[pos], large);
        aligned_block->prev_footer = large -> header; 
        sf_block* aligned_next = (sf_block*) ((void*)aligned_block+actual_need);
        if(aligned_next==next)
        {
            return (void*) aligned_block + 16;
        }
        // yet another block to split
        size_t free_two_size= (void*)next - (void*)aligned_next;
        aligned_next->header = free_two_size + PREV_BLOCK_ALLOCATED;
        pos = position_in_heads(free_two_size);
        insert_free_list(&sf_free_list_heads[pos], aligned_next);
        aligned_next = merge_free_block(aligned_next);
        disconnect_free_block(aligned_next);
        // pos = position_in_heads(aligned_next->header & BLOCK_SIZE_MASK);
        // disconnect_free_block(aligned_next);
        // insert_free_list(&sf_free_list_heads[pos], aligned_next);
        sf_block* next_next = (sf_block*) ((void*)aligned_next 
            + (aligned_next->header & BLOCK_SIZE_MASK));
        next_next->prev_footer = aligned_next->header;
        //sf_show_heap();
        if (next_next==epi)
        {
            sf_free_list_heads[9].body.links.prev = &sf_free_list_heads[9];
            sf_free_list_heads[9].body.links.next = &sf_free_list_heads[9];
            insert_free_list(&sf_free_list_heads[9], aligned_next);
        }
        else
        {
            pos = position_in_heads(aligned_next->header & BLOCK_SIZE_MASK);
            disconnect_free_block(aligned_next);
            insert_free_list(&sf_free_list_heads[pos], aligned_next);
        }
        return (void*) aligned_block + 16;
    }
    //need to split the end
    sf_block* free_two = (sf_block*)((void*)large + actual_need);
    large->header = actual_need + (large->header &PREV_BLOCK_ALLOCATED)+THIS_BLOCK_ALLOCATED;
    free_two->header = large_size-actual_need+PREV_BLOCK_ALLOCATED;
    size_t ft_sz = large_size-actual_need;
    int ft_pos = position_in_heads(ft_sz);
    insert_free_list(&sf_free_list_heads[ft_pos], free_two);
    free_two = merge_free_block(free_two);
    ft_sz = free_two->header & BLOCK_SIZE_MASK;
    disconnect_free_block(free_two);
    sf_block* next_block = (sf_block*) ((void*)free_two+ft_sz);
    next_block->prev_footer = free_two->header;
    if(next_block==epi)
    {
        sf_free_list_heads[9].body.links.prev = &sf_free_list_heads[9];
        sf_free_list_heads[9].body.links.next = &sf_free_list_heads[9];
        insert_free_list(&sf_free_list_heads[9], free_two);
    }
    else
    {
        ft_pos= position_in_heads(ft_sz);
        insert_free_list(&sf_free_list_heads[ft_pos], free_two);
    }
    return (void*)large + 16;
}
