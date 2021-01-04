#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include "vsimplefs.h"

#define DEBUG 1

#define BLOCKSIZE 4096
#define ADDRSIZE 256
#define NUM_ADDR_PER_BLOCK (BLOCKSIZE/ADDRSIZE)

#define MAX_DIR_ENTRY 256
#define MAX_FILENAME 64
#define MAX_PATHNAME 1024
#define MAX_DIRS 16

#define SUPER_BLOCK 0
#define SUPER_BLOCK_OFFSET 4096 

#define ROOT (SUPER_BLOCK + SUPER_BLOCK_OFFSET)
#define ROOT_OFFSET (8 * BLOCKSIZE)

#define DIRECTORY_ENTRY_OFFSET 256

#define FAT (ROOT + ROOT_OFFSET)
#define FAT_BLOCKS 256
#define FAT_OFFSET 8
#define MAX_NO_OF_FAT_ENTRIES 112
#define MAX_FILE_NO 112
#define FAT_ENTRY_NO_PER_BLOCK 512

/**
 * Important notice. A process can open at most 16 files simultaneously. 
 * Hence the library will have an open file table with 16 entries. 
 */

// Global Variables =======================================
int vdisk_fd; // Global virtual disk file descriptor. Global within the library.
              // Will be assigned with the vsfs_mount call.
              // Any function in this file can use this.
              // Applications will not use  this directly.

struct root_buf {
    char* filename;
    int size;
    int first_fat_entry;
};

struct fat_buf {
    int memory_block;
    // struct fat_buf *next_fat;
    int next_fat;
};

struct superblock_buf {
    int *directory_info[112];
    int file_number;
    //int *empty_fat[FAT_BLOCKS * FAT_ENTRY_NO_PER_BLOCK];
};

char *fileNames[112];
off_t fileSizes[112];
ino_t fileStartBlocks[112];
FILE *fds[112];


char *openFiles[MAX_DIRS];

int fileCount = 0;
int openFileCount = 0;

int noOfFATEntries = 0;
int *empty_directory[112];
// ========================================================

// read block k from disk (virtual disk) into buffer block.
// size of the block is BLOCKSIZE.
// space for block must be allocated outside of this function.
// block numbers start from 0 in the virtual disk.
int read_block(void *block, int k)
{
    int n;
    int offset;

    offset = k * BLOCKSIZE;
    lseek(vdisk_fd, (off_t)offset, SEEK_SET);
    n = read(vdisk_fd, block, BLOCKSIZE);
    if (n != BLOCKSIZE)
    {
        printf("read error\n");
        return -1;
    }
    return (0);
}

// write block k into the virtual disk.
int write_block(void *block, int k)
{
    int n;
    int offset;

    offset = k * BLOCKSIZE;
    lseek(vdisk_fd, (off_t)offset, SEEK_SET);
    n = write(vdisk_fd, block, BLOCKSIZE);
    if (n != BLOCKSIZE)
    {
        printf("write error\n");
        //if (DEBUG) printf( "k is %d \n",k);
        return (-1);
    }
    return 0;
}

/**********************************************************************
   The following functions are to be called by applications directly. 
***********************************************************************/

/**
 * TODO
 */
