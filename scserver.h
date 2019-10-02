#ifndef SCSERVER_H
#define SCSERVER_H

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define BUFF_SIZE 256
#define DEFAULT_DICTIONARY "dictionary.txt"
#define DEFAULT_LOG_FILE "logFile.txt"
#define DEFAULT_PORT 8010
#define DICTIONARY_LENGTH 99171 /* how many lines are in provided dictionary */
#define EXIT_NUM -1
#define MAX_SIZE 1000
#define NUM_WORKERS 5

/* defining the double pointer for the dictionary of words */
char **dictionary_words;


/****** pThread Locks and Condition variables ******/
/* lock and release the threads */
pthread_mutex_t job_queue_lock;
pthread_mutex_t log_queue_lock;
pthread_mutex_t log_lock;

/* signals where the thread is locked/unlocked 
  prevents deadlocks with the threads */
pthread_cond_t condition1;
pthread_cond_t condition2;
pthread_cond_t condition3;
pthread_cond_t condition4;


/****** Structs, Nodes, Queues *******/
/* Structs for the queues to create for job clients and logs */
struct Queue *job_queue;
struct Queue *log_queue;

/* typedef to streamline the fifo queues for the clients */
typedef struct Node {
    struct sockaddr_in client;
    int client_socket;
    char *word;
    struct Node *next;
} Node;

/* typedef for the node's queue */
typedef struct Queue {
    Node *front;
    int queue_size;
} Queue;


/********* Function Declartion ***********/
/* These define the FIFO queues and the creating nodes, the pushing, and 
  pulling of said queues.(got these from Lab01) */
Queue *createQueue();
Node *createNode(struct sockaddr_in, char *, int);
void push(Queue *, struct sockaddr_in, char *, int);
Node *pop(Queue *);

/* read the dictionary function */
char **open_dictionary(char *);

/* file descriptor for opening the listener (we got from class) */
int open_listenfd(int);

/* both the worker thread function that prompts the user for their input,
  and checks the word against the dictionary. The log thread which is used
  to log all the activity from the clients requests to the server */
void *worker_thread(void *);
void *logger_thread(void *);

#endif