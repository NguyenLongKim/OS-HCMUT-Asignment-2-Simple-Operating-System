
#include "mem.h"
#include "stdlib.h"
#include "string.h"
#include <pthread.h>
#include <stdio.h>

static BYTE _ram[RAM_SIZE];

static struct {
	uint32_t proc;	// ID of process currently uses this page
	int index;	// Index of the page in the list of pages allocated to the process.
	int next;	// The next page in the list. -1 if it is the last page.
} _mem_stat [NUM_PAGES];

static pthread_mutex_t mem_lock;
static pthread_mutex_t ram_lock;

void init_mem(void) {
	memset(_mem_stat, 0, sizeof(*_mem_stat) * NUM_PAGES);
	memset(_ram, 0, sizeof(BYTE) * RAM_SIZE);
	pthread_mutex_init(&mem_lock, NULL);
	pthread_mutex_init(&ram_lock, NULL);
}

/* get offset of the virtual address */
static addr_t get_offset(addr_t addr) {
	return addr & ~((~0U) << OFFSET_LEN);
}

/* get the first layer index */
static addr_t get_first_lv(addr_t addr) {
	return addr >> (OFFSET_LEN + PAGE_LEN);
}

/* get the second layer index */
static addr_t get_second_lv(addr_t addr) {
	return (addr >> OFFSET_LEN) - (get_first_lv(addr) << PAGE_LEN);
}

/* Search for page table table from the a segment table */
static struct page_table_t * get_page_table(
		addr_t index, 	// Segment level index
		struct seg_table_t * seg_table) { // first level table
	
	/*
	 * TODO: Given the Segment index [index], you must go through each
	 * row of the segment table [seg_table] and check if the v_index
	 * field of the row is equal to the index
	 *
	 * */

	int i;
	for (i = 0; i < seg_table->size; i++) {
		// Enter your code here
		if (seg_table->table[i].v_index == index){
			return seg_table->table[i].pages;
		}
	}
	return NULL;

}

/* Translate virtual address to physical address. If [virtual_addr] is valid,
 * return 1 and write its physical counterpart to [physical_addr].
 * Otherwise, return 0 */
static int translate(
		addr_t virtual_addr, 	// Given virtual address
		addr_t * physical_addr, // Physical address to be returned
		struct pcb_t * proc) {  // Process uses given virtual address

	/* Offset of the virtual address */
	addr_t offset = get_offset(virtual_addr);
	/* The first layer index */
	addr_t first_lv = get_first_lv(virtual_addr);
	/* The second layer index */
	addr_t second_lv = get_second_lv(virtual_addr);
	
	/* Search in the first level */
	struct page_table_t * page_table = NULL;
	page_table = get_page_table(first_lv, proc->seg_table);
	if (page_table == NULL) {
		return 0;
	}

	int i;
	for (i = 0; i < page_table->size; i++) {
		if (page_table->table[i].v_index == second_lv) {
			/* TODO: Concatenate the offset of the virtual addess
			 * to [p_index] field of page_table->table[i] to 
			 * produce the correct physical address and save it to
			 * [*physical_addr]  */
			*physical_addr = (page_table->table[i].p_index << OFFSET_LEN) | (offset);
			return 1;
		}
	}
	return 0;	
}

addr_t alloc_mem(uint32_t size, struct pcb_t * proc) {
	pthread_mutex_lock(&mem_lock);
	addr_t ret_mem = 0;
	/* TODO: Allocate [size] byte in the memory for the
	 * process [proc] and save the address of the first
	 * byte in the allocated memory region to [ret_mem].
	 * */

	uint32_t num_pages = (size % PAGE_SIZE == 0) ? size / PAGE_SIZE : size / PAGE_SIZE + 1; // Number of pages we will use
	int mem_avail = 0; // We could allocate new memory region or not?

	/* First we must check if the amount of free memory in
	 * virtual address space and physical address space is
	 * large enough to represent the amount of required 
	 * memory. If so, set 1 to [mem_avail].
	 * Hint: check [proc] bit in each page of _mem_stat
	 * to know whether this page has been used by a process.
	 * For virtual memory space, check bp (break pointer).
	 * */
	int NumOfFreePages = 0;
	for (int i=0; i<NUM_PAGES; i++){
		if (_mem_stat[i].proc==0){
			NumOfFreePages++;
		}
	}

	addr_t new_bp = proc->bp + num_pages*PAGE_SIZE;

	if (num_pages <= NumOfFreePages && new_bp<=RAM_SIZE){
		mem_avail = 1;
	}

	if (mem_avail) {
		/* We could allocate new memory region to the process */
		ret_mem = proc->bp;
		proc->bp += num_pages * PAGE_SIZE;
		/* Update status of physical pages which will be allocated
		 * to [proc] in _mem_stat. Tasks to do:
		 * 	- Update [proc], [index], and [next] field
		 * 	- Add entries to segment table page tables of [proc]
		 * 	  to ensure accesses to allocated memory slot is
		 * 	  valid. */
		int numOfAllocatedPages = 0; // to count allocated pages
		int lastAllocatedPageIndex = -1; // to save last allocated page index
		for (int i = 0; i < NUM_PAGES; i++) {
			if (_mem_stat[i].proc) continue; // this physical page is used 
			
			/************************************************
			 * TASK: Update [proc], [index], and [next] field
			 * *********************************************/
			//Update status of physical pages which will be allocated to [proc] in _mem_stat
			_mem_stat[i].proc = proc->pid; // update [proc] field
			_mem_stat[i].index = numOfAllocatedPages; // update [index] field
			if (lastAllocatedPageIndex > -1) { 
				_mem_stat[lastAllocatedPageIndex].next = i; // update [next] field of previous allocated page
			}
			lastAllocatedPageIndex = i;// change last allocated page index

			/**********************************************************
			 * TASK: Add entries to segment table page tables of [proc]
			 * *******************************************************/
			addr_t v_address = ret_mem + numOfAllocatedPages * PAGE_SIZE; // virtual address of this allocated page
			// get page table given [v_address]
			struct page_table_t * v_page_table = get_page_table(get_first_lv(v_address), proc->seg_table); 
			/* If page table not already exist then create a new one */
			if (!v_page_table) { 
				// set first_lv [v_index] of new page table
				proc->seg_table->table[proc->seg_table->size].v_index = get_first_lv(v_address); 
				// allocate memory for new page table
				v_page_table = proc->seg_table->table[proc->seg_table->size].pages = malloc(sizeof(struct page_table_t)); 
				v_page_table->size = 0; // initial size of new page table
				proc->seg_table->size++; // increase size of [seg_table]
			}
			//Map virtual address to physical address
			v_page_table->table[v_page_table->size].v_index = get_second_lv(v_address); 
			v_page_table->table[v_page_table->size].p_index = i; 
			v_page_table->size++;
			
			// increase num of allocated pages, 
			// if it equal to num of pages has to allocate then we exit for loop
			if (++numOfAllocatedPages == num_pages) {
				_mem_stat[i].next = -1; // set [next] field of last allocated page
				break;
			}
		}
	}
	pthread_mutex_unlock(&mem_lock);
	return ret_mem;
}



