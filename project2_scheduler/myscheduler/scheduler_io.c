//###############################################################
// 236064 1041756 Kaminaris Konstantinos
//###############################################################
#include <bits/types/siginfo_t.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <wait.h>
#include <time.h>
#include <limits.h>

#define NAMESIZE 250
#define N_PROCESSES 50
#define NEW 1
#define RUNNING 2
#define STOPPED 3
#define EXITED 4
#define BLOCKED 5

// A linked list node to store a process
struct processNode {
    char name[NAMESIZE];
    struct processNode* next;
    struct processNode* prev;
    pid_t pid;
    int status;
    struct timespec start_time, end_time;
    double elapsed_time;
};

// The queue, head is the first and tail is the last element
struct queue {
    struct processNode *head, *tail;
};

struct queue q = {
    NULL,
    NULL
};

// Create a new linked list node
struct processNode* newProcess(char *name){
    struct processNode* temp 
        = (struct processNode*)malloc(sizeof(struct processNode));
    strcpy(temp->name, name);
    temp->next = NULL;
    temp->status = NEW;
    temp->prev = NULL;
    clock_gettime(CLOCK_MONOTONIC, &temp->start_time);
    return temp;
}

// Initialize the queue
struct queue* createQueue() {
    struct queue* q 
        = (struct queue*)malloc(sizeof(struct queue*));
    q->head = NULL;
    q->tail = NULL;
    return q;
};

// Enqueue a process
void enqueue(struct queue* q, char *name){

    // Create new process node
    struct processNode* temp = newProcess(name);

    // If queue is empty initialize head and tail
    if (q->tail == NULL) {
        q->head = temp;
        q->tail = temp;
        return;
    }

    // Add new node at the end of the queue
    q->tail->next = temp;
    temp->prev = q->tail;
    q->tail = temp;
    return;
}

// print info
void print_info(struct processNode* k) {
    //printf("Dequeue process with name %s\n", k->name);
    clock_gettime(CLOCK_MONOTONIC, &k->end_time);
    int secs = k->end_time.tv_sec-k->start_time.tv_sec;
    int ms = (k->end_time.tv_nsec-k->start_time.tv_nsec)/1000;
    double res = (double)(secs + (double)ms/1000000);
    k->elapsed_time = res;
    printf("PID: [%d] - CMD: %s\n\tElapsed time: %.2f secs\n", 
           k->pid, k->name, k->elapsed_time);
}

// Dequeue a process
void remove_from_queue(struct queue* q, pid_t pid){

    // if queue is empty return null
    if (q->head == NULL)
        return;

    // if there is only one element, empty all the things
    if (q->head == q->tail) {
        print_info(q->head);
        q->head->next = q->head->prev = NULL;
        // TODO////////////////////////////////CHANGE TO WORKLOAD TIME
        printf("WORKLOAD TIME: %.2f secs\n", q->head->elapsed_time);
        free(q->head);
        q->head = q->tail = NULL;
        return;
    }

    struct processNode* temp = q->head;

    while (temp->pid != pid && temp != q->tail) {
        temp = temp->next;
    }

    // check if we found the pid
    if (temp->pid != pid)
        return;

    // remove from queue
    print_info(temp);
    temp->prev->next = temp->next;
    temp->next->prev = temp->prev;

    // if item was head or tail, update
    if (temp == q->head)
        q->head = temp->next;
    if (temp == q->tail)
        q->tail = temp->prev;

    // Free memory
    free(temp);

    return;
}

void execute(struct processNode* process, struct timespec request,
             struct timespec remaining) {

    pid_t pid; 

    // process is new
    if (process->status == NEW) {
        pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        else if (pid == 0) {
            execlp(process->name, process->name, NULL);
            exit(EXIT_SUCCESS);
        }
        else {
            printf("executing %s\n", process->name);
            process->pid = pid;
            process->status = RUNNING;
            nanosleep(&request, &remaining);
        }
    }
    else if (process->status == BLOCKED) {
    }
    // process was stopped
    else if (process->status == STOPPED) {
        printf("executing %s\n", process->name);
        kill(process->pid, SIGCONT);
        process->status = RUNNING;
        nanosleep(&request, &remaining);
    }
}

void rr_scheduler(struct queue* q, unsigned long long quantum) {

    struct processNode* k = q->head;
    struct timespec remaining, request;
    request.tv_sec = quantum/1000;
    quantum = quantum%1000;
    request.tv_nsec = quantum * 1000000;

    while (k != NULL) {
        execute(k, request, remaining);
        if (k->status == RUNNING) {
            kill(k->pid, SIGSTOP);
            k->status = STOPPED;
        }
        k = k->next;
    }; 

    printf("scheduler exits\n");

}

void make_blocked(struct queue* q, pid_t pid) {

    struct processNode* k = q->head;

    while (k != q->tail && k->pid != pid) {
        k = k->next;
    }

    if (k->pid == pid) {
        k->status = BLOCKED;
    }

    return;
}

void make_unblocked(struct queue* q, pid_t pid) {

    struct processNode* k = q->head;

    while (k != q->tail && k->pid != pid) {
        k = k->next;
    }

    if (k->pid == pid) {
        k->status = STOPPED;
    }

    return;
}

void sigchld_handler(int signo, siginfo_t *si, void *unused) {
    remove_from_queue(&q, si->si_pid);
}

void sigusr1_handler(int signo, siginfo_t *si, void *unused) {
    make_blocked(&q, si->si_pid);
}

void sigusr2_handler(int signo, siginfo_t *si, void *unused) {
    make_unblocked(&q, si->si_pid);
}

int main(int argc, char **argv) {

    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t readline;
    int i, lines = 0;
    char ch;

    struct sigaction sachld;                                        
    sigemptyset( &sachld.sa_mask );                                 
    sachld.sa_flags = SA_SIGINFO | SA_RESTART | SA_NOCLDSTOP;
    sachld.sa_sigaction = sigchld_handler;
    if (sigaction(SIGCHLD, &sachld, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    struct sigaction sausr1;                                        
    sigemptyset( &sausr1.sa_mask );                                 
    sausr1.sa_flags = SA_SIGINFO | SA_RESTART;
    sausr1.sa_sigaction = sigusr1_handler;
    if (sigaction(SIGUSR1, &sausr1, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    struct sigaction sausr2;                                        
    sigemptyset( &sausr2.sa_mask );                                 
    sausr2.sa_flags = SA_SIGINFO | SA_RESTART;
    sausr2.sa_sigaction = sigusr2_handler;
    if (sigaction(SIGUSR2, &sausr2, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    // Check input
    if (argc < 2)
        exit(EXIT_FAILURE);

    // handle input
    fp = fopen(argv[argc-1], "r");
    if (fp == NULL)
        exit(EXIT_FAILURE);

    // count number of lines in FILE
    while(!feof(fp))
    {
        ch = fgetc(fp);
        if(ch == '\n')
        {
            lines++;
        }
    }

    // reset pointer to start of FILE
    rewind(fp);

    // Initiate queue

    i = -1;
    while ((readline = getline(&line, &len, fp)) != -1) {
        i++;
        //remove trailing newline
        if (line[strlen(line) - 1] == '\n')
            line[strlen(line) - 1] = '\0';
        enqueue(&q, line);
    }
    // queue is a loop
    q.tail->next = q.head;
    q.head->prev = q.tail;

    for (int i=0; i<lines; i++){
        wait(NULL);
    }
    //free(line);
    fclose(fp);

    if (argc == 3)
        rr_scheduler(&q, ULLONG_MAX);
    else
        rr_scheduler(&q, strtoull(argv[2], NULL, 10));

    return 0;
}
