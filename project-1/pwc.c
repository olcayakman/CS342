#include <stdlib.h>
#include <mqueue.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>

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

struct msg {
    char data[MAX_WORD_SIZE];
    int count;
};

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

/**
 * reads and processes the given input file specified as parameter,
 * and creates a sorted linked list of distinct words in memory (via insertion
 * sort) then adds the linked list items to the given message queue
*/
void word_sorter(char *input_file, mqd_t mq) {
    FILE *fp;
    char word[MAX_WORD_SIZE];
    struct node *word_list;
    struct node *word_list_head;
    struct msg *message;
    word_list = NULL;
    int msg_send_code; 
    int i;

    printf("im in sorter\n");
    
    fp = fopen(input_file, "r");

    i = 0;
    while ( fscanf(fp, "%s", word) && !feof(fp)) {
        add_to_list(&word_list, word);

        i++;
    }
    
    while (word_list != NULL ) {
        message = malloc(sizeof(struct msg));
        strcpy(message->data, word_list->word);
        message->count = word_list->count;
        msg_send_code = mq_send(mq, (char *) message, sizeof(struct msg), 0);
        // printf("this is sent to mq: %s %d\n", message->data, message->count);
        if (msg_send_code == -1) {
            printf("mq_send error.\n");
            exit(1);
        }
        word_list = word_list->next;
    }

    strcpy(message->data, END_OF_WORDS);
    msg_send_code = mq_send(mq, (char *) message, sizeof(struct msg), 0);
    if (msg_send_code == -1) {
        printf("mq_send error.\n");
        exit(1);
    }
    fclose(fp);

}

int main(int argc, char **argv) {

    struct timeval start, end;

    gettimeofday(&start, NULL);

    //properties
    int i;
    int no_of_files;
    pid_t pid;
    mqd_t mq;
    char input_files[MAX_NO_OF_FILES][MAX_FILE_NAME];
    char output_file[MAX_FILE_NAME];
    char mq_names[MAX_NO_OF_FILES][MAX_FILE_NAME];
    mqd_t mq_ids[MAX_NO_OF_FILES];
    pid_t word_sorter_pids[MAX_NO_OF_FILES];
    FILE *fp;
    struct mq_attr mq_attr;
    struct msg *messageptr = malloc(sizeof (struct msg));
    char *bufptr;
    int buf_length;
    char firsts_of_mqs[MAX_NO_OF_FILES][MAX_WORD_SIZE];
    int frequencies[MAX_NO_OF_FILES];
    char *min;
    int min_count;
    int q;
    int msg_receive_code;
    int no_of_fin_qs; //no of queues finished
    char tmp_str[MAX_WORD_SIZE];

    no_of_files = atoi(argv[2]);

    for (i = 0; i < no_of_files; i++) {
        strcpy(input_files[i], argv[3+i]);  
    }
    strcpy(output_file, argv[3+no_of_files+1]);

    mq_attr.mq_msgsize = sizeof(struct msg);
    mq_attr.mq_flags = 0;
    mq_attr.mq_maxmsg = 10;
    
    //create msg queues
    for (i = 0; i < no_of_files; i++) {
        sprintf(mq_names[i], "%s%d\n", "/msgq", i);
        mq_unlink(mq_names[i]);
        mq = mq_open(mq_names[i], O_RDWR | O_CREAT, 0666, &mq_attr);
        if (mq == -1) {
            perror("mq_open error.\n");
            exit(1);
        }
        mq_ids[i] = mq;
    }

    //create child processes
    for (i = 0; i < no_of_files; i++) {
        pid = fork();
        if (pid == 0) {
            word_sorter(input_files[i], mq_ids[i]);
            exit(0);
        }
        word_sorter_pids[i] = pid;
    }


    /**
     * receive the incmoning words and their counts from children
     * as messages, in sorted order, and write them in the output file. 
     */
    fp = fopen(output_file, "w");

    mq_getattr(mq_ids[0], &mq_attr);
    buf_length = mq_attr.mq_msgsize;
    bufptr = (char *) malloc(buf_length);

    no_of_fin_qs = 0;

    for (i = 0; i < no_of_files; i++) {
        msg_receive_code = mq_receive(mq_ids[i], (char *) messageptr, buf_length, NULL);
        if (msg_receive_code == -1) {
            perror("mq_receive error.\n");
            exit(1);
        }
        printf("this is received: %s\n", messageptr->data);
        if ( strcmp(messageptr->data, END_OF_WORDS) == 0 ) {
            strcpy(firsts_of_mqs[i], END_OF_WORDS);
            no_of_fin_qs++;
        } else {
            strcpy(firsts_of_mqs[i], messageptr->data);
            frequencies[i] = messageptr->count;
        }
    }

    char last[MAX_WORD_SIZE];
    int last_freq;

    while (no_of_fin_qs < no_of_files) {
        q = -1;
        for (i = 0; i < no_of_files; i++) {
            if ( strcmp(firsts_of_mqs[i], END_OF_WORDS) != 0 ) {
                if (q == -1) {
                    q = i;
                    min_count = frequencies[i];
                }
                if (strcmp(firsts_of_mqs[i], firsts_of_mqs[q]) < 0) {
                    min_count = frequencies[i];
                    q = i;
                }
            }
        }

        if (strcmp(last, firsts_of_mqs[q]) == 0) {
            last_freq += frequencies[q];
        } else {
            if (last_freq > 0)
                fprintf(fp, "%s - %d\n", last, last_freq);
            strcpy(last, firsts_of_mqs[q]);
            last_freq = frequencies[q];
        }
        
        msg_receive_code = mq_receive(mq_ids[q], (char *) messageptr, buf_length, NULL);
        printf("this is received: %s\n", messageptr->data);

        if (msg_receive_code == -1 ) {
            perror("mq_receive error\n");
            exit(1);
        }

        if ( strcmp(messageptr->data, END_OF_WORDS) == 0 ) {
            strcpy(firsts_of_mqs[q], END_OF_WORDS);
            no_of_fin_qs++;
        } else {
            strcpy(firsts_of_mqs[q], messageptr->data);
            frequencies[q] = messageptr->count;
        }
    }

    fprintf(fp, "%s - %d\n", last, last_freq);

    free(bufptr);
    fclose(fp);

    //release the memory from message queues - garbage collection!
    for (i = 0; i < no_of_files; i++) {
        mq_close(mq_ids[i]);
    }

    printf("bye!...\n");


    gettimeofday(&end, NULL);

    printf("%ld\n", ((end.tv_usec) - (start.tv_usec)));

    //end the main function
    return 0;
}
