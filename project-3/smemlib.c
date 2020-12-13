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

// Define semaphore(s)
#define SEMNAME_MUTEX "/name_sem_mutex"
#define SEMNAME_FULL "/name_sem_full"
#define SEMNAME_EMPTY "/name_sem_empty"

// Define your stuctures and variables.
sem_t *sem_mutex; //protects the buffer
sem_t *sem_full;  //counts the number of items
sem_t *sem_empty; //counts the number of empty buffer slots

int shm;
void *shm_start;
struct stat sbuf;

struct segment *allocated_segments_head;
struct segment *allocated_segments_cur;
int alloc_size;

int segment_size = 4000000;

struct segment *sort_by_address(struct segment **head)
{

    struct segment *node = NULL, *temp = NULL;
    void *tempaddress; //temp variable to store node data
    int tempsize;
    node = *head;
    //temp = node;//temp node to hold node data and next link
    while (node != NULL)
    {
        temp = node;
        while (temp->next != NULL) //travel till the second last element
        {
            if (temp->segment_pointer < temp->next->segment_pointer) // compare the data of the nodes
            {
                tempaddress = temp->segment_pointer;
                tempsize = temp->segment_size;

                temp->segment_pointer = temp->next->segment_pointer; // swap the data
                temp->segment_size = temp->next->segment_size;

                temp->next->segment_pointer = tempaddress;
                temp->next->segment_size = tempsize;
            }
            temp = temp->next; // move to the next element
        }
        node = node->next; // move to the next node

        return temp;
    }
    return NULL;
}

struct segment *sort_by_size(struct segment **head)
{

    struct segment *node = NULL, *temp = NULL;
    void *tempaddress; //temp variable to store node data
    int tempsize;
    node = *head;
    //temp = node;//temp node to hold node data and next link
    while (node != NULL)
    {
        temp = node;
        while (temp->next != NULL) //travel till the second last element
        {
            if (temp->segment_size < temp->next->segment_size) // compare the data of the nodes
            {
                tempaddress = temp->segment_pointer;
                tempsize = temp->segment_size;

                temp->segment_pointer = temp->next->segment_pointer; // swap the data
                temp->segment_size = temp->next->segment_size;

                temp->next->segment_pointer = tempaddress;
                temp->next->segment_size = tempsize;
            }
            temp = temp->next; // move to the next element
        }
        node = node->next; // move to the next node
    }
    return temp;
}

/**
 * Create and initialize a shared memory segment.
 * if operation is successful return 0, else
 * return -1
 */
int smem_init(int segmentsize)
{
    if (DEBUG)
        printf("smem init called\n"); // remove all printfs when you are submitting to us.

    segment_size = segmentsize; //holding it in a global variable

    // //check if segmentsize is a power of two
    // if (ceil(log2(segmentsize)) != floor(log2(segmentsize)))
    //     return -1;

    //clean the shared memory with the same name
    shm_unlink(SHM_NAME);
    //create a shared memory segment
    shm = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm < 0)
    {
        if (DEBUG)
            printf("shared memory segment cannot be created \n");
        return -1;
    }
    if (DEBUG)
        printf("shm is created\n");
    //set the size of shm
    ftruncate(shm, segmentsize);
    if (DEBUG)
        printf("shm size = %d\n", segmentsize);

    // initially there is no segment allocated,
    // there is one giant hole, the size of segmentsize.
    allocated_segments_head = NULL;
    alloc_size = 0;

    allocated_segments_cur = allocated_segments_head;

    fstat(shm, &sbuf); /* get info about the shared memory */
    if(DEBUG)
        printf("sbuf size :%ld ", sbuf.st_size);

    shm_start = mmap(NULL, sbuf.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm, 0);
    if (DEBUG)
        printf(" start address %p\n", shm_start);

    return (0);
}

/**
 * Remove the shared memory segment from the system.
 * Return -1 if unsuccessful, 0 if successful.
 */
