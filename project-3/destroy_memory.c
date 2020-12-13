#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>

#include "smemlib.h"

int main()
{

    smem_remove(); 

    if (DEBUG)
        printf ("memory segment is removed from the system. System is clean now.\n");
    if (DEBUG)
        printf ("you can no longer run processes to allocate memory using the library\n"); 

    return (0); 
}