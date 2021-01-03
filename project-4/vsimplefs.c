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

//hello

/**
 * Important notice. A process can open at most 16 files simultaneously. 
 * Hence the library will have an open file table with 16 entries. 
 */

// Global Variables =======================================
int vdisk_fd; // Global virtual disk file descriptor. Global within the library.
              // Will be assigned with the vsfs_mount call.
              // Any function in this file can use this.
              // Applications will not use  this directly.
char *fileNames[16];
off_t fileSizes[16];
ino_t fileStartBlocks[16];
FILE *fds[16];

char *openFiles[16];

int fileCount = 0;
int openFileCount = 0;
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
    //check if file with given name exists
    if (access(filename, F_OK) == 0)
    {
        // file exists
        if (DEBUG)
            printf("Cannot create file. File with name %s already exists.\n", filename);
        return -1;
    }
    else
    {
        char command[1000];
        struct stat *stats;
        // stats = malloc(sizeof(struct stat));
        // file doesn't exist
        // create the file. return 0;
        sprintf(command, "touch %s", filename);
        if (DEBUG)
        {
            printf("File successfully created with name %s\n", filename);
        }
        char path[1000] = "/dev/zero/";
        strcat(path, filename);
        stat(path, &stats);
        // int filesize = stats->st_size;
        // printf("filesize: %d\n", filesize);

        //initialize info table about file
        fileNames[fileCount % 16] = filename;
        fileSizes[fileCount % 16] = stats->st_size;
        fileStartBlocks[fileCount % 16] = stats->st_ino;

        printf("filecount: %d\n", fileCount);
        fileCount++;
        return 0;
    }
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
    if (MODE_APPEND)
    {
        // append-only mode...
        if (DEBUG)
            printf("Opening file %s in Append-Only mode.\n", filename);
        FILE *fp = fopen(filename, "a");
        openFiles[openFileCount % 16] = filename;
        fds[openFileCount % 16] = fp;
        return openFileCount++;
    }
    else if (MODE_READ)
    {
        // read-only mode...
        if (DEBUG)
            printf("Opening file %s in Read-Only mode.\n", filename);
        FILE *fp = fopen(filename, "r");
        openFiles[openFileCount % 16] = filename;
        fds[openFileCount % 16] = fp;
        return openFileCount++;
    }
    else
    {
        // error sitch. should be either read or append mode.
        // return -1.
        if (DEBUG)
            printf("Could not open specified file. Given mode is incorrect.\n");
        return -1;
    }
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
