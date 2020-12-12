

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "smemlib.h"

#define SEG_SIZE 128

int main()
{
    
    smem_init(SEG_SIZE); 

    printf ("\nmemory segment is removed from the system. System is clean now.\n");
    printf ("you can no longer run processes to allocate memory using the library\n"); 

    return (0); 
}