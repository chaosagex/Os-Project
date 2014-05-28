
/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/mmu.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/elf.h>

#include <kern/user_environment.h>
#include <kern/memory_manager.h>
#include <kern/file_manager.h>
#include <kern/trap.h>
#include <kern/command_prompt.h>
#include <kern/helpers.h>
#include <kern/sched.h>
#include <inc/queue.h>

struct Env* envs = NULL;		// All environments
struct Env* curenv = NULL;	        // The current env
static struct Env_list env_free_list;	// Free Environment list

#define ENVGENSHIFT		12		// >= LOGNENV

//Contains information about each program segment (e.g. start address, size, virtual address...)
//It will be used below in "env_create" to load each program segment into the user environment

struct ProgramSegment {
	uint8 *ptr_start;
	uint32 size_in_file;
	uint32 size_in_memory;
	uint8 *virtual_address;

	// for use only with PROGRAM_SEGMENT_FOREACH	
	uint32 segment_id; 
};

// Used inside the PROGRAM_SEGMENT_FOREACH macro to get the first program segment 
// and then iterate on the next ones
struct ProgramSegment* PROGRAM_SEGMENT_NEXT(struct ProgramSegment* seg, uint8* ptr_program_start);
struct ProgramSegment PROGRAM_SEGMENT_FIRST( uint8* ptr_program_start);

// Used inside "env_create" function to get information about each program segment inside the user program 
#define	PROGRAM_SEGMENT_FOREACH(Seg, ptr_program_start)					\
		struct ProgramSegment* first; \
		struct ProgramSegment tmp; \
		tmp = (PROGRAM_SEGMENT_FIRST(ptr_program_start));	 \
		first = &tmp; \
		if(first->segment_id == -1) first = NULL;\
		Seg = first; \
		for (;				\
		Seg;							\
		Seg = PROGRAM_SEGMENT_NEXT(Seg,ptr_program_start) )

// Helper functions to be used below
void complete_environment_initialization(struct Env* e);
void set_environment_entry_point(struct Env* e, uint8* ptr_program_start);

///===================================================================================
/// To add FOS support for new user program, just add the appropriate lines like below

//The input for any DECLARE_START_OF macro must be the ".c" filename of the user program 
DECLARE_START_OF(fos_helloWorld)
DECLARE_START_OF(fos_add)
DECLARE_START_OF(fos_alloc)
DECLARE_START_OF(fos_input)
DECLARE_START_OF(game)
DECLARE_START_OF(fos_static_data_section)
DECLARE_START_OF(fos_data_on_stack)
DECLARE_START_OF(test_ptf_stack)
DECLARE_START_OF(quicksort_heap)

//2012
DECLARE_START_OF(fos_fibonacci)
DECLARE_START_OF(fos_factorial)

//DECLARE_START_OF(tst_malloc)
//DECLARE_START_OF(tst_sharing_1);
//DECLARE_START_OF(tst_sharing_2master);
//DECLARE_START_OF(tst_sharing_2slave);
//DECLARE_START_OF(tst_sharing_3);
DECLARE_START_OF(tst_sharing_4);
DECLARE_START_OF(tst_sharing_5_master);
DECLARE_START_OF(tst_sharing_5_slave);
DECLARE_START_OF(tst_sharing_5_slaveB1);
DECLARE_START_OF(tst_sharing_5_slaveB2);

//2014
DECLARE_START_OF(tst_free_1);
DECLARE_START_OF(tst_free_2);
DECLARE_START_OF(tst_malloc_1);
DECLARE_START_OF(tst_malloc_2);
DECLARE_START_OF(tst_first_fit_1);
DECLARE_START_OF(tst_first_fit_2);
DECLARE_START_OF(tst_sharing_1);
DECLARE_START_OF(tst_sharing_2master);
DECLARE_START_OF(tst_sharing_2slave1);
DECLARE_START_OF(tst_sharing_2slave2);
DECLARE_START_OF(tst_sharing_3);
DECLARE_START_OF(mergesort_leakage);
DECLARE_START_OF(mergesort_noleakage);
DECLARE_START_OF(quicksort_noleakage);

//2014 Final Tests
DECLARE_START_OF(tst1);
DECLARE_START_OF(tst2);
DECLARE_START_OF(tst3);
DECLARE_START_OF(tst4);
DECLARE_START_OF(tst5);
DECLARE_START_OF(tst6);
DECLARE_START_OF(tst_realloc_1);
DECLARE_START_OF(tst_realloc_2);