int free_mem(addr_t address, struct pcb_t * proc) {
	/*TODO: Release memory region allocated by [proc]. The first byte of
	 * this region is indicated by [address]. Task to do:
	 * 	- Set flag [proc] of physical page use by the memory block
	 * 	  back to zero to indicate that it is free.
	 * 	- Remove unused entries in segment table and page tables of
	 * 	  the process [proc].
	 * 	- Remember to use lock to protect the memory from other
	 * 	  processes.  */
	pthread_mutex_lock(&mem_lock);

    addr_t p_address;
    if (translate(address, &p_address, proc))
    {
        uint32_t numPages = 0;
        addr_t v_address = address;
        addr_t p_page = p_address >> OFFSET_LEN; //Physical page

        //Set flag [proc] of physical pages
        while (p_page != -1)
        {
            _mem_stat[p_page].proc = 0;
            p_page = _mem_stat[p_page].next;
            ++numPages;
        }
        
        uint32_t remainingPages = numPages; // to count remaining pages has to remove
        //While loop to remove unused entries
        while (remainingPages > 0)
        {
            addr_t v_segment = get_first_lv(v_address);
            uint32_t removedPages = 0;
            //Get current page table of the process given virtual segment index
            struct page_table_t *currentPageTable = get_page_table(v_segment, proc->seg_table);
            //If the page table is found, delete unused entries
            if (currentPageTable)
            {
                uint32_t startSize = currentPageTable->size;
                uint32_t j = 0;
                for (uint32_t i = 0; i < startSize; ++i)
                {
                    //Calculate virtual segment_page index at entry "j" in page table.
                    uint32_t v_seg_page = (v_segment << PAGE_LEN) + currentPageTable->table[j].v_index;
                    //Remove unused entry
                    if(v_seg_page >= (address >> OFFSET_LEN)
						&& v_seg_page <= (address >> OFFSET_LEN) + numPages - 1)
                    {
                        //Replace the current entry by the last entry in this page table
                        currentPageTable->table[j] = currentPageTable->table[--(currentPageTable->size)];
                        removedPages++;
                    }
                    else{
                        //If this entry is not the one we has to remove, move to next entry.
                        j++;
                    }
                }
                //Free the page table that has size = 0
                if (currentPageTable->size == 0)
                {
                    free(currentPageTable);
                    //Decrease the size of segment table
                    proc->seg_table->size--;
                }
            }
            v_address += ((removedPages) ? removedPages : 1) * PAGE_SIZE;
            remainingPages -= removedPages ? removedPages : 1;
        }

        // Update break point   
        while(get_page_table(get_first_lv(proc->bp), proc->seg_table)){
            proc->bp -= PAGE_SIZE;
        }
        proc->bp += PAGE_SIZE;
    }
   
    pthread_mutex_unlock(&mem_lock);
    return 0;
}

int read_mem(addr_t address, struct pcb_t * proc, BYTE * data) {
	addr_t physical_addr;
	if (translate(address, &physical_addr, proc)) {
		pthread_mutex_lock(&ram_lock);
		*data = _ram[physical_addr];
		pthread_mutex_unlock(&ram_lock);
		return 0;
	}else{
		return 1;
	}
}

int write_mem(addr_t address, struct pcb_t * proc, BYTE data) {
	addr_t physical_addr;
	if (translate(address, &physical_addr, proc)) {
		pthread_mutex_lock(&ram_lock);
		_ram[physical_addr] = data;
		pthread_mutex_unlock(&ram_lock);
		return 0;
	}else{
		return 1;
	}
}

void dump(void) {
	int i;
	for (i = 0; i < NUM_PAGES; i++) {
		if (_mem_stat[i].proc != 0) {
			printf("%03d: ", i);
			printf("%05x-%05x - PID: %02d (idx %03d, nxt: %03d)\n",
				i << OFFSET_LEN,
				((i + 1) << OFFSET_LEN) - 1,
				_mem_stat[i].proc,
				_mem_stat[i].index,
				_mem_stat[i].next
			);
			int j;
			for (	j = i << OFFSET_LEN;
				j < ((i+1) << OFFSET_LEN) - 1;
				j++) {
				
				if (_ram[j] != 0) {
					printf("\t%05x: %02x\n", j, _ram[j]);
				}
					
			}
		}
	}
}


