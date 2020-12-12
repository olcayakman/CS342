#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <math.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

#include "smemlib.h"

// Define a name for your shared memory; you can give any name that start with a slash character; it will be like a filename.
#define SHM_NAME "/piruli"
#define NO_OF_SEGMENTS 16777216
#define MAX_SEGMENT_SIZE 16777216

// Define semaphore(s) 
#define SEMNAME_MUTEX "SEMNAME_MUTEX"
#define SEMNAME_FULL "SEMNAME_FULL"
#define SEMNAME_EMPTY "SEMNAME_EMPTY"

// Define your stuctures and variables. 
sem_t *sem_mutex; //protects the buffer
sem_t *sem_full; //counts the number of items
sem_t *sem_empty; //counts the number of empty buffer slots

int shm;
void *shm_start;
struct stat sbuf;

struct segment* holes_head;
struct segment* holes_cur;
int holes_size;
struct segment* allocated_segments_head;
struct segment* allocated_segments_cur;
int alloc_size;


void add_to_list( struct segment** head, int size, int address){
    struct segment *cur = *head, *new;
    new = (struct segment*) malloc (sizeof(struct segment));

    if( new == NULL){
        if(DEBUG)
            printf("cannot create a new node \n");
        return;
    }
    
    new->segment_pointer = address;
    new->segment_size = size;
    new->next = NULL;

    if( (*head) == NULL){
        *head = new;
         if(DEBUG)
            printf("Item succesfully added \n");
        return;    
    }
    if( address < (*head)->segment_pointer){
        new->next = cur;
        *head = new;
        if(DEBUG)
            printf("Item succesfully added \n");
        return;
    }

    while(cur->next){
        if( address <= (cur->next)->segment_pointer)
            break;
        else
            cur = cur->next;    
    }

    if(cur->next == NULL){
        cur->next = new;
        if(DEBUG)
            printf("Item succesfully added \n");
        return;
    }
    else{
        new->next = cur->next;
        cur->next = new;
        if(DEBUG)
            printf("Item succesfully added \n");
        return;
    }
    
}

/**
 * Create and initialize a shared memory segment.
 * if operation is successful return 0, else
 * return -1
 */
int smem_init(int segmentsize)
{
    if (DEBUG)
        printf ("smem init called\n"); // remove all printfs when you are submitting to us.  

    //check if segmentsize is a power of two
    if ( ceil(log2(segmentsize)) != floor(log2(segmentsize)) )
        return -1;

    //clean the shared memory with the same name 
    shm_unlink(SHM_NAME);
    //create a shared memory segment  
    shm = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666); 
    if(shm < 0){
        if (DEBUG)
            printf("shared memory segment cannot be created \n");
        return -1;
    }
    if (DEBUG)
        printf("shm is created\n");
    //set the size of shm
    ftruncate(shm,segmentsize);
    if(DEBUG)
        printf("shm size = %d\n", segmentsize);

    // initially there is no segment allocated,
    // there is one giant hole, the size of segmentsize.
    allocated_segments_head = NULL;
    alloc_size = 0;

    holes_head->segment_pointer = shm;
    holes_head->segment_size = segmentsize;
    holes_head->next = NULL;
    holes_size = 1;


    return (0); 
}

/**
 * Remove the shared memory segment from the system.
 * Return -1 if unsuccessful, 0 if successful.
 */
int smem_remove()
{
    //BUNU BURDA MI YAPCAM BİLEMEDİM
    sem_close(sem_mutex);
    sem_close(sem_full);
    sem_close(sem_empty);

    //remove the semophores 
    sem_unlink(SEMNAME_MUTEX);
    sem_unlink(SEMNAME_FULL);
    sem_unlink(SEMNAME_EMPTY);

    //remove the shared memory
    shm_unlink(SHM_NAME);

    if(DEBUG)
        printf("shared memory segment is removed\n");
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
    fstat(shm, &sbuf); /* get info about the shared memory */
    if (DEBUG)
        printf("size = %d\n", (int) sbuf.st_size);
    shm_start = mmap(NULL, sbuf.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm, 0);
    if (shm_start < 0) {
        if (DEBUG)
            perror("cannot map the shared memory");
        return -1;
    } else if (DEBUG) {
        printf("mapping ok, start address = %lu\n", (unsigned long) shm_start);
    }
    return (0); 
}

/**
 * Shares memory of size at least reqsize.
 * Returns a pointer to the allocated space.
 * If memory cannot be allocated, returns NULL.
 */
void *smem_alloc (int reqsize)
{
    /* first clean up semaphores with same names */
    sem_unlink(SEMNAME_MUTEX);
    sem_unlink(SEMNAME_FULL);
    sem_unlink(SEMNAME_EMPTY);

    /* create and initialize the semaphores */
    sem_mutex = sem_open(SEMNAME_MUTEX, O_RDWR | O_CREAT, 0660, 1);
    if (sem_mutex < 0) {
        if (DEBUG)
            perror("cannot create semaphore\n");
        return NULL;
    }
    if (DEBUG) {
        printf("sem %s created\n", SEMNAME_MUTEX);
    }

    sem_full = sem_open(SEMNAME_FULL, O_RDWR | O_CREAT, 0660, 1);
    if (sem_full < 0) {
        if (DEBUG)
            perror("cannot create semaphore\n");
        return NULL;
    }
    if (DEBUG) {
        printf("sem %s created\n", SEMNAME_FULL);
    }

    sem_empty = sem_open(SEMNAME_EMPTY, O_RDWR | O_CREAT, 0660, 1);
    if (sem_empty < 0) {
        if (DEBUG)
            perror("cannot create semaphore\n");
        return NULL;
    }
    if (DEBUG) {
        printf("sem %s created\n", SEMNAME_EMPTY);
    }

    // allocate space as big as reqsize
    if (alloc_size <= 0) {
        
        allocated_segments_head->segment_pointer = holes_head->segment_pointer;
        allocated_segments_head->segment_size = reqsize;
        allocated_segments_head->next = NULL;
        if (allocated_segments_head->segment_size == 0) {
            alloc_size = 0;
            if (DEBUG) 
                perror("could not allocate segment\n");
            return NULL;
        } else
            alloc_size++;

        if (holes_head->segment_size <= reqsize) {
            holes_head->segment_pointer = holes_head->segment_pointer + reqsize;
            holes_head->segment_size = holes_head->segment_size - reqsize;
            holes_head->next = NULL;
        } else {
            if (DEBUG)
                printf("cannot allocate size, not enough space\n");
            return NULL;
        }
        
        if (holes_head->segment_size == 0)
            holes_size = 0;
        
    } else if (holes_size <= 0) {
        if (DEBUG) 
            printf("cannot allocate segment; not enough space\n");
        return NULL;
    } else {
        // there are both allocated segments and holes avaliable.
        // apply first fit AND worst fit here
        if (FIRST_FIT) {
            //apply first fit
            if (holes_head->segment_size >= reqsize) {
                //add
            } else {
                holes_cur = holes_head;
                while ((holes_cur->segment_size < reqsize) && (holes_cur != NULL)) {
                    // idk what to do here bro
                }
            }
        } else if (WORST_FIT) {
            //apply worst fit
        }
    }


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