int smem_remove()
{

    //remove the semophores
    sem_unlink(SEMNAME_MUTEX);
    sem_unlink(SEMNAME_FULL);
    sem_unlink(SEMNAME_EMPTY);

    //remove the shared memory
    shm_unlink(SHM_NAME);

    if (DEBUG)
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
 
    shm_start = mmap(NULL, sbuf.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm, 0);
    close(shm);

    if (shm_start <= 0)
    {
        if (DEBUG)
            perror("cannot map the shared memory\n");

        return -1;
    }
    else if (DEBUG)
    {
        printf("mapping ok, start address = %p\n", shm_start);
    }

    /* first clean up semaphores with same names */
    sem_unlink(SEMNAME_MUTEX);
    sem_unlink(SEMNAME_FULL);
    sem_unlink(SEMNAME_EMPTY);

    /* create and initialize the semaphores */
    sem_mutex = sem_open(SEMNAME_MUTEX, O_RDWR | O_CREAT, 0660, 1);
    if (sem_mutex < 0)
    {
        if (DEBUG)
            perror("cannot create semaphore\n");
        //close the created semaphores
        sem_close(sem_mutex);
        sem_close(sem_full);
        sem_close(sem_empty);
        return -1;
    }
    if (DEBUG)
    {
        printf("sem %s created\n", SEMNAME_MUTEX);
    }

    sem_full = sem_open(SEMNAME_FULL, O_RDWR | O_CREAT, 0660, 0);
    if (sem_full < 0)
    {
        if (DEBUG)
            perror("cannot create semaphore\n");
        //close the created semaphores
        sem_close(sem_mutex);
        sem_close(sem_full);
        sem_close(sem_empty);
        return -1;
    }
    if (DEBUG)
    {
        printf("sem %s created\n", SEMNAME_FULL);
    }

    sem_empty = sem_open(SEMNAME_EMPTY, O_RDWR | O_CREAT, 0660, 10);
    if (sem_empty < 0)
    {
        if (DEBUG)
            perror("cannot create semaphore\n");
        //close the created semaphores
        sem_close(sem_mutex);
        sem_close(sem_full);
        sem_close(sem_empty);
        return -1;
    }
    if (DEBUG)
    {
        printf("sem %s created\n", SEMNAME_EMPTY);
    }
    //close the created semaphores
    sem_close(sem_mutex);
    sem_close(sem_full);
    sem_close(sem_empty);

    return (0);
}

/**
 * Shares memory of size at least reqsize.
 * Returns a pointer to the allocated space.
 * If memory cannot be allocated, returns NULL.
 */