//User Programs Table
//The input for any PTR_START_OF macro must be the ".c" filename of the user program
struct UserProgramInfo userPrograms[] = {
		{ "fos_helloWorld", "Created by FOS team, fos@nowhere.com", PTR_START_OF(fos_helloWorld)},
		{ "fos_add", "Created by FOS team, fos@nowhere.com", PTR_START_OF(fos_add)},
		{ "fos_alloc", "Created by FOS team, fos@nowhere.com", PTR_START_OF(fos_alloc)},
		{ "fos_input", "Created by FOS team, fos@nowhere.com", PTR_START_OF(fos_input)},
		{ "fos_game", "Created by FOS team, fos@nowhere.com", PTR_START_OF(game)},
		{ "fos_static_data_section", "Created by FOS team, fos@nowhere.com", PTR_START_OF(fos_static_data_section)},
		{ "fos_data_on_stack", "Created by FOS team, fos@nowhere.com", PTR_START_OF(fos_data_on_stack)},
		{ "tqs", "Quicksort without freeHeap", PTR_START_OF(quicksort_heap)},

		{ "fact", "Factorial Recursive", PTR_START_OF(fos_factorial)},
		{ "fib", "Fibonacci Recursive", PTR_START_OF(fos_fibonacci)},

		{ "tshr4", "Tests the shared variables", PTR_START_OF(tst_sharing_4)},
		{ "tshr5", "Tests the shared variables", PTR_START_OF(tst_sharing_5_master)},
		{ "tshr5slave", "Tests the shared variables", PTR_START_OF(tst_sharing_5_slave)},
		{ "tshr5slaveB1", "Tests the shared variables", PTR_START_OF(tst_sharing_5_slaveB1)},
		{ "tshr5slaveB2", "Tests the shared variables", PTR_START_OF(tst_sharing_5_slaveB2)},


		/*TESTING 2014*/
		//[1] READY MADE TESTS
		{ "tf1", "tests free (1): num of freed frames", PTR_START_OF(tst_free_1)},
		{ "tf2", "tests free (2): try accessing values in freed spaces", PTR_START_OF(tst_free_2)},
		{ "tm1", "tests malloc (1): start address & allocated frames", PTR_START_OF(tst_malloc_1)},
		{ "tm2", "tests malloc (2): writing & reading values in allocated spaces", PTR_START_OF(tst_malloc_2)},
		{ "tff1", "tests first fit (1): always find suitable space", PTR_START_OF(tst_first_fit_1)},
		{ "tff2", "tests first fit (2): no suitable space", PTR_START_OF(tst_first_fit_2)},
		{ "tshr1", "Tests the shared variables [create]", PTR_START_OF(tst_sharing_1)},
		{ "tshr2", "Tests the shared variables [create, get and perms]", PTR_START_OF(tst_sharing_2master)},
		{ "shr2Slave1", "[Slave program1] of tst_sharing_2master", PTR_START_OF(tst_sharing_2slave1)},
		{ "shr2Slave2", "[Slave program2] of tst_sharing_2master", PTR_START_OF(tst_sharing_2slave2)},
		{ "tshr3", "Tests the shared variables [Special cases of create]", PTR_START_OF(tst_sharing_3)},
		{ "qs", "Quicksort with NO memory leakage", PTR_START_OF(quicksort_noleakage)},
		{ "ms1", "Mergesort with NO memory leakage", PTR_START_OF(mergesort_noleakage)},
		{ "ms2", "Mergesort that cause memory leakage", PTR_START_OF(mergesort_leakage)},

		//Final Test Programs
		{ "tst1", "", PTR_START_OF(tst1)},
		{ "tst2", "", PTR_START_OF(tst2)},
		{ "tst3", "", PTR_START_OF(tst3)},
		{ "tst4", "", PTR_START_OF(tst4)},
		{ "tst5", "", PTR_START_OF(tst5)},
		{ "tst6", "", PTR_START_OF(tst6)},
		{ "ra1", "", PTR_START_OF(tst_realloc_1)},
		{ "ra2", "", PTR_START_OF(tst_realloc_2)},
};

///=========================================================

// To be used as extern in other files 
struct UserProgramInfo* ptr_UserPrograms = &userPrograms[0];

// Number of user programs in the program table
int NUM_USER_PROGS = (sizeof(userPrograms)/sizeof(userPrograms[0]));

//
// Allocates and initializes a new environment.
// On success, the new environment is stored in *e.
//
// Returns 0 on success, < 0 on failure.  Errors include:
//	E_NO_FREE_ENV if all NENVS environments are allocated
//
int allocate_environment(struct Env** e)
{	
	if (!(*e = LIST_FIRST(&env_free_list)))
		return E_NO_FREE_ENV;
	return 0;
}

