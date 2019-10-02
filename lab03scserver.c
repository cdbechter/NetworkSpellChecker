#include "scserver.h"


int main(int argc, char **argv) {
    int port;
    char *dictionary;

    /* this is the way to find out if the user/client has entered a 
        port, dictionary, both, or none. If none, the default port
        and dictionary are used. */
    if(argc == 1) {
        port = DEFAULT_PORT;
        dictionary = DEFAULT_DICTIONARY;
    } else if(argc == 2) {
        port = atoi(argv[1]);
        dictionary = DEFAULT_DICTIONARY;
    } else {
        port = atoi(argv[1]);
        dictionary = argv[2];
    }

    /* this puts the words into a usable dictionary for comparrison */
    dictionary_words = open_dictionary(dictionary);

    /* Initialize the fifo_queues for the Client buffers */
    job_queue = createQueue();
    log_queue = createQueue();

    /* initialize the thread mutex locks */
    pthread_mutex_init(&job_queue_lock, NULL);
    pthread_mutex_init(&log_queue_lock, NULL);
    pthread_mutex_init(&log_lock, NULL);

    /* initialize the thread condition variables */
    pthread_cond_init(&condition1, NULL);
    pthread_cond_init(&condition2, NULL);
    pthread_cond_init(&condition3, NULL);
    pthread_cond_init(&condition4, NULL);

    /* initialize the threads ftom the workers constant */
    pthread_t workers[NUM_WORKERS];
    for(int i = 0; i < NUM_WORKERS; i++) {
        pthread_create(&workers[i], NULL, &worker_thread, NULL);
    }

    /* initialize the thread for the log file */
    pthread_t log_thread;
    pthread_create(&log_thread, NULL, &logger_thread, NULL);

    /* create the socket structs and connection */
    struct sockaddr_in client;
    socklen_t client_size = sizeof(struct sockaddr_in);
    int connection_socket = open_listenfd(port);

    /* simple pass or fail messages for connection */
    char *connection_successful = "Connected to server. Type -1 to exit.\n";
    char *job_buff_full = "Sorry, the job buffer is full. Please try again later.\n";

    /* like the shell, this is a forever loop if the socket connects, and is accept, 
        unless it doesn't connect, then the fail submits a -1. If connection is successful
        then it then creates the the job queue, checks if full, waits when it is full,
        otherwise pushes the client, then unloacks and signals of the unlocks for next client */
    while(1) {
      int client_socket = accept(connection_socket, (struct sockaddr *)&client, &client_size);
      /* if connection fails */
      if(client_socket == -1) {
        printf("Couldn't connect the socket %d.\n", client_socket);
        continue;
      }

      /* checking if the queue for jobs is at max to wait or lock */
      pthread_mutex_lock(&job_queue_lock);
      if(job_queue->queue_size >= MAX_SIZE) {
          send(client_socket, job_buff_full, strlen(job_buff_full), 0);
          pthread_cond_wait(&condition2, &job_queue_lock);
      }

      printf("New client connected! Client ID: %d\n", client_socket);
      send(client_socket, connection_successful, strlen(connection_successful), 0);

      /* pushes onto job queue, then unlocks and signals */
      push(job_queue, client, NULL, client_socket);
      pthread_mutex_unlock(&job_queue_lock);
      pthread_cond_signal(&condition1);
    }
    return 0;
}

/* Returns a char** to all of the words in the dictionary file. This opens the 
    designated file the user puts in or the default, which is dictionary.txt and 
    coopies the list for comparsion.  (simple fgets from lap01)*/
char **open_dictionary(char *filename) {
    FILE *fd;
    char **output = malloc(DICTIONARY_LENGTH *sizeof(char *) + 1);
    char line [BUFF_SIZE];
    int index = 0;

    /* this is a fault if file is entered wrong */
    fd = fopen(filename, "r");
    if(fd == NULL) {
      printf("Couldn't open file dictionary file.\n");
      exit(1);
    }

    while((fgets(line, BUFF_SIZE, fd)) != NULL) {
      output[index] = (char *) malloc(strlen(line) *sizeof(char *) + 1);
      int temp = strlen(line) - 2;
      line[temp] = '\0';
      strcpy(output[index], line);
      index++;
    }
    fclose(fd);
    return output;
}

/* This section was taken from the slides provided and nearly word from 
    word from the book. Essentially, it creates the listener file descriptor, 
    opens the socket descriptor, sets the socketopt. */
