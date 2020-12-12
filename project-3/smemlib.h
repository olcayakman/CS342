#define DEBUG 1
#define FIRST_FIT 1
#define WORST_FIT 0

int smem_init(int segmentsize); 
int smem_remove(); 

int smem_open(); 
void *smem_alloc (int size);
void smem_free(void *p);
int smem_close(); 

struct segment {
    int segment_size;
    int segment_pointer;
    struct segment* next;
};