// Free the given environment "e", simply by adding it to the free environment list.
void free_environment(struct Env* e)
{
	memset(e, 0, sizeof(*e));
	e->env_status = ENV_FREE;
	LIST_INSERT_HEAD(&env_free_list, e);
}


//
// Initialize the kernel virtual memory layout for environment e.
// Given a pointer to an allocated page directory, set the e->env_pgdir and e->env_cr3 accordingly,
// and initialize the kernel portion of the new environment's address space.
// Do NOT (yet) map anything into the user portion
// of the environment's virtual address space.
//
void initialize_environment(struct Env* e, uint32* ptr_user_page_directory)
{	
	//panic("initialize_environment function is not completed yet") ;
	// [1] initialize the kernel portion of the new environment's address space.
	// [2] set e->env_pgdir and e->env_cr3 accordingly,
	int i;
	e->env_pgdir = ptr_user_page_directory; 
	e->env_cr3 = K_PHYSICAL_ADDRESS(ptr_user_page_directory) ;

	for (i = 0 ; i < PDX(USER_TOP) ; i++)
	{
		e->env_pgdir[i] = 0 ;
	}

	for (i = PDX(USER_TOP) ; i < 1024 ; i++)
	{
		e->env_pgdir[i] = ptr_page_directory[i] ;
	}

	//initialize environment working set
	for(i=0; i< PAGE_WS_MAX_SIZE; i++)
	{
		e->pageWorkingSet[i].virtual_address = 0;
		e->pageWorkingSet[i].empty = 1;
		e->pageWorkingSet[i].time_stamp = 0 ;
	}
	e->page_last_WS_index = 0;	

	for(i=0; i< TABLE_WS_MAX_SIZE; i++)
	{
		e->tableWorkingSet[i].virtual_address = 0;
		e->tableWorkingSet[i].empty = 1;
		e->tableWorkingSet[i].time_stamp = 0 ;
	}
	e->table_last_WS_index = 0;

	e->pageFaultsCounter=0;
	e->tableFaultsCounter=0;

	e->nModifiedPages=0;
	e->nNotModifiedPages=0;

	e->shared_free_address = USER_SHARED_MEM_START;

	//Completes other environment initializations, (envID, status and most of registers)
	complete_environment_initialization(e);	
}

//
// Allocate length bytes of physical memory for environment env,
// and map it at virtual address va in the environment's address space.
// Does not zero or otherwise initialize the mapped pages in any way.
// Pages should be writable by user and kernel.
//
// The allocation shouldn't failed
// return 0
//
static int program_segment_alloc_map_copy_workingset(struct Env *e, struct ProgramSegment* seg, uint32* allocated_pages, uint32 remaining_ws_pages, uint32* lastTableNumber)
{
	void *vaddr = seg->virtual_address;
	uint32 length = seg->size_in_memory;

	uint32 end_vaddr = ROUNDUP((uint32)vaddr + length,PAGE_SIZE) ;
	uint32 iVA = ROUNDDOWN((uint32)vaddr,PAGE_SIZE) ;
	int r ;
	uint32 i = 0 ;
	struct Frame_Info *p = NULL;

	*allocated_pages =0;

	for (; iVA < end_vaddr && i<remaining_ws_pages; i++, iVA += PAGE_SIZE)	
	{
		// Allocate a page 
		allocate_frame(&p) ;

		LOG_STRING("segment page allocated");
		loadtime_map_frame(e->env_pgdir, p, (void *)iVA, PERM_USER | PERM_WRITEABLE);
		LOG_STRING("segment page mapped");	

		LOG_STATMENT(cprintf("Updating working set entry # %d",e->page_last_WS_index));

		e->pageWorkingSet[e->page_last_WS_index].virtual_address = iVA;	
		e->pageWorkingSet[e->page_last_WS_index].empty = 0;
		e->pageWorkingSet[e->page_last_WS_index].time_stamp = 0;

		e->page_last_WS_index ++;
		e->page_last_WS_index %= PAGE_WS_MAX_SIZE;

		//if a new table is created during the mapping, add it to the table working set
		if(PDX(iVA) != (*lastTableNumber))
		{
			addTableToTableWorkingSet(e, ROUNDDOWN(iVA, PAGE_SIZE*1024));
			if (e->table_last_WS_index == 0)
				panic("\nenv_create: Table working set become FULL during the application loading. Please increase the table working set size to be able to load the program successfully\n");
			(*lastTableNumber) = PDX(iVA);
		}

		/// TAKE CARE !!!! this was an destructive error
		/// DON'T MAKE IT " *allocated_pages ++ " EVER !
		(*allocated_pages) ++;
	}

	uint8 *src_ptr = (uint8 *)(seg->ptr_start) ;
	uint8 *dst_ptr = (uint8 *) seg->virtual_address;

	//copy program segment page from (seg->ptr_start) to (seg->virtual_address) 	

	LOG_STATMENT(cprintf("copying data to allocated area VA %x from source %x",dst_ptr,src_ptr));
	while((uint32)dst_ptr < (ROUNDDOWN((uint32)vaddr,PAGE_SIZE) + (*allocated_pages)*PAGE_SIZE) &&
			((uint32)dst_ptr< ((uint32)vaddr+ seg->size_in_file)) )
	{
		*dst_ptr = *src_ptr ;
		dst_ptr++ ;
		src_ptr++ ;
	}
	LOG_STRING("zeroing remaining page space");	
	while((uint32)dst_ptr < (ROUNDDOWN((uint32)vaddr,PAGE_SIZE) + (*allocated_pages)*PAGE_SIZE) )
	{
		*dst_ptr = 0;
		dst_ptr++ ;
	}

	//============================================

	//	LOG_STRING("creating page tables");
	//	iVA = ROUNDDOWN((uint32)vaddr,PAGE_SIZE) ;
	//	i = 0 ;
	//	for (; iVA < end_vaddr; i++, iVA += PAGE_SIZE)
	//	{
	//		uint32 *ptr_page_table;
	//		get_page_table(e->env_pgdir, (void *)iVA, 1, &ptr_page_table);
	//	}
	//	LOG_STRING("page tables created successfully");
	return 0;
}