// this function is partially implemented.
int create_format_vdisk(char *vdiskname, unsigned int m)
{
    char command[1000];
    int size;
    int num = 1;
    int count;
    size = num << m;
    count = size / BLOCKSIZE;
    //    printf ("%d %d", m, size);
    sprintf(command, "dd if=/dev/zero of=%s bs=%d count=%d",
            vdiskname, BLOCKSIZE, count);
    //printf ("executing command = %s\n", command);
    system(command);

    // now write the code to format the disk below.
    // .. your code...
   
    int error = 0;
    //create superblock and write to it .
    
    printf("\nSize of struct superblock_buf: %d\n\n", ((int) sizeof(struct superblock_buf)));

    if (error == -1) return -1;
    struct superblock_buf *superblock = malloc(sizeof(struct superblock_buf));
     int i = 0;
    for(i=0; i < 112; i++){
        superblock->directory_info[i] = 0;
        superblock->file_number = 0;
    }

    if (DEBUG) printf("hata burda\n");
    error = write_block(superblock, SUPER_BLOCK + SUPER_BLOCK_OFFSET);
    if (DEBUG) printf("hata burda2\n");

    // for(i=0; i < FAT_BLOCKS * FAT_ENTRY_NO_PER_BLOCK; i++){
    //     superblock->empty_fat[i] = 0;
    // }
    int n;
    n = write(vdisk_fd, superblock, BLOCKSIZE);
    if( n != BLOCKSIZE ){
        printf("write error\n");
        return (-1);
    }

    //create root directory, 7 blocks.
    for (i = 0; i < 7; i++) {
        struct root_buf *crb = malloc(DIRECTORY_ENTRY_OFFSET);
        error = write_block(crb, ROOT + (DIRECTORY_ENTRY_OFFSET * i));
        if (error == -1) return -1;
    }

    // create FAT here
    for (i = 0; i < 256; i++) {
        struct fat_buf *cfb = malloc(FAT_OFFSET);
        error = write_block(cfb, FAT + (FAT_OFFSET * i));
        if (error == -1) return -1;
    }


    // struct fat_buf *fatHead;
    // fatHead = malloc(sizeof(struct fat_buf));
    // fatHead->memory_block = 0;
    // fatHead->next_fat = -1;
    // noOfFATEntries = 0;

    //write(vdisk_fd + FAT, fatHead, FAT_OFFSET);

    return (0);
}

// already implemented
int vsfs_mount(char *vdiskname)
{
    // simply open the Linux file vdiskname and in this
    // way make it ready to be used for other operations.
    // vdisk_fd is global; hence other function can use it.
    vdisk_fd = open(vdiskname, O_RDWR);
    return (0);
}

// already implemented
int vsfs_umount()
{
    // fsync(vdisk_fd); // copy everything in memory to disk
    close(vdisk_fd);
    return (0);
}

/**
 * TODO - creates a new file with given name filename. Function will
 * use an entry in the root directory to store information about the 
 * created file, like its name, size, first data block number, etc.
 * If success, return 0. Else, return -1.
 */
int vsfs_create(char *filename)
{
    // check if file exists in file system.
    struct superblock_buf *sb;
    int n = read(vdisk_fd, sb, SUPER_BLOCK_OFFSET);
    if( n != SUPER_BLOCK_OFFSET){
        printf("read error\n");
        return -1;
    }
    
    //find an empty place in directory
    if( sb->file_number == MAX_FILE_NO){
        if(DEBUG){
            printf("There is no place for a new file \n");
        }
        return -1;
    }
    int i;
    for(i = 0; i < MAX_FILE_NO; i++){
        if(sb->directory_info[i] == 0){
            break;
        }
    }

    //find an empty place in FAT
    int j;
    for(j = 0; j < FAT_BLOCKS * FAT_ENTRY_NO_PER_BLOCK; j++ ){
        // if( sb->empty_fat[j] == 0){
        //     sb->empty_fat[j] = 1;
        //     break;
        // }
    }
    sb->directory_info[i] = 1;

    // write filename to root directory
    struct root_buf *rb = malloc(sizeof(struct root_buf));
    rb->filename = filename;
    rb->size = 0;
    rb->first_fat_entry = j; //CHECK THIS!!! NOT correct...
    n = write(vdisk_fd + ROOT, rb, ROOT_OFFSET * i);


    //write to fat:
    struct fat_buf *fb = malloc(sizeof(struct fat_buf));
    fb->memory_block = -1;
    fb->next_fat = -1;
    write(vdisk_fd + FAT, fb, FAT_OFFSET * j);

    return (0);
}

/**
 * TODO - opens a file named filename. mode specifies if the file will be
 * opened in read-only or append-only mode. a file can either be read or 
 * appended to. File cannot be opened to be read and appended at the same
 * time. Index of that entry can be returned as the return value. Return
 * value will be non-negative, acting as a file descriptor, used in later 
 * operations. If error, return -1.
 */
