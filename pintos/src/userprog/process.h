#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "lib/user/syscall.h"

typedef struct file_status {
    int fd;
    char *file_name;
    struct file *file;
    struct list_elem elem;
    struct dir *dir;
} open_file;

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
child *find_child(struct thread *t, tid_t child_pid);

#endif /* userprog/process.h */