void *smem_alloc(int reqsize)
{

    //open the semaphores which are already initialized
    sem_mutex = sem_open(SEMNAME_MUTEX, O_RDWR);

    if (sem_mutex < 0)
    {
        if (DEBUG)
            printf("cannot open semaphore mutex \n");
        return NULL;
    }
    if (DEBUG)
        printf("semaphore %s is opened \n", SEMNAME_MUTEX);

    sem_empty = sem_open(SEMNAME_EMPTY, O_RDWR);

    if (sem_empty < 0)
    {
        if (DEBUG)
            printf("cannot open %s \n", SEMNAME_EMPTY);
        return NULL;
    }
    if (DEBUG)
        printf("semaphore %s is opened \n", SEMNAME_EMPTY);

    sem_full = sem_open(SEMNAME_FULL, O_RDWR);

    if (sem_full < 0)
    {
        if (DEBUG)
            printf("cannot open %s \n", SEMNAME_FULL);
        return NULL;
    }
    if (DEBUG)
        printf("semaphore %s is opened \n", SEMNAME_FULL);

    /*check if there is empty space for semaphores 
        && if other semaphore is currently allocating space*/
    sem_wait(sem_empty);
    sem_wait(sem_mutex);
    if (DEBUG)
        printf(" Wait: sem mutex and sem empty \n");
    // allocate space as big as reqsize
    if (alloc_size <= 0)
    {
        struct segment *allocated_segments_head = (struct segment *)malloc(sizeof(struct segment));
    
        allocated_segments_head->segment_pointer = shm_start;
        if (DEBUG)
            printf("%p\n", allocated_segments_head->segment_pointer);
        allocated_segments_head->segment_size = reqsize;
        allocated_segments_head->next = NULL;

        // allocated_segments_head = new_alloc;
        if (allocated_segments_head->segment_size == 0)
        {
            alloc_size = 0;
            if (DEBUG)
                perror("could not allocate segment\n");
            sem_post(sem_mutex);
            sem_post(sem_empty);

            sem_close(sem_mutex);
            sem_close(sem_full);
            sem_close(sem_empty);
            return NULL;
        }
        else
            alloc_size++;

        sem_post(sem_mutex);
        sem_post(sem_full);
        if (DEBUG)
            printf("smem allocation is succesfull with requested size %d\n", reqsize);

        sem_close(sem_mutex);
        sem_close(sem_full);
        sem_close(sem_empty);

        if (DEBUG)
        {
            printf("bi tane yerlestirdik\n");
            printf("%p", allocated_segments_head->segment_pointer);
            printf("\n");
        }

        return allocated_segments_head->segment_pointer;
    }
    else
    {
        struct segment *temp = allocated_segments_head;
        //check if head has next
        if (temp->next == NULL)
        {

            if (temp->segment_pointer - shm_start > reqsize)
            {
                struct segment *new_alloc = (struct segment *)malloc(sizeof(struct segment));
                new_alloc->segment_pointer = shm_start;
                new_alloc->segment_size = reqsize;
                new_alloc->next = allocated_segments_head;
                allocated_segments_head = new_alloc;

                alloc_size++;
                sem_post(sem_mutex);
                sem_post(sem_full);
                if (DEBUG)
                    printf("smem allocation is succesfull with requested size %d\n", reqsize);

                sem_close(sem_mutex);
                sem_close(sem_full);
                sem_close(sem_empty);
                if (DEBUG)
                    printf("allocated.\n");
                return new_alloc->segment_pointer;
            }
            else if ((shm_start + segment_size) - (temp->segment_pointer + temp->segment_size) > reqsize)
            {
                struct segment *new_alloc = (struct segment *)malloc(sizeof(struct segment));
                new_alloc->segment_pointer = temp->segment_pointer + temp->segment_size;
                new_alloc->segment_size = reqsize;
                allocated_segments_head->next = new_alloc;

                alloc_size++;
                sem_post(sem_mutex);
                sem_post(sem_full);
                if (DEBUG)
                    printf("smem allocation is succesfull with requested size %d\n", reqsize);

                sem_close(sem_mutex);
                sem_close(sem_full);
                sem_close(sem_empty);
                return new_alloc->segment_pointer;
            }
            if (DEBUG)
                printf("allocation unsuccessful.\n");

            sem_post(sem_mutex);
            sem_post(sem_empty);

            sem_close(sem_mutex);
            sem_close(sem_full);
            sem_close(sem_empty);
            return NULL;
        }
        // there are both allocated segments and holes avaliable.
        // apply first fit AND worst fit here
        if (FIRST_FIT)
        {
            allocated_segments_cur = sort_by_address(&allocated_segments_head);

            while (temp->next)
            {
                if ((temp->next->segment_pointer) - (temp->segment_pointer + temp->segment_size) > reqsize)
                {
                    struct segment *new_alloc = (struct segment *)malloc(sizeof(struct segment));
                    new_alloc->segment_pointer = temp->segment_pointer + temp->segment_size;
                    new_alloc->segment_size = reqsize;

                    struct segment *temp_for_next = temp->next;
                    temp->next = new_alloc;
                    new_alloc->next = temp_for_next;

                    alloc_size++;
                    sem_post(sem_mutex);
                    sem_post(sem_full);
                    if (DEBUG)
                        printf("smem allocation is succesfull with requested size %d\n", reqsize);

                    sem_close(sem_mutex);
                    sem_close(sem_full);
                    sem_close(sem_empty);
                    return new_alloc->segment_pointer;
                }
                temp = temp->next;
            }

            //check if the reqsize fits the remaining space
            if ((shm_start + segment_size) - (temp->segment_pointer + temp->segment_size) > reqsize)
            {
                struct segment *new_alloc = (struct segment *)malloc(sizeof(struct segment));
                new_alloc->segment_pointer = temp->segment_pointer + temp->segment_size;
                new_alloc->segment_size = reqsize;
                temp->next = new_alloc;
                new_alloc->next = NULL;

                alloc_size++;
                sem_post(sem_mutex);
                sem_post(sem_full);
                if (DEBUG)
                    printf("smem allocation is succesfull with requested size %d\n", reqsize);

                sem_close(sem_mutex);
                sem_close(sem_full);
                sem_close(sem_empty);
                return new_alloc->segment_pointer;
            }

            if (DEBUG)
                printf("cannot allocate\n");
            sem_post(sem_mutex);
            sem_post(sem_empty);

            sem_close(sem_mutex);
            sem_close(sem_full);
            sem_close(sem_empty);
            return NULL;
        }
        else if (WORST_FIT)
        {
            // TODO
            //apply worst fit
            allocated_segments_cur = sort_by_address(&allocated_segments_head);

            struct segment *temp = allocated_segments_head;
            //struct segment *max = shm_start;
            void *temp_space;
            long int max_space = allocated_segments_head->segment_pointer - shm_start;

            while (temp->next)
            {

                if ((temp->next->segment_pointer) - (temp->segment_pointer + temp->segment_size) > max_space)
                {
                    if ((temp->next->segment_pointer) - (temp->segment_pointer + temp->segment_size) > reqsize)
                    {
                        temp_space = (temp->segment_pointer + temp->segment_size);
                    }
                }
            }
            if ((shm_start + segment_size) - (temp->segment_pointer + temp->segment_size) > reqsize)
            {
                if ((shm_start + segment_size) - (temp->segment_pointer + temp->segment_size) > max_space)
                {
                    temp_space = (temp->segment_pointer + temp->segment_size);
                }
            }

            struct segment *new_alloc = (struct segment *)malloc(sizeof(struct segment));
            new_alloc->segment_pointer = temp_space;
            new_alloc->segment_size = reqsize;

            struct segment *temp_for_next = temp->next;
            temp->next = new_alloc;
            new_alloc->next = temp_for_next;

            sem_post(sem_mutex);
            sem_post(sem_empty);

            sem_close(sem_mutex);
            sem_close(sem_full);
            sem_close(sem_empty);
            return new_alloc->segment_pointer;
        }
        sem_post(sem_mutex);
        sem_post(sem_empty);

        sem_close(sem_mutex);
        sem_close(sem_full);
        sem_close(sem_empty);
        return (NULL);
    }
}

