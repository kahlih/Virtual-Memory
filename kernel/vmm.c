#include "vmm.h"
#include "console.h"
#include "stdint.h"
#include "machine.h"

#define MISSING() do { \
    putStr(__FILE__); \
    putStr(":"); \
    putDec(__LINE__); \
    putStr(" is missing\n"); \
    shutdown(); \
} while (0)

/* Each frame is 4K */
#define FRAME_SIZE (1 << 12)

/* A table contains 4K/4 = 1K page table entries */
#define TABLE_ENTRIES (FRAME_SIZE / sizeof(uint32_t))

/* A table, either a PD, or a PT */
typedef struct {
    uint32_t entries[TABLE_ENTRIES];
} table_s;


/* Address of first avaialble frame */
uint32_t avail = 0x100000;

/* pointer to page directory */
table_s *pd = 0;

/* zero-fill a frame */
static void zeroFrame(uint32_t ptr) {
    char* p = (char*) ptr;
    for (int i=0; i<FRAME_SIZE; i++) {
        *p = 0;
    }
}

/* The world's simplest frame allocator */
uint32_t vmm_frame(void) {
    uint32_t p = avail;
    avail += FRAME_SIZE;
    zeroFrame(p);
    return p;
}


/* handle a page fault */
void pageFault(uint32_t addr) {
   sayHex("page fault @ ", addr);

	vmm_map(addr,(uint32_t)(vmm_frame()));
}

/* Return a pointer to the PD, allocate it if you have to */
table_s* getPD() {
    if (pd == 0) {
        pd = (table_s*) vmm_frame();
    }
    return pd;            
}


/* Return a pointer to the PT that maps the given va,
   allocate it if you have to */
table_s* getPT(uint32_t va) {
	
    	table_s* pd = getPD();
	uint32_t f = (va >> 22); //get highest 10 bits
	table_s* pt; //pionter we're returning
	uint32_t pde = pd->entries[f];
	if ( pde & 1 ) //if P-bit is valid (1)
	{
		pt = (table_s*) (pde);
	}
	else //if P-bit is not valid (0)
	{
		pt = (table_s*) vmm_frame();
		uint32_t last = (pde & 0x00000fff) + 1;
		uint32_t first = (((uint32_t)pt) & 0xfffff000);
		pd->entries[f] = first + last;
		pt = (table_s*)pd->entries[f];
	}
	return (table_s*)(((uint32_t)pt) & 0xFFFFF000);
}

/* Create a new mapping from va to pa */
void vmm_map(uint32_t va, uint32_t pa) {
  
	uint32_t s = (va >> 12) & 0x000003FF; // second 10 bits
 	
	table_s* PT = getPT(va);
	PT->entries[s] = (pa & 0xFFFFF000) + 1;

	//MISSING();*/
}

/* check if the page containing the given PA is dirty */
int vmm_dirty(uint32_t va) {
  
	table_s* PT = getPT(va);
	uint32_t s = (va >> 12) & 0x000003FF;

	uint32_t pte = PT->entries[s];
	
	if (pte & 0x00000040)
	{
		return 1;
	}
	return 0;
	
	//MISSING();
    	//return 0;
}

/* check if the page containing the given PA has been accessed */
int vmm_accessed(uint32_t va) {
    
    	uint32_t s = (va >> 12) & 0x000003FF;

        table_s* PT = getPT(va);
        uint32_t pte = PT->entries[s];
        if (pte & 0x00000020)
	{
		return 1;
	}
	return 0;
}

/* return the PA that corresponds to the given VA, 0xffffffff is not mapped */
uint32_t vmm_pa(uint32_t va) {
	
	uint32_t f = va >> 22;	
	uint32_t s = (va >> 12) & 0x000003FF;
	//uint32_t o = va & 0x00000FFF;
 
	table_s* PD = getPD();
	uint32_t pde = PD->entries[f];
	
	if (pde & 1)
	{
		table_s* PT = getPT(va);
		uint32_t pte = PT->entries[s];
		
		if (pte & 1)
		{
			uint32_t pa = pte & 0xFFFFF000;
			return pa + (uint32_t)((va)& 0x00000FFF);
		}
	}	
	return 0xFFFFFFFF;	
	    

//	MISSING();
//    	return -1;
}

extern void invlpg(uint32_t);
/* unmap the given va */
void vmm_unmap(uint32_t va) {
	
	table_s* PT = getPT(va);
	uint32_t s = (va >> 12) & 0x000003FF;
	uint32_t pte = PT->entries[s];

	PT->entries[s] = pte & 0xFFFFFFFE;
	invlpg(va);
	

		//MISSING();
}

/* print the contents of the page table */
void vmm_dump() {
    table_s *pd = getPD();
    sayHex("PD @ ",(uint32_t) pd);
    for (int i=0; i<TABLE_ENTRIES; i++) {
        uint32_t e = pd->entries[i];
        if (e != 0) {
            putStr("    ");
            putHex(i);
            sayHex(") PDE = ",e);
            table_s * pt = (table_s*) (e & 0xfffff000);
            for (int j=0; j<TABLE_ENTRIES; j++) {
                uint32_t e = pt->entries[j];
                if (e != 0) {
                    putStr("        ");
                    putHex(j);
                    sayHex(") PTE = ",e);
                }
            }
        }
    }
}