// Allocates a new env and loads the named user program into it.
struct Env* env_create(char* user_program_name)
{
	//[1] get pointer to the start of the "user_program_name" program in memory
	// Hint: use "get_user_program_info" function, 
	// you should set the following "ptr_program_start" by the start address of the user program 
	uint8* ptr_program_start = 0; 

	struct UserProgramInfo* ptr_user_program_info = get_user_program_info(user_program_name);	
	if(ptr_user_program_info == 0) return NULL;
	ptr_program_start = ptr_user_program_info->ptr_start ;
//	if(ptr_user_program_info->environment != NULL)
//	{
//		cprintf("env_create: an old environment already exist for [%s]!! \nfreeing the old one by calling env_free....\n", ptr_user_program_info->environment->prog_name);
//		env_free(ptr_user_program_info->environment);
//
//		//return ptr_user_program_info;
//	}


	//[2] allocate new environment, (from the free environment list)
	//if there's no one, return NULL
	// Hint: use "allocate_environment" function
	struct Env* e = NULL;
	if(allocate_environment(&e) < 0)
	{
		return 0;
	}

	//[2.5 - 2012] Set program name inside the environment
	e->prog_name = ptr_user_program_info->name ;

	//[3] allocate a frame for the page directory, Don't forget to set the references of the allocated frame.
	//REMEMBER: "allocate_frame" should always return a free frame
	int r;
	struct Frame_Info *p = NULL;

	allocate_frame(&p) ;
	p->references = 1;

	//[4] initialize the new environment by the virtual address of the page directory 
	// Hint: use "initialize_environment" function
	uint32* ptr_user_page_directory = K_VIRTUAL_ADDRESS(to_physical_address(p));	
	initialize_environment(e, ptr_user_page_directory);

	// We want to load the program into the user virtual space
	// each program is constructed from one or more segments,
	// each segment has the following information grouped in "struct ProgramSegment"
	//	1- uint8 *ptr_start: 	start address of this segment in memory 
	//	2- uint32 size_in_file: size occupied by this segment inside the program file, 
	//	3- uint32 size_in_memory: actual size required by this segment in memory
	// 	usually size_in_file < or = size_in_memory 
	//	4- uint8 *virtual_address: start virtual address that this segment should be copied to it  

	//[6] switch to user page directory
	// Hint: use rcr3() and lcr3()	
	uint32 kern_phys_pgdir = rcr3() ;	
	lcr3(e->env_cr3) ;

	//[7] load each program segment into user virtual space
	struct ProgramSegment* seg = NULL;  //use inside PROGRAM_SEGMENT_FOREACH as current segment information	
	int segment_counter=0;
	uint32 remaining_ws_pages = PAGE_WS_MAX_SIZE-1; // we are reserving 1 page of WS for the stack that will be allocated just before the end of this function
	uint32 lastTableNumber=0xffffffff;

