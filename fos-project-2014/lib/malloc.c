	
#include <inc/lib.h>
#define HeapSize (USER_HEAP_MAX-USER_HEAP_START)/PAGE_SIZE
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
int arr_count=1;
int begining(int index,uint32 size)
{
	int i=index;
	int count=0;
	int start;
	if(index==0)
	{
		for(;i<HeapSize;i++)
		{
			if(used[i]==0)
			{
				start=i;
				for(;count<size;count++)
				{
					if(used[i]==0&&i<HeapSize)
					{
						++i;
						continue;
					}
					else
					{
						count=0;
						break;
					}
				}
				if(count==size)
					{
						return start;
					}

			}
		}
		if(count<size)
		{
			return -1;
		}

	}
	else
	{
		for(;i<HeapSize;i++)
		{
			if(used[i]==0)
			{
				for(;count<size;count++)
				{
					if(used[i]==0&&i<HeapSize)
					{
						++i;
						continue;
					}
					else
					{
						return -1;
					}
				}
				return 1;
			}
			return -1;
		}
		return -1;
	}
return -1;
}
void* malloc(uint32 size)
{
	// your code is here, remove the panic and write your code
	//panic("malloc() is not implemented yet...!!");

	// Steps:
	//	1) search for FIRST FIT space in heap that is suitable for the required allocation size
	size=ROUNDUP(size,PAGE_SIZE);
	size/=PAGE_SIZE;
	uint32 address=USER_HEAP_START;
	int start=begining(0,size);
	if(start==-1)
		return NULL;
	int k;
	for(k=0;k<size;k++)
		used[k+start]=arr_count;
	start*=PAGE_SIZE;
	address+=start;
	sys_allocateMem(address,size);
	arr_count++;
	return (uint32*)address;
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
	// your code is here, remove the panic and write your code
	//panic("free() is not implemented yet...!!");
	uint32 add=(uint32)virtual_address-USER_HEAP_START;
	add/=PAGE_SIZE;
	int i=add;
	uint32 size=0;
	uint32 cur=used[add];
	while(1==1)
	{
		if(used[i]==cur)
			{
				used[i]=0;
				i++;
				size++;
			}
		else
			break;
	}
	sys_freeMem((uint32)virtual_address,size);
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
	// your code is here, remove the panic and write your code
	//panic("realloc() is not implemented yet...!!");
	if(virtual_address==NULL)
		return malloc(new_size);
	else
	{
		uint32 add=(uint32)virtual_address-USER_HEAP_START;
		add/=PAGE_SIZE;
		int i=add;
		uint32 size=0;
		uint32 orgSize=new_size;
		new_size=ROUNDUP(new_size,PAGE_SIZE);
		new_size/=PAGE_SIZE;
		uint32 va=(uint32)virtual_address;
		uint32 cur=used[add];
		while(1==1)
		{
			if(used[i]==cur)
				{
					i++;
					size++;
				}
			else
				break;
		}
		if(new_size==0)
		{
			free(virtual_address);
			return NULL;
		}
		else
		{
			int remainder=new_size-size;
			int flag=begining(i,remainder);

			if(flag==1)
			{
				sys_allocateMem(va+(size*PAGE_SIZE),remainder);
				int k;
				for(k=i;k<i+remainder;k++)
					used[k]=cur;
				return virtual_address;
			}
			else
			{
				void* dest=malloc(orgSize);
				if(dest==NULL)
					return virtual_address;
				int k;
				for(k=add;k<add+size;k++)
				{
					if(used[k]==cur)
						used[k]=0;
				}
				sys_moveMem((uint32)virtual_address,(uint32)dest,size);
				return dest;
			}
		}
	}
	return NULL;
}

