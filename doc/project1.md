Design Document for Project 1: User Programs
============================================

## Group Members

* Sina Dalir <sdalir@berkeley.edu>
* Pedram Pourdavood <pedrampd@berkeley.edu>
* Sahba Bostanbakhsh <sahba@berkeley.edu>
* Hossein Shirinabadi <hossein_shirinabadi@berkeley.edu>



### Task 1 : Argument Passing

 
 #### DataStructures & function :


```C
static char * argv[];
static int argc;
tid_t process_execute (const char *file_name);
static void start_process (void *file_name_);
static bool setup_stack (void **esp);
bool load (const char *file_name, void (**eip) (void), void **esp);
char ** parser(char *file_name);
char *strtok(char *str, const char *delim);
void *memcpy(void *dest, const void *src, size_t n);
```

We create the function `parser()` which does the tokenizing of the string file_name into word by a delimiter of spaces and add a null value to the end of each token/word


 #### Algorithm : 

 First we need to pass input to the `process_execute()` named "file_name" to the `parser()` function. 
 This will parse the file_name with the delimiter as `spaces` and save the tokenized words inside a list variable such as `argv`.
 The `parser()` function will also add a null pointer to the end of each word/token so we ensure that each string has a null character at the end.
 Now `argv[0]` will be the `command` and the rest the arguments.
 Then we set `argc` to the size of the `argv`

 Next step is to push all these onto the stack in order to pass the arguments to the process.
 We do this in  `setup_stack()` by first pushing the elements of argv to the top of the stack in any order using `memcpy`.
 Then we also add a null pointer sentinel to the top of the stack as required by C standard.
 Then we start pushing the addresses of each element of argv to the stack in a reverse order (right to left). 
 Next we push the address of the `argv` & `argc` variables to the top of the stack in the same order. 
 Finally we push a fake return address of 0 to the top of stack due to stack standard architecture. 


 #### Synchronization: 
None. `process_execute` already handles race conditions 
 #### Rationale :
Our implementation seems good by looking at the requirements inside the pregame. It follows the same order as the  stack structure after processing a process. The runtime of most methods are contact and space efficiently allocated. The only function that will take longer is the parser() which takes at most O(n) where n is the max argument size. We came up with the parser() function which works better then our other methods which parsed the arguments within the function already available because it’s more robust and more abstracted. 

### Task 2: Process Control Syscalls


#### DataStructures & function :

```C
static void syscall_handler (struct intr_frame *f UNUSED); //modified

static bool validate_exec(char * arg);

int process_wait (tid_t child_tid UNUSED); // modified

tid_t process_execute(const char *file_name); //modified

tid_t thread_create(const char *name, int priority, thread_func *function, void *aux); // modified

Struct child_status {
    char *status;
    struct semaphore sem;
    pid_t parent_id;
    struct list_elem elem;
}

struct wait_status {
    /* ‘children’ list element. */
    struct list_elem elem; 
    /* Protects ref_cnt. */
    struct lock lock;
    /* ‘children’ list element. */ 
    /* Protects ref_cnt. */
    /* 2=child and parent both alive, 1=either child or parent alive,0=child and parent both dead. */
    int ref_cnt; 
    /* Child thread id. */
    tid_t tid;
    /* Child exit code, if dead. */
    int exit_code;
    /* 0=child alive, 1=child dead. */
    struct semaphore dead;
    };
    /* This process’s completion state. */ 
    struct wait_status *wait_status; 
    /* Completion status of children. */
    struct list children;
}

```

`validate_exec()` is only used in exec and it’s used to check whether the argument given is not a null pointer and it’s a valid command in pintos command line & is also is a valid user memory address.

The struct wait_status is used for wait only because we have to keep track of all variables within the struct for each child. This help with the synchronization and data sharing between parent and children

We will modify `process_wait()` to go through child processes and check whether their pid matches the pid given and call sema_down for that child. Also remember to to free memory of “zombie processes” 


 #### Algorithm:

We will implement all algorithms in the `syscall_handler()` function inside userprog/syscall.c. The args[0] will contain the syscall type, and the subsequent elements will contain the syscall arguments, if any.

- args[0] == SYS_HALT:
Calling `shutdown_power_off()` will terminate Pintos.

- args[0] == SYS_EXEC:
We first use the validate_exec function to validate the command line argument passed to the exec syscall. This will make sure that the argument passed in is a valid null-terminator command, its pointer is in the user space, and is not null.
Next, we will make modifications to the `process_execute` function: we first create a `child_status` struct and add it to the list of current’s process children and we set the semaphore value of this struct to 0. Then using the `thread_create` function, we create a new thread and modify it to set the `parent_id` of the child struct to be the thread id of the parent process. Then after the new program has been loaded, we increment the semaphore by calling sema_up.