	PROGRAM_SEGMENT_FOREACH(seg, ptr_program_start)
	{
		segment_counter++;
		//allocate space for current program segment and map it at seg->virtual_address then copy its content
		// from seg->ptr_start to seg->virtual_address 
		//Hint: use program_segment_alloc_map_copy_workingset() 

		//cprintf("SEGMENT #%d, dest start va = %x, dest end va = %x\n",segment_counter, seg->virtual_address, (seg->virtual_address + seg->size_in_memory));
		LOG_STRING("===============================================================================");
		LOG_STATMENT(cprintf("SEGMENT #%d, size_in_file = %d, size_in_memory= %d, dest va = %x",segment_counter,seg->size_in_file,
				seg->size_in_memory, seg->virtual_address));
		LOG_STRING("===============================================================================");

		uint32 allocated_pages=0;
		program_segment_alloc_map_copy_workingset(e, seg, &allocated_pages, remaining_ws_pages, &lastTableNumber);

		remaining_ws_pages -= allocated_pages;		
		LOG_STATMENT(cprintf("SEGMENT: allocated pages in WS = %d",allocated_pages));
		LOG_STATMENT(cprintf("SEGMENT: remaining WS pages after allocation = %d",remaining_ws_pages));


		///[1] temporary initialize 1st page in memory then writing it on page file
		uint32 dataSrc_va = (uint32) seg->ptr_start;
		uint32 seg_va = (uint32) seg->virtual_address ;

		uint32 start_first_page = ROUNDDOWN(seg_va , PAGE_SIZE);
		uint32 end_first_page = ROUNDUP(seg_va , PAGE_SIZE);
		uint32 offset_first_page = seg_va  - start_first_page ; 

		memset(ptr_temp_page , 0, PAGE_SIZE);
		uint8 *src_ptr =  (uint8*) dataSrc_va;
		uint8 *dst_ptr =  (uint8*) (ptr_temp_page + offset_first_page);
		int i;
		for (i = seg_va ; i < end_first_page ; i++, src_ptr++,dst_ptr++ )
		{
			*dst_ptr = *src_ptr ;
		}

		if (pf_add_env_page(e, start_first_page, ptr_temp_page) == E_NO_PAGE_FILE_SPACE)
			panic("ERROR: Page File OUT OF SPACE. can't load the program in Page file!!");

		//LOG_STRING(" -------------------- PAGE FILE: 1st page is written");


		///[2] Start writing the segment ,from 2nd page until before last page, to page file ...		

		uint32 start_last_page = ROUNDDOWN(seg_va  + seg->size_in_file, PAGE_SIZE) ;
		uint32 end_last_page = seg_va  + seg->size_in_file;

		for (i = end_first_page ; i < start_last_page ; i+= PAGE_SIZE, src_ptr+= PAGE_SIZE)
		{
			if (pf_add_env_page(e, i, src_ptr) == E_NO_PAGE_FILE_SPACE)
				panic("ERROR: Page File OUT OF SPACE. can't load the program in Page file!!");

		}		
		//LOG_STRING(" -------------------- PAGE FILE: 2nd page --> before last page are written");

		///[3] temporary initialize last page in memory then writing it on page file

		dst_ptr =  (uint8*) ptr_temp_page;
		memset(dst_ptr, 0, PAGE_SIZE);

		for (i = start_last_page ; i < end_last_page ; i++, src_ptr++,dst_ptr++ )
		{
			*dst_ptr = *src_ptr;
		}
		if (pf_add_env_page(e, start_last_page, ptr_temp_page) == E_NO_PAGE_FILE_SPACE)
			panic("ERROR: Page File OUT OF SPACE. can't load the program in Page file!!");


		//LOG_STRING(" -------------------- PAGE FILE: last page is written");

		///[4] writing the remaining seg->size_in_memory pages to disk

		uint32 start_remaining_area = ROUNDUP(seg_va + seg->size_in_file,PAGE_SIZE) ;
		uint32 remainingLength = (seg_va + seg->size_in_memory) - start_remaining_area ;

		for (i=0 ; i < ROUNDUP(remainingLength,PAGE_SIZE) ;i+= PAGE_SIZE, start_remaining_area += PAGE_SIZE)
		{
			if (pf_add_empty_env_page(e, start_remaining_area) == E_NO_PAGE_FILE_SPACE)
				panic("ERROR: Page File OUT OF SPACE. can't load the program in Page file!!");
		}
		//LOG_STRING(" -------------------- PAGE FILE: segment remaining area is written (the zeros) ");
	}