int vsfs_open(char *filename, int mode)
{
    if (mode == MODE_READ) {
        open(filename, O_RDONLY);
        if (DEBUG) printf("File %s opened in read-only mode.\n", filename);
    } else if (mode == MODE_APPEND) {
        open(filename, O_APPEND);
        if (DEBUG) printf("File %s opened in append-only mode.\n", filename);
    }
    


    // if (MODE_APPEND)
    // {
    //     // append-only mode...
    //     if (DEBUG)
    //         printf("Opening file %s in Append-Only mode.\n", filename);
    //     FILE *fp = fopen(filename, "a");
    //     openFiles[openFileCount % 16] = filename;
    //     fds[openFileCount % 16] = fp;
    //     return openFileCount++;
    // }
    // else if (MODE_READ)
    // {
    //     // read-only mode...
    //     if (DEBUG)
    //         printf("Opening file %s in Read-Only mode.\n", filename);
    //     FILE *fp = fopen(filename, "r");
    //     openFiles[openFileCount % 16] = filename;
    //     fds[openFileCount % 16] = fp;
    //     return openFileCount++;
    // }
    // else
    // {
    //     // error sitch. should be either read or append mode.
    //     // return -1.
    //     if (DEBUG)
    //         printf("Could not open specified file. Given mode is incorrect.\n");
    //     return -1;
    // }
    return (0);
}

/**
 * TODO - closes a file with DESCRIPTOR fd. The related open file 
 * table entry should be marked as free.
 */
int vsfs_close(int fd)
{
    if (fd >= 16) {
        if (DEBUG) 
            printf("invalid fd value. cannot close file.\n");
        return -1;
    }
    fclose(fds[fd]);
    char* filename = fileNames[fd];
    fileNames[fd] = NULL;
    fileSizes[fd] = NULL;
    fileStartBlocks[fd] = NULL;
    fds[fd] = NULL;
    if (DEBUG) 
        printf("File %s successfully closed, and deleted from table.\n", filename);

    return (0);
}

/**
 * TODO - gives the size of a file  with descriptor fd.
 * File must be opened beforehand. Return the number of data bytes
 * in file. A file with no data has size 0. If error, return -1.
 */
int vsfs_getsize(int fd)
{
    // if (fd >= 16) {
    //     if (DEBUG)
    //         printf("invalid fd value. cannot get size.\n");
    //     return -1;
    // }
    return fileSizes[fd];
}

/**
 * TODO - with this an application can read data from a file. fd is
 * file descriptor. buf points to a memory area for which space
 * is allocated earlier with malloc (or it can be a static array).
 * n is the amount of data to read. If error, return -1. If success, 
 * return number of bytes successfully read. 
 */
int vsfs_read(int fd, void *buf, int n)
{

    return (0);
}

/**
 * TODO - application can append new data to file with descriptor fd. 
 * buf points to a static array holding the data or a dynamically allocated
 * memory space (malloc) holding the data. n is the size of the data to write 
 * (append) into file. If error, return -1. Else, retunn the number of bytes
 * successfully appended to file. 
 */
int vsfs_append(int fd, void *buf, int n)
{
    if (fd < 0)
    {
        if (DEBUG)
            printf("Cannot append. File invalid.\n");
        return -1;
    }

    printf("inital fd: %d\n", fd);

    int i = 0;
    for (i = 0; i < n; i++)
    {
        char curItem = &buf[i];
        fputc(curItem, fds[fd]);
    }

    if (DEBUG)
        printf("Data successfully appended to file %s\n", fileNames[fd]);
    // if (DEBUG)
    //     printf("The data appended: %s\n", buf);

    return (0);
}

/**
 * TODO - Deletes a file with name filename. If success, return 0. If error, 
 * return -1. 
 */
int vsfs_delete(char *filename)
{
    return (0);
}

// struct fat_buf* find_empty_fat (struct fat_buf* fathead) {
//     if (fathead->memory_block == 0)
//         return fathead;
//     struct fat_buf* tmp = fathead;
//     while (tmp->next_fat != NULL) tmp = tmp->next_fat; 
//     tmp->next_fat = malloc(sizeof(struct fat_buf));
//     return tmp->next_fat;
// }