Then, after process_execute has returned a tid_t for the newly created thread, we find the child whose pid_t is equal to the tid_t and decrement its semaphore using sema_down. By doing so, we will know if the child has been loaded successfully or not. If it was loaded successfully, we return the child’s pid, otherwise we return -1 to indicate a failure in loading.


- args[0] == SYS_WAIT:

This syscall will use the function `process_wait()`. Before using the arguments passed to the function we must validate them using `validate()`. 
The parent will call sys_wait this will call `process_wait` with a tid_t. 
At first we have to check whether this tid is in the children list. If no, we return -1 because this thread is not the parent’s child.
 If yes, we first check if the child exit_code is 0, that means the child is exited and we will return the code and continue with the parent but if the child’s exit_code is not 0 it means it’s still not exited ,therefore we will `sema_down()` the the lock shared between the parent and child. This will block the parent until the child is done. After the child is done or exits,it has already set it’s exit_code &  it will `sema_up()` which will mean the parent can continue running. 
Before returning destroy the shared data and remove it from list of children of parent to prevent zombie processes and return the child’s exit_code. 


- args[0] == SYS_PRACTICE:
This will take the second argument `args[1]` which is an int and increment it and set the increment value to the  frame’s eax register by using  `f->eax = args[1] + 1` 


 #### Synchronization:

In every SYS_EXEC  a child process is created  and its parent must be blocked until the child exits. So in some sense we need to keep track of child exit status. A form of a static data holder that both parents and child have access to and the information won't be lost  even after the process exits. So we need a separate structure to handle this task. The structure  keeps track of the childs as well as the process status of each one.
 #### Rationale :
We tried having one global semaphore but that caused many concurrency issues therefore we made the decision to create a struct between parents and children to make it more shared only between each parent and children. This way if a child also has children this will not allow concurrency issues to occur in that case. 

### Task 3: File Operation Syscalls

#### DataStructures & function:

```C
// global lock for file system level
struct lock flock;

// list of all files for the current process
struct list files;

// keeps track of the maximum fd created
int current_fd;

struct file_status {
    int fd;
    char *file_name;
    struct file *file;
    struct list_elem elem;
}
```
#### Algorithm:

For all of the following syscalls, first we call `validate()` (defined in task 2) in order to ensure the correctness of the arguments and their size (14 characters max), as well as validating the memory buffer. In addition, we acquire an `flock` in the beginning of each of the function calls, and release them before it returns. All of the mentioned `filesys_xxx` function calls are inside `filesys/filesys.c`.

- create: call the `filesys_create` which will create a new file initially initial_size bytes in size. Returns true if successful, false otherwise.

- remove: loop through the `files` list to find the specified file in the current process and remove it from list. Then call `filesys_remove` on the file. 

- open: open the specified file using the `filesys_open` function. Then, get this file’s fd by adding 1 to `current_fd` and update current_fd. Create an instance of the file_status struct using these info and add it to the `files` list. 

- filesize: call `file_length` function on the file

- read: call `file_read` function on the file 


- write: file_write

- seek: file_seek

- tell: file_tell

- close: file_close


#### Synchronization: 

According to the spec we can use a global lock since we will make a scheduler and a more robust locking mechanism in project 3. We will then lock every file on open and make sure no other thread or process has access or can modify that file while another process is running on it. Then release the lock before returning from the syscall having a lock on the file. This allows for no thread to concurrently use the file and gets rid of any data race.


 #### Rationale :
The lock system causes the multiple file processes not interfere or cause a deadlock in the processes.


### Additional Questions

#### 1. 
The test sc-bad-sp.c test is the one that tests this. 
If we look at the test we see that it invokes two lines of assembly by first moving the esp (stack pointer ) to 64*1024*1024 down which is about 64Mb. 
The exact line is line 18 : `movl $.-(64*1024*1024), %esp`
This moves the stack pointer to a invalid section not inside user memory. Therefore it crashes when a syscall is called in the same line by : `int $0x30`
This crashes because the stack pointer given to that syscall is invalid 

#### 2.
 
The test sc-bad-sp.c test is the one that tests this. 
If we look closer it’s similar to the same question as above but with a small subtle difference. 

In this test it changes the esp to the value 0xbffffffc by calling the assembly : `movl $0xbffffffc, %%esp` on line 14 manually then it sets the value at that location to a 4 byte number 0; 

This 4 byte number of 0x0 is extended 32 bits making it go to `0xbffffffc + 0x00000020 = 0xc000001c`

Next a syscall is called by calling `int $0x30` the syscall number now goes into invalid memory(kernel memory) by reading the syscall number which is above `PHYS_BASE`


#### 3. 


The test suit doesn't check whether a few processes share a file and want to access at same time and if one closes can other processes or threads still have access to the file which they should. Also another case is when a deadlock occurs between processes. This is a very important case because if a deadlock occurs the program will just run infinitely. Since all processes are waiting for another one to finish but are deadlocked together. 