	/// set the modified bit of each page in the pageWorkingSet to 0
	int i=0;
	for(;i<PAGE_WS_MAX_SIZE; i++) 
	{
		if(e->pageWorkingSet[i].empty == 0) 
		{
			uint32 virtual_address = e->pageWorkingSet[i].virtual_address;
			uint32* ptr_page_table;

			//Here, page tables of all working set pages should be exist in memory
			//So, get_page_table should return the existing table
			get_page_table(e->env_pgdir, (void *)virtual_address, &ptr_page_table);
			ptr_page_table[PTX(virtual_address)] &= (~PERM_MODIFIED);			
		}
	}


	//unsigned int * ptr = (unsigned int * )0x801084;
	//LOG_STATMENT(cprintf("contents at address %x=%x",0x801084, *ptr));

	//[8] now set the entry point of the environment
	//Hint: use set_environment_entry_point()
	set_environment_entry_point(e, ptr_user_program_info->ptr_start);

	//[9] Allocate and map one page for the program's initial stack
	// at virtual address USTACKTOP - PAGE_SIZE.
	// we assume that the stack is counted in the environment working set


	// map the allocated page
	uint32 ptr_user_stack_bottom = (USTACKTOP - 1*PAGE_SIZE);

	uint32 stackVa = USTACKTOP - PAGE_SIZE;
	for(;stackVa >= ptr_user_stack_bottom; stackVa -= PAGE_SIZE)
	{
		struct Frame_Info *pp = NULL;
		allocate_frame(&pp);

		loadtime_map_frame(e->env_pgdir, pp, (void*)stackVa, PERM_USER | PERM_WRITEABLE);

		//initialize new page by 0's
		memset((void*)stackVa, 0, PAGE_SIZE);

		//now add it to the working set and the page table
		{
			//env_page_ws_set_entry(e, e->page_last_WS_index, (uint32) stackVa) ;
			//e->page_last_WS_index ++;
			//e->page_last_WS_index %= PAGE_WS_MAX_SIZE;

			//addTableToTableWorkingSet(e, ROUNDDOWN((uint32)stackVa, PAGE_SIZE*1024));

			//int success = pf_add_empty_env_page(e, (uint32)stackVa);
			//if(success == 0) LOG_STATMENT(cprintf("STACK Page added to page file successfully\n"));
		}
	}

	//		cprintf("Table working set after loading the program...\n");
	//		env_table_ws_print(e);
	//
	//
	//		cprintf("Page working set after loading the program...\n");
	//		env_page_ws_print(e);

	///[10] switch back to the page directory exists before segment loading 
	lcr3(kern_phys_pgdir) ;

	//[11] 2012, add the ready env we have just created to the scheduler ready queue

	sched_insert_new(e);

	return e;

}

// Used to run the given environment "e", simply by 
// context switch from curenv to env e.
//  (This function does not return.)
//
void env_run(struct Env *e)
{
	if(curenv != e)
	{
		curenv = e ;
		curenv->env_runs++ ;
		lcr3(curenv->env_cr3) ;	
	}	
	//uint16 cnt0 = kclock_read_cnt0_latch();
	//cprintf("env_run: Counter0 Before Restart Again = %x\n", cnt0);
	//kclock_start();

	kclock_resume();
	env_pop_tf(&(curenv->env_tf));
}

//
// Frees environment "e" and all memory it uses.
// 
void env_free(struct Env *e)
{
	panic("env_free is not implemented...");
}

///*****************************************************************************************
///*****************************************************************************************

// Mark all environments in 'envs' as free, set their env_ids to 0,
// and insert them into the env_free_list.
// Insert in reverse order, so that the first call to allocate_environment()
// returns envs[0].
//

void env_init(void)
{	
	int iEnv = NENV-1;
	for(; iEnv >= 0; iEnv--)
	{
		envs[iEnv].env_status = ENV_FREE;
		envs[iEnv].env_id = 0;
		LIST_INSERT_HEAD(&env_free_list, &envs[iEnv]);
	}
}