int open_listenfd(int port) {
    int listenfd, optval = 1;
    struct sockaddr_in serverAddress;

    /* creates a socket descriptor from connection*/
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
      return -1;
    }

    /* gets rid of the "Address already in use" error */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int)) < 0){
      return -1;
    }

    /* Reset the serverAddress struct, setting all of it's bytes to zero.
        bind() is then called, associating the port number with the socket fd. */
    bzero((char *) &serverAddress, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons((unsigned short)port);
    if (bind(listenfd, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0){
      return -1;
    }

    /* Prepare the socket to allow accept() calls. Twenty is the backlog accoring
        to the book, this is the max num of connection. Only that max will be placed 
        on queue. Accept() then will be called after that max is met. */
    if (listen(listenfd, 20) < 0) {
        return -1;
    }
    return listenfd;
}

/* create and return the fifo queue struct (lab01 learned stuff) */
Queue *createQueue() {
    Queue *temp = (Queue *)malloc(sizeof(Queue));
    temp->front = NULL;
    temp->queue_size = 0;
    return temp;
}

/* makes a Node struct and also returns it which is used in the 
    queue struct, which is also part of the fifo queue
    previously explained */
Node *createNode(struct sockaddr_in client, char *word, int socket) {
    Node *temp = (Node *)malloc(sizeof(Node));
    temp->client = client;
    if(word == NULL) {
      temp->word = word;
    } else {
      temp->word = malloc(sizeof(char *) *strlen(word) + 1);
      if(temp->word == NULL){
        printf("Unable to allocate memory for Node.\n");
        exit(1);
      }
      strcpy(temp->word, word);
    }
    temp->next = NULL;
    temp->client_socket = socket;
    return temp;
}

/* linked list esk push function that pushes the node onto the queue struct 
    which again was used in lab01 */
void push(Queue *queue, struct sockaddr_in client, char *word, int socket) {
    Node *temp = createNode(client, word, socket);

    /* if the node is empty, simply place at the end of the queue */
    if (queue->queue_size == 0) {
      queue->front = temp;
    } else {
        Node *head = queue->front;
        while(head->next != NULL) {
            head = head->next;
        }
        head->next = temp;
      }
    queue->queue_size++;
    return;
}

/* simple pop function, pops the first node off the queue (again from lab01) */
Node *pop(Queue *queue) {
    /* simply returns NULL, should the queue be empty */
    if (queue->front == NULL) {
      queue->queue_size = 0;
      return NULL;
    }

    /* simply moves the next node front as the orignal front node was popped */
    Node *temp = queue->front;
    queue->front = queue->front->next;
    queue->queue_size--;
    free(queue->front); /* gotta free! */
    return temp;
}

/* This is the worker thread that handles the concurrent threads for each client thats
    connected to the server. It creates as many worker threads as specified, all while
    concurrently working with the server to fulfill each clients request. It has a prompt, 
    close, and error message like stated in the book. */
void *worker_thread(void *params) {
    char *prompt_msg = "Spell Checked Word Is? >> ";
    char *error_msg = "Something went wrong, please type your word again!\n";
    char *close_msg = "The connection with server has been closed!\n";

    /* This while loops files through all the user words, consistently checking 
        each word from the database and displaying the correct message. It goes 
        through the Queue, popping and pushing each command as they are typed 
        with the appropriate response or function */
    while (1) {
        pthread_mutex_lock(&job_queue_lock);
        if (job_queue->queue_size <= 0) {
            pthread_cond_wait(&condition1, &job_queue_lock);
        }

        /* this pops first job off the queue, releases the lock, and send the signal
            from the condition that its been released. */
        //pop first job off queue
        Node *job = pop(job_queue);
        pthread_mutex_unlock(&job_queue_lock);
        pthread_cond_signal(&condition2);

        /* gets next clients socket */
        int client_socket = job->client_socket;

        /* this communicates the recieve and send for the client socket which
            will take in the word from the client and check it under the server */
        while (1) {
          char receive_buff[BUFF_SIZE] = "";
          send(client_socket, prompt_msg, strlen(prompt_msg), 0);
          int bytes_returned = recv(client_socket, receive_buff, BUFF_SIZE, 0);

          /* deals with in there is an error in the input from client */
          if (bytes_returned <= -1) {
            send(client_socket, error_msg, strlen(error_msg), 0);
            continue;
          /* deals with quit command */
          } else if (atoi(&receive_buff[0]) == EXIT_NUM) {
            send(client_socket, close_msg, strlen(close_msg), 0);
            close(client_socket);
            break;
          /* the meat and potatoes, checks the clients word to the dictionary */
          } else {
            receive_buff[strlen(receive_buff) - 1] = '\0';
            receive_buff[bytes_returned - 2] = '\0';

            char *result = " INCORRECT\n";
            for (int i = 0; i < DICTIONARY_LENGTH; i++) {
              if (strcmp(receive_buff, dictionary_words[i]) == 0) {
                result = " CORRECT\n";
                break;
              }
            }

            strcat(receive_buff, result);
            printf("%s", receive_buff);
            send(client_socket, receive_buff, strlen(receive_buff), 0);

            struct sockaddr_in client = job->client;

            if(log_queue->queue_size >= MAX_SIZE) {
              pthread_cond_wait(&condition4, &log_queue_lock);
            }
            /* this will add the result to push, release the lock, and send signal
                just like in previous functions to indicate its status */
            push(log_queue, client, receive_buff, client_socket);
            pthread_mutex_unlock(&log_queue_lock);
            pthread_cond_signal(&condition3);
          }
        }
      }
}

/* This is the logger thread indicated by the Prof. This logs from the servers point 
    of view everything every client enters. I will demonstrate 3 clients, but it can be as many
    as the program specifys. It records all the words each user types, and whether they are 
    correct or incorrect, the user see's them, the server records them. */
void *logger_thread(void *params) {
  while(1) {
    /* this will pop from log buffer we previously established */
    pthread_mutex_lock(&log_queue_lock);
    if (log_queue->queue_size <= 0) {
      pthread_cond_wait(&condition3, &log_queue_lock);
    }
    
    Node *node = pop(log_queue);
    char * word = node->word;

    /* This like before, releases the lock and sends signal */
    pthread_mutex_unlock(&log_queue_lock);
    pthread_cond_signal(&condition4);

    /* very simple, do nothing if nothing has been done */
    if (word == NULL) {
      continue;
    }

    /* this will get the log file and write it to the default 
        log file, it locks and writes, the write log file is 
        from lab01 yet again... sorry.. lotta repetation */
    pthread_mutex_lock(&log_lock);
    FILE *log_file = fopen(DEFAULT_LOG_FILE, "a");
    fprintf(log_file, "%s", word);
    fclose(log_file);

    // Release log file lock
    pthread_mutex_unlock(&log_lock);
  }
}
