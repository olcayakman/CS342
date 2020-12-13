

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>

#include "smemlib.h"

#define SEG_SIZE 32000000

int main()
{
    
    smem_init(SEG_SIZE); 

    if (DEBUG)
        printf ("memory segment is removed from the system. System is clean now.\n");
    if (DEBUG)
        printf ("you can no longer run processes to allocate memory using the library\n"); 

    return (0); 
}