/**
 * Deallocates the shared memory space pointed to 
 * by pointer ptr. The deallocated memory space will 
 * be part of the free memory in the segment. 
 */
void smem_free(void *p)
{

    if (p == NULL)
    {
        if (DEBUG)
            printf("cannot free the given memory, it is already empty.\n");
        return;
    }
    //open the semaphores which are already initialized
    sem_mutex = sem_open(SEMNAME_MUTEX, O_RDWR);
    if (sem_mutex < 0)
    {
        if (DEBUG)
            printf("cannot open semaphore mutex \n");
        return;
    }
    if (DEBUG)
        printf("semaphore %s is opened \n", SEMNAME_MUTEX);
    sem_empty = sem_open(SEMNAME_EMPTY, O_RDWR);
    if (sem_empty < 0)
    {
        if (DEBUG)
            printf("cannot open %s \n", SEMNAME_EMPTY);
    }
    if (DEBUG)
        printf("semaphore %s is opened \n", SEMNAME_EMPTY);
    sem_full = sem_open(SEMNAME_FULL, O_RDWR);
    if (sem_full < 0)
    {
        if (DEBUG)
            printf("cannot open %s \n", SEMNAME_FULL);
        return;
    }
    if (DEBUG)
        printf("semaphore %s is opened \n", SEMNAME_FULL);

    // find p in alloc segments list

    struct segment *alloc_tmp_prev = NULL;
    struct segment *alloc_tmp = allocated_segments_head;
    struct segment *found = NULL;
    while (alloc_tmp)
    {
        if (alloc_tmp->segment_pointer == p)
        {
            found = alloc_tmp;
            break;
        }
        alloc_tmp_prev = alloc_tmp;
        alloc_tmp = alloc_tmp->next;
    }

    if (found == NULL)
    {
        if (DEBUG)
            printf("there is no such allocated space \n");
        return;
    }

    sem_wait(sem_full);
    sem_wait(sem_mutex);
    // if there is no alloc prev
    if (alloc_tmp_prev == NULL)
    {
        allocated_segments_head = NULL;
        allocated_segments_head = alloc_tmp->next;

        sem_post(sem_mutex);
        sem_post(sem_empty);

        sem_close(sem_mutex);
        sem_close(sem_full);
        sem_close(sem_empty);
        return;
    }

    // if there is no next alloc
    if (alloc_tmp->next == NULL)
    {
        alloc_tmp_prev->next = NULL;
        alloc_tmp = NULL;

        sem_post(sem_mutex);
        sem_post(sem_empty);

        sem_close(sem_mutex);
        sem_close(sem_full);
        sem_close(sem_empty);
        return;
    }

    // there is a prev and latter to p
    if ((alloc_tmp_prev != NULL) && (alloc_tmp->next != NULL))
    {
        alloc_tmp_prev->next = alloc_tmp->next;
        alloc_tmp = NULL;

        sem_post(sem_mutex);
        sem_post(sem_empty);

        sem_close(sem_mutex);
        sem_close(sem_full);
        sem_close(sem_empty);
        return;
    }

    if (DEBUG)
        printf("uh... there's a problem. couldnt remove???\n");

    sem_post(sem_mutex);
    sem_post(sem_full);

    sem_close(sem_mutex);
    sem_close(sem_full);
    sem_close(sem_empty);
    return;
}

/**
 * When this is called by a process, the lib will 
 * know this process wont use it anymore.
 * The shared segment is unmapped from the virtual adress
 * space of the process calling this function.
 */
int smem_close()
{
    munmap(shm_start, segment_size);

    if (DEBUG)
        printf("address %d successfully unmapped from the shared memory\n", shm);

    return (0);
}
