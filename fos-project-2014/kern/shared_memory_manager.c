#include <inc/mmu.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/environment_definitions.h>

#include <kern/shared_memory_manager.h>
#include <kern/memory_manager.h>
#include <kern/syscall.h>

int getShareNumber(char* name)
{
	int i=0;
	for(; i< nShares; ++i)
	{
		if(strcmp(name, shares[i].name)==0)
		{
			return i;
		}
	}
	return -1;
}

int createSharedObject(char* shareName, uint32 size, uint8 isWritable, void** returned_shared_address)
{
	// your code is here, remove the panic and write your code
	//panic("createSharedObject() is not implemented yet...!!");
	struct Env* myenv = curenv; //The calling environment
	int32 kern_phys_pgdir = rcr3() ;
	lcr3(myenv->env_cr3) ;
	if(nShares>99)
		return E_NO_SHARE;
	else if(getShareNumber(shareName)!=-1)
		return E_SHARED_MEM_EXISTS;
	uint32 sz=ROUNDUP(size,PAGE_SIZE);
	uint32 add=myenv->shared_free_address;
	if(add+sz>=USER_SHARED_MEM_MAX)
		return E_NO_MEM;
	else
	{
		int i=0;
		struct Frame_Info* frame=NULL;
		int error=0;
		uint32* pgd=myenv->env_pgdir;
		shares[nShares].isWritable=isWritable;
		int len=strlen(shareName);
		for(i=0;i<len;i++)
			shares[nShares].name[i]=shareName[i];
		uint32* strg=shares[nShares].framesStorage;
		sz/=PAGE_SIZE;
		shares[nShares].size=sz;
		*returned_shared_address=(void*)add;
		for(i=0;i<sz;i++)
		{
			error=allocate_frame(&frame);
			if(error==E_NO_MEM)
			{
				*returned_shared_address=NULL;
				return E_NO_MEM;
			}
			map_frame(pgd,frame,(void*)add,PERM_USER|PERM_WRITEABLE);
			add_frame_to_storage(strg,frame,i);
			add+=PAGE_SIZE;
		}
		shares[nShares].references=1;
		nShares++;
		myenv->shared_free_address=add;
	}
	// This function should create the shared object with the given size
	// in the SHARED area of the current environment
	// and return the start address of it by setting the "*returned_shared_address"
	// RETURN:
	//	a) 0 if success
	//	b) E_NO_MEM if the required size exceed the max shared area "USER_SHARED_MEM_MAX"
	//	c) E_SHARED_MEM_EXISTS if the shared object already exists
	//	d) E_NO_SHARE if the number of shared objects reaches max "MAX_SHARES"

	// Steps:
	//	1) Allocate the required space in the physical memory on a PAGE boundary
	//	2) Map the allocated space on the "shared_free_address" of current environment "curenv": object OWNER, with writable permissions
	//	3) Update the "shared_free_address" of the Current environment to be aligned on a PAGE boundary (i.e. 4 KB boundary)
	//	4) Add all allocated frames to "frames_storage" of this shared object to keep track of them for later use
	//		(use: add_frame_to_storage())
	//	5) Add the shared object to the end of the "shares" array (use "nShares"), remember to:
	//		a) Set the data members of the object with suitable values (name, size, ...)
	//		b) Set references to 1 (as there's 1 user environment that use the object now - OWNER)
	//		c) Store the object's isWritable flag (0:ReadOnly, 1:Writable) for later use by getSharedObject()
	// 	6) Return the start address of the object by setting the "*returned_shared_address"
	lcr3(kern_phys_pgdir) ;
	return 0;
}

int getSharedObject(char* shareName, void** returned_shared_address)
{
	// This function should share the required object in SHARED area of the current environment (with the specified permissions: read_only/writable)
	// and return the start address of it by setting the "*returned_shared_address"
	// RETURN:
	//	a) 0 if success
	//	b) E_SHARED_MEM_NOT_EXISTS if the shared object is not exists
	int index=getShareNumber(shareName);
	if(index==-1)
		return E_SHARED_MEM_NOT_EXISTS;
	struct Env* myenv = curenv; //The calling environment
	int32 kern_phys_pgdir = rcr3() ;
	lcr3(myenv->env_cr3) ;
	struct Frame_Info *frame;
	int size=shares[index].size;
	int add=myenv->shared_free_address;
	int i=0;
	uint32* pgd=myenv->env_pgdir;
	uint8 writable=shares[index].isWritable;
	*returned_shared_address=(void*)add;
	for(;i<size;i++)
	{
		frame=get_frame_from_storage(shares[index].framesStorage,i);
		if(writable==1)
			map_frame(pgd,frame,(void*)add,PERM_USER|PERM_WRITEABLE);
		else
			map_frame(pgd,frame,(void*)add,PERM_USER);
		add+=PAGE_SIZE;
	}
	myenv->shared_free_address=add;
	shares[index].references++;
	lcr3(kern_phys_pgdir) ;
	return 0;
}

//========================== BONUS ============================
// Free Shared Object

int freeSharedObject(char* shareName)
{
	//TODO: [PROJECT 2014 - BONUS3] freeSharedObject()
	// your code is here, remove the panic and write your code
	//panic("freeSharedObject() is not implemented yet...!!");
	struct Env* myenv = curenv; //The calling environment
	int32 kern_phys_pgdir = rcr3() ;
	lcr3(myenv->env_cr3) ;
	int index=getShareNumber(shareName);
	if(index==-1)
		return E_SHARED_MEM_NOT_EXISTS;
	struct Frame_Info* frame=NULL;
	struct Frame_Info* frame2=NULL;
	uint32* pgd=myenv->env_pgdir;
	int size=shares[index].size;
	//cprintf("size=%d\n",size);
	//cprintf("add=%x\n",myenv->shared_free_address);
	uint32 va=myenv->shared_free_address-(size*PAGE_SIZE);
	uint32* pt=NULL;
	frame=get_frame_from_storage(shares[index].framesStorage,0);
	frame2=get_frame_info(pgd,(void*)va,&pt);
	if(frame==frame2&&frame!=NULL)
		freeMem(myenv,va,size);
	else
	{
		while(va>USER_SHARED_MEM_START)
		{
			frame2=get_frame_info(pgd,(void*)va,&pt);
			if(frame==frame2&&frame!=NULL)
			{
				freeMem(myenv,va,size);
				break;
			}
			va-=PAGE_SIZE;
		}
	}
	shares[index].references--;
	if(shares[index].references==0)
	{
		clear_frames_storage(shares[index].framesStorage);
		shares[index].isWritable=0;
		int len=strlen(shares[index].name);
		int k;
		for(k=0;k<len;k++)
			shares[index].name[k]=0;
		shares[index].size=0;
		struct Share s;
	}
	tlbflush();
	lcr3(kern_phys_pgdir);
	return 0;
	// This function should free (delete) the shared object from the SHARED area of the current environment
	// If this is the last shared env, then the "frames_store" should be cleared and the shared object should be deleted
	// RETURN:
	//	a) 0 if success
	//	b) E_SHARED_MEM_NOT_EXISTS if the shared object is not exists


	// Steps:
	//	1) Get the shared object from the "shares" array
	//	2) Find its start address in the current environment
	//	2) Unmap it from the current environment "curenv"
	//	3) If one or more table becomes empty, remove it
	//	4) Update references
	//	5) If this is the last share:
	//		a) clear the frames_storage of this shared object (use: clear_frames_storage())
	//		b) clear all other data members of this object (name, size, ...)
	//	6) Flush the cache "tlbflush()"


	return 0;
}
