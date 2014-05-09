	
#include <inc/lib.h>
#define HeapSize 262144
/*
 * Simple malloc()
 *
 * The address space for the dynamic allocation is 
 * from "USER_HEAP_START" to "USER_HEAP_MAX"-1
 * Pages are allocated ON 4KB ALIGMENT
 * On succeed, return void pointer to the allocated space
 * return NULL if
 *	-there's no suitable space for the required allocation
 */

// malloc()
//	This function use FIRST FIT strategy to allocate space in heap with the given size
//  and return void pointer to the start of the allocated space

//	To do this, we need to switch to the kernel, allocate the required space
//	in Main memory then switch back to the user again.
//
//	We can use sys_allocateMem(uint32 virtual_address, uint32 size); which
//		switches to the kernel mode, calls allocateMem(struct Env* e, uint32 virtual_address, uint32 size) in
//		"memory_manager.c", then switch back to the user mode here
//	the allocateMem function is empty, make sure to implement it.
uint32 used[HeapSize];
uint32 sz[HeapSize];
int arr_count=1;
void* malloc(uint32 size)
{	
	//TODO: [PROJECT 2014 - Heap] malloc()
	// your code is here, remove the panic and write your code
	//panic("malloc() is not implemented yet...!!");

	// Steps:
	//	1) search for FIRST FIT space in heap that is suitable for the required allocation size
	uint32 free=sys_calculate_free_frames() ;
	size=ROUNDUP(size,PAGE_SIZE);
	size/=PAGE_SIZE;
	uint32 address=USER_HEAP_START;
	int i=0;
	int count=0;
	int start;
	for(;i<HeapSize;i++)
	{
		if(used[i]==0)
		{
			start=i;
			for(;count<size;count++)
			{
				if(used[i]==0)
				{
					i++;
					continue;
				}
				else
				{
					count=0;
					break;
				}
				i++;
			}
			if(size==count)
			{
				int k;
				for(k=0;k<count;k++)
					used[k+start]=arr_count;
				break;
			}

		}
	}
	if(count<size)
		return NULL;
	start*=PAGE_SIZE;
	address+=start;
	sys_allocateMem(address,size);
	sz[arr_count]=size;
	arr_count++;
	return (uint32*)address;
	//	2) if no suitable space found, return NULL
	//	 Else,
	//	3) Call sys_allocateMem to invoke the Kernel for allocation
	// 	4) Return pointer containing the virtual address of allocated space,
	//

	//This function should allocate ALL pages of the required range
	// ******** ON 4KB ALIGNMENT *******************
}


// free():
//	This function frees the allocation of the given virtual_address
//	To do this, we need to switch to the kernel, free the pages AND empty PAGE TABLES only
//	from Main memory then switch back to the user again.
//	
//	We can use sys_freeMem(uint32 virtual_address, uint32 size); which
//		switches to the kernel mode, calls freeMem(struct Env* e, uint32 virtual_address, uint32 size) in
//		"memory_manager.c", then switch back to the user mode here
//	the freeMem function is empty, make sure to implement it.
void free(void* virtual_address)
{
	uint32 free2=sys_calculate_free_frames() ;
	//TODO: [PROJECT 2014 - Heap] free()
	// your code is here, remove the panic and write your code
	//panic("free() is not implemented yet...!!");
	uint32 add=(uint32)virtual_address-USER_HEAP_START;
	add/=PAGE_SIZE;
	int i;
	uint32 cur=used[add];
	for(i=0;i<used[add];i++)
	{
		if(used[i]==cur)
			{
				used[add]=0;
			}
		else
			break;
	}
	sys_freeMem((uint32)virtual_address,sz[cur]);
	//get the size of the given allocation using its address
	//you need to call sys_freeMem()

}


//================= [BONUS] =====================

// realloc():

//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to malloc().
//	A call with new_size = zero is equivalent to free().

//  Hint: you may need to use the sys_moveMem(uint32 src_virtual_address, uint32 dst_virtual_address, uint32 size)
//		which switches to the kernel mode, calls moveMem(struct Env* e, uint32 src_virtual_address, uint32 dst_virtual_address, uint32 size)
//		in "memory_manager.c", then switch back to the user mode here
//	the moveMem function is empty, make sure to implement it.

void *realloc(void *virtual_address, uint32 new_size)
{
	//TODO: [PROJECT 2014 - BONUS1] realloc()
	// your code is here, remove the panic and write your code
	panic("realloc() is not implemented yet...!!");

}