void complete_environment_initialization(struct Env* e)
{	
	//VPT and UVPT map the env's own page table, with
	//different permissions.
	e->env_pgdir[PDX(VPT)]  = e->env_cr3 | PERM_PRESENT | PERM_WRITEABLE;
	e->env_pgdir[PDX(UVPT)] = e->env_cr3 | PERM_PRESENT | PERM_USER;

	// page file directory initialization
	e->env_disk_pgdir= 0;

	int32 generation;	
	// Generate an env_id for this environment.
	generation = (e->env_id + (1 << ENVGENSHIFT)) & ~(NENV - 1);
	if (generation <= 0)	// Don't create a negative env_id.
		generation = 1 << ENVGENSHIFT;
	e->env_id = generation | (e - envs);

	// Set the basic status variables.
	e->env_parent_id = 0;//parent_id;
	e->env_status = ENV_NEW;
	e->env_runs = 0;

	// Clear out all the saved register state,
	// to prevent the register values
	// of a prior environment inhabiting this Env structure
	// from "leaking" into our new environment.
	memset(&e->env_tf, 0, sizeof(e->env_tf));

	// Set up appropriate initial values for the segment registers.
	// GD_UD is the user data segment selector in the GDT, and 
	// GD_UT is the user text segment selector (see inc/memlayout.h).
	// The low 2 bits of each segment register contains the
	// Requester Privilege Level (RPL); 3 means user mode.

	e->env_tf.tf_ds = GD_UD | 3;
	e->env_tf.tf_es = GD_UD | 3;
	e->env_tf.tf_ss = GD_UD | 3;
	e->env_tf.tf_esp = (uint32*)USTACKTOP;
	e->env_tf.tf_cs = GD_UT | 3;
	e->env_tf.tf_eflags |= FL_IF;

	// You will set e->env_tf.tf_eip later.

	// commit the allocation
	LIST_REMOVE(&env_free_list ,e);
	return ;
}

struct ProgramSegment* PROGRAM_SEGMENT_NEXT(struct ProgramSegment* seg, uint8* ptr_program_start)
{
	int index = (*seg).segment_id++;

	struct Proghdr *ph, *eph; 
	struct Elf * pELFHDR = (struct Elf *)ptr_program_start ; 
	if (pELFHDR->e_magic != ELF_MAGIC) 
		panic("Matafa2nash 3ala Keda"); 
	ph = (struct Proghdr *) ( ((uint8 *) ptr_program_start) + pELFHDR->e_phoff);

	while (ph[(*seg).segment_id].p_type != ELF_PROG_LOAD && ((*seg).segment_id < pELFHDR->e_phnum)) (*seg).segment_id++;	
	index = (*seg).segment_id;

	if(index < pELFHDR->e_phnum)
	{
		(*seg).ptr_start = (uint8 *) ptr_program_start + ph[index].p_offset;
		(*seg).size_in_memory =  ph[index].p_memsz;
		(*seg).size_in_file = ph[index].p_filesz;
		(*seg).virtual_address = (uint8*)ph[index].p_va;
		return seg;
	}
	return 0;
												}

struct ProgramSegment PROGRAM_SEGMENT_FIRST( uint8* ptr_program_start)
{
	struct ProgramSegment seg;
	seg.segment_id = 0;

	struct Proghdr *ph, *eph; 
	struct Elf * pELFHDR = (struct Elf *)ptr_program_start ; 
	if (pELFHDR->e_magic != ELF_MAGIC) 
		panic("Matafa2nash 3ala Keda"); 
	ph = (struct Proghdr *) ( ((uint8 *) ptr_program_start) + pELFHDR->e_phoff);
	while (ph[(seg).segment_id].p_type != ELF_PROG_LOAD && ((seg).segment_id < pELFHDR->e_phnum)) (seg).segment_id++;
	int index = (seg).segment_id;

	if(index < pELFHDR->e_phnum)
	{	
		(seg).ptr_start = (uint8 *) ptr_program_start + ph[index].p_offset;
		(seg).size_in_memory =  ph[index].p_memsz;
		(seg).size_in_file = ph[index].p_filesz;
		(seg).virtual_address = (uint8*)ph[index].p_va;
		return seg;
	}
	seg.segment_id = -1;
	return seg;
}

struct UserProgramInfo* get_user_program_info(char* user_program_name)
{
	int i;
	for (i = 0; i < NUM_USER_PROGS; i++) {
		if (strcmp(user_program_name, userPrograms[i].name) == 0)
			break;
	}
	if(i==NUM_USER_PROGS) 
	{
		cprintf("Unknown user program '%s'\n", user_program_name);
		return 0;
	}

	return &userPrograms[i];
}

struct UserProgramInfo* get_user_program_info_by_env(struct Env* e)
{
	int i;
	for (i = 0; i < NUM_USER_PROGS; i++) {
		if ( strcmp( e->prog_name , userPrograms[i].name) ==0)
			break;
	}
	if(i==NUM_USER_PROGS) 
	{
		cprintf("Unknown user program \n");
		return 0;
	}

	return &userPrograms[i];
}

