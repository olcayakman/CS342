
#include <unistd.h>
#include <stdlib.h>
#include <semaphore.h>
#include <stdio.h>

#include "smemlib.h"

int main()
{
    smem_init(4000000);

    int i, ret; 
    char *p;  	

    ret = smem_open(); 
    if (ret == -1)
	exit (1); 
    
    p = smem_alloc (1024); // allocate space to hold 1024 characters
    if (DEBUG)
    printf("smem allcoated back to app ");
    if (DEBUG)
    printf("at adress %p \n", p);
    for (i = 0; i < 1024; ++i) {
	    p[i] = 'a'; // init all chars to ‘a’
    }
    if (DEBUG)
    printf("memorye biseyler yerlestirildi wow\n");    
    smem_free (p);
    if (DEBUG)
    printf("back to mainn, memory freed \n");

    smem_close(); 

    smem_remove();
    
    return (0); 
}