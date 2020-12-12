
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <math.h>

// Define a name for your shared memory; you can give any name that start with a slash character; it will be like a filename.
#define SHM_NAME "/piruli"

// Define semaphore(s) 

// Define your stuctures and variables. 

/**
 * Create and initialize a shared memory segment.
 * if operation is successful return 0, else
 * return -1
 */
int smem_init(int segmentsize)
{
    //check if segmentsize is a power of two
    if ( ceil(log2(segmentsize)) != floor(log2(segmentsize)) )
        return -1;
    //shm_open()
    shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    //ftruncate()
    printf ("smem init calledW O O W\n"); // remove all printfs when you are submitting to us.  

    return (0); 
}

/**
 * Remove the shared memory segment from the system.
 * Return -1 if unsuccessful, 0 if successful.
 */
int smem_remove()
{

    return (0); 
}

/**
 * Indicates to the library that the process wants to
 * use the library. Library maps the shared segment to 
 * the virtual address space of the process using mmap().
 * If there are too many processes using the lib atm, 
 * returns -1, otherwise returns 0.
 */
int smem_open()
{

    return (0); 
}

/**
 * Shares memory of size at least reqsize.
 * Returns a pointer to the allocated space.
 * If memory cannot be allocated, returns NULL.
 */
void *smem_alloc (int reqsize)
{
    return (NULL);
}

/**
 * Deallocates the shared memory space pointed to 
 * by pointer ptr. The deallocated memory space will 
 * be part of the free memory in the segment. 
 */
void smem_free (void *p)
{

 
}

/**
 * When this is called by a process, the lib will 
 * know this process wont use it anymore.
 * The shared segment is unmapped from the virtual adress
 * space of the process calling this function.
 */
int smem_close()
{
    
    return (0); 
}