void set_environment_entry_point(struct Env* e, uint8* ptr_program_start)
{
	struct Elf * pELFHDR = (struct Elf *)ptr_program_start ; 
	if (pELFHDR->e_magic != ELF_MAGIC) 
		panic("Matafa2nash 3ala Keda"); 
	e->env_tf.tf_eip = (uint32*)pELFHDR->e_entry ;
}

//
// Frees environment e.
// If e was the current env, then runs a new environment (and does not return
// to the caller).
//
void env_destroy(struct Env *e) 
{
	env_free(e);

	//cprintf("Destroyed the only environment - nothing more to do!\n");
	while (1)
		run_command_prompt();
}

void env_run_cmd_prmpt()
{

	//NEW !! 2012, remove remaining pages in the modified list and reset counter (if any)
	struct Frame_Info *ptr_fi=NULL ;

	//	cprintf("[%s] deleting modified at end of env\n", curenv->prog_name);
	//	struct freeFramesCounters ffc = calculate_available_frames();
	//	cprintf("[%s] bef, mod = %d, fb = %d, fnb = %d\n",curenv->prog_name, ffc.modified, ffc.freeBuffered, ffc.freeNotBuffered);

	LIST_FOREACH(ptr_fi, &modified_frame_list)
	{
		if(ptr_fi->environment != curenv)
		{
			//			cprintf("[%s] frame env = %x, curenv = %x\n", curenv->prog_name ,ptr_fi->environment, curenv);
		}
		if(ptr_fi->environment == curenv)
		{
			pt_set_page_permissions(ptr_fi->environment, ptr_fi->va, 0, PERM_BUFFERED);

			ptr_fi->isBuffered=0;
			ptr_fi->references=0;
			ptr_fi->environment=0;
			ptr_fi->va=0;

			//cprintf("==================\n");
			//cprintf("[%s] ptr_fi = %x, ptr_fi next = %x \n",curenv->prog_name, ptr_fi, LIST_NEXT(ptr_fi));
			bufferlist_remove_page(&modified_frame_list, ptr_fi);
			LIST_INSERT_HEAD(&free_frame_list, ptr_fi) ;

			//			modified_list_count--;

			//cprintf("[%s] ptr_fi = %x, ptr_fi next = %x, saved next = %x \n", curenv->prog_name ,ptr_fi, LIST_NEXT(ptr_fi), ___ptr_next);
			//cprintf("==================\n");
		}
	}

	//	cprintf("[%s] finished deleting modified frames at the end of env\n", curenv->prog_name);
	//	struct freeFramesCounters ffc2 = calculate_available_frames();
	//	cprintf("[%s] aft, mod = %d, fb = %d, fnb = %d\n",curenv->prog_name, ffc2.modified, ffc2.freeBuffered, ffc2.freeNotBuffered);

	//LIST_CONCAT(&free_frame_list, &modified_frame_list);
	//modified_list_count = 0 ;


	//struct UserProgramInfo* upi= get_user_program_info_by_env(curenv);
//	// Clear out all the saved register state,
//	// to prevent the register values
//	// of a prior environment inhabiting this Env structure
//	// from "leaking" into our new environment.
//	memset(&curenv->env_tf, 0, sizeof(curenv->env_tf));
//
//	// Set up appropriate initial values for the segment registers.
//	// GD_UD is the user data segment selector in the GDT, and
//	// GD_UT is the user text segment selector (see inc/memlayout.h).
//	// The low 2 bits of each segment register contains the
//	// Requester Privilege Level (RPL); 3 means user mode.
//
//	curenv->env_tf.tf_ds = GD_UD | 3;
//	curenv->env_tf.tf_es = GD_UD | 3;
//	curenv->env_tf.tf_ss = GD_UD | 3;
//	curenv->env_tf.tf_esp = (uint32*)USTACKTOP;
//	curenv->env_tf.tf_cs = GD_UT | 3;
//	//set_environment_entry_point(upi);

	lcr3(K_PHYSICAL_ADDRESS(ptr_page_directory));


	//remove the process from the ready queue
	sched_remove_ready(curenv);
	curenv->env_status = ENV_EXIT;
	curenv = NULL;
	fos_scheduler();
}

//
// Restores the register values in the Trapframe with the 'iret' instruction.
// This exits the kernel and starts executing some environment's code.
// This function does not return.
//
void
env_pop_tf(struct Trapframe *tf)
{
	__asm __volatile("movl %0,%%esp\n"
			"\tpopal\n"
			"\tpopl %%es\n"
			"\tpopl %%ds\n"
			"\taddl $0x8,%%esp\n" /* skip tf_trapno and tf_errcode */
			"\tiret"
			: : "g" (tf) : "memory");
	panic("iret failed");  /* mostly to placate the compiler */
}
