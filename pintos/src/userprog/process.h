#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "lib/user/syscall.h"

typedef struct file_status {
    int fd;
    char *file_name;
    struct file *file;
    struct list_elem elem;
} open_file;

typedef struct child_status {
    pid_t pid;  // child pid
    bool isLoaded;  // true if executable loaded successfully, false otherwise
    int exit_code;  // Child exit code, if dead.
    bool isWaiting;  // whether parent is waiting for this child or not
    struct semaphore sem;
    struct list_elem elem;
} child;

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
child *find_child(struct thread *t, tid_t child_pid);

#endif /* userprog/process.h */
