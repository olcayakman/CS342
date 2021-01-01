#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>

#define DEBUG -1

#define MAX_WORD_SIZE 1024 //the maximum length of a word in characters,
                           //including the terminating NULL character
#define MAX_FILE_NAME 128
#define MAX_NO_OF_FILES 5 //since max no of child processes is 5
#define END_OF_WORDS "end-of-words-in-file"

struct node {
    char *word;
    int count;
    struct node *next;
};


//global variables
struct node *listheads[MAX_NO_OF_FILES];
char inputfiles[MAX_NO_OF_FILES][MAX_FILE_NAME];
pthread_t word_sorters[MAX_NO_OF_FILES];
char outputfile[MAX_FILE_NAME];

/**
 * printer func
 */
void print_sorted_list(struct node* el) {
    printf("sorted list:\n");
    struct node *p = el;
    while (p) {
        printf("%s\n", p->word);
        p = p->next;
    }
    printf("\n");
}

/**
 * adds to specified  sorted list the given word, uses 
 * insertion sort.
 */
void add_to_list(struct node** list, char word[MAX_WORD_SIZE]) {
    struct node *new_node;
    struct node *tmp;

    //allocating memory space for the new node to be inserted

    if ( (*list) == NULL) {
        new_node = (struct node *) malloc(sizeof(struct node));
        if (new_node == NULL) {
            perror("malloc error\n");
            exit(1);
        }
        new_node->word = strdup(word);
        new_node->count = 1;
        new_node->next = NULL;

        *list = new_node;
        return;
    } else if ( strcmp(word, (*list)->word) < 0 ) {
        new_node = (struct node *) malloc(sizeof(struct node));
        if (new_node == NULL) {
            perror("malloc error\n");
            exit(1);
        }
        new_node->word = strdup(word);
        new_node->count = 1;
        new_node->next = NULL;

        new_node->next = *list;
        *list = new_node;

        return;
    } else {
        tmp = *list;

        while (tmp->next) {
            
            if ( strcmp(word, (tmp->next)->word) < 0)
                break;
            else if ( strcmp(word, (tmp->next)->word) == 0 ) {
                tmp->next->count++;
                return;
            }
            else 
                tmp = tmp->next;
        }

        if (tmp->next == NULL) {
            new_node = (struct node *) malloc(sizeof(struct node));
            if (new_node == NULL) {
                perror("malloc error\n");
                exit(1);
            }
        new_node->word = strdup(word);
            new_node->count = 1;
            new_node->next = NULL;

            tmp->next = new_node;
            
            return;
        } else {
            new_node = (struct node *) malloc(sizeof(struct node));
            if (new_node == NULL) {
                perror("malloc error\n");
                exit(1);
            }
            new_node->word = strdup(word);
            new_node->count = 1;
            new_node->next = NULL;

            new_node->next = tmp->next;
            tmp->next = new_node;
            
            return;
        }
    }

}

void *word_sorter(void *arg) {
    FILE *fp;
    int thread_index;
    char word[MAX_WORD_SIZE];

    thread_index = (int) arg;

    struct node *list = NULL;

    fp = fopen(inputfiles[thread_index], "r");
    if (fp == NULL) {
        printf("fopen failed\n");
        exit(1);
    }

    while (fscanf(fp, "%s", word) && !feof(fp)) 
        add_to_list(&list, word);

    if (DEBUG) 
        print_sorted_list(list);
    
    listheads[thread_index] = list;

    fclose(fp);

    if (DEBUG)
        printf("thread %d exiting\n", thread_index);

    pthread_exit(0);

}

void *merger(void *arg) {
    FILE *fp;
    char firsts[MAX_NO_OF_FILES][MAX_WORD_SIZE];
    char *min;
    int min_count;
    int q;
    int no_of_files;
    int no_of_fin_qs;
    struct node *tmp;
    int frequencies[MAX_NO_OF_FILES];
    int i;
    char last[MAX_WORD_SIZE];
    int last_freq;


    no_of_fin_qs = 0;

    no_of_files = (int) arg;

    for(i = 0; i < no_of_files; i++) {
        pthread_join(word_sorters[i], NULL);
    }

    if (DEBUG) 
        printf("merger: all sorters terminated\n");

    fp = fopen(outputfile, "w");
    if (fp == NULL) {
        perror("fopen failed\n");
        exit(1);
    }
    no_of_fin_qs = 0;

    //read one word from each message queue
    for (i = 0; i < no_of_files; i++) {
        if (listheads[i] == NULL) {
            strcpy(firsts[i], END_OF_WORDS);
            frequencies[i] = 0; //----------------------------
            no_of_fin_qs++;
        } else {
            strcpy(firsts[i], listheads[i]->word);
            frequencies[i] = listheads[i]->count;
            tmp = listheads[i];
            listheads[i] = listheads[i]->next;
            free(tmp);
        }
    }

    while(no_of_fin_qs < no_of_files) {
        q = -1;
        for (i = 0; i < no_of_files; i++) {
            if ( strcmp(firsts[i], END_OF_WORDS) != 0 ) {
                if (q == -1) {
                    q = i;
                    min_count = frequencies[i];
                }
                if (strcmp(firsts[i], firsts[q]) < 0) {
                    min_count = frequencies[i];
                    q = i;
                }
            }
        }

        if (strcmp(last, firsts[q]) == 0) {
            last_freq += frequencies[q];
        } else {
            if (last_freq > 0)
                fprintf(fp, "%s - %d\n", last, last_freq);
            strcpy(last, firsts[q]);
            last_freq = frequencies[q];
        }

        //fprintf(fp, "%s - %d\n", last, last_freq);

        //continue with min
        if (listheads[q] == NULL) {
            strcpy(firsts[q], END_OF_WORDS);
            no_of_fin_qs++;
        } else {
            strcpy(firsts[q], listheads[q]->word);
            frequencies[q] = listheads[q]->count;
            tmp = listheads[q];
            listheads[q] = listheads[q]->next;
            free(tmp);
        }

    }

    fprintf(fp, "%s - %d\n", last, last_freq); //-----------------

    fclose(fp);

    pthread_exit(0);

}

int main(int argc, char** argv) {

    struct timeval start,end;

    gettimeofday(&start, NULL);

    int i;
    pthread_t merger_tid;
    int no_of_files;

    no_of_files = atoi(argv[2]);
    for (i = 0; i < no_of_files; i++) {
        strcpy(inputfiles[i], argv[3+i]);
    }

    strcpy(outputfile, argv[3+no_of_files+1]);

    for (i = 0; i < no_of_files; i++) {
        listheads[i] = NULL;
    }

    int ret = pthread_create(&merger_tid, NULL, merger, (void *) no_of_files);

    if (ret != 0) {
        perror("thread create failed\n");
        exit(1);
    }

    for (i = 0; i < no_of_files; i++) {
        int ret = pthread_create(&(word_sorters[i]), NULL, word_sorter, (void *) i);

        if (ret != 0) {
            perror("thread create failed\n");
            exit(1);
        }
    }

    pthread_join(merger_tid, NULL);

    gettimeofday(&end, NULL);

    printf("%ld\n", ((end.tv_usec) - (start.tv_usec)));

    printf("bye!...\n");


    

    //end main
    return 0;
}
