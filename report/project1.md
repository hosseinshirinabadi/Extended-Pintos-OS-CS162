Final Report for Project 1: User Programs
=========================================

### Task 1:
For this task, we mostly stuck to our original design. Some of the minor changes are listed below:
- As suggested by out TA, we used `strtok_r` instead of `strtok` for parsing the command line argument
- We modified the parser to set static global variables argv and argc instead of returning a list, to accessing argv and argc in setup stack easier
- We used padding to make our stacks 16-byte aligned
Some challenges we ran into for setting up the stack:
    - Conversion of esp to pointer for decrementing it
    - Figuring out the best way for 16-byte alignment was a bit tricky

### Task 2:
- We created one struct instead of two structs `child_status` and `wait_status`. We named it `child_status` (typedefed to `child`) which includes all variables needed to do exec and wait.   
 ```C 
typedef struct child_status {
	tid_t pid;  // child pid
	bool isLoaded;  // true if executable loaded successfully, false otherwise
	int exit_code;  // Child exit code, if dead.
	bool isWaiting;  // whether parent is waiting for this child or not
	struct semaphore sem;  // semaphore used for waiting and waking up
	struct list_elem elem;
} child;
```
- Created helper functions for each syscall to make it more readable and allow much more generic and modularized code inside `sycall_handler`.
- Modified `start_process` to set the load status of the child. We did this to make sure when we are exiting a process or thread we know it was successfully loaded by checking it’s load and returning `-1` if not loaded successfully. 
- Added parent_thread instead of parent_id for each thread to keep track of the parent thread in order to find the child_status of the child in parent’s children list, used in `process_exit`. We did this to have access to the parents child list so we can also have access to the child’s child_status struct to change variables needed in exiting that process.
- Modified section of `SYS_EXIT` syscall to set the exit code of the child using arg[1]. We did this to also have access to the `child_status` variable so we can retrieve its exit code and set it properly to return. 
- Adding the `is_waiting` field to the child struct used to terminate if a child has already been called wait. This helped with calling wait a couple times in a row. It allows us to check if wait was already called on this child by it’s parent so we return `-1`. 
- Adding an exec_file to each thread to allow and denying writes to the executable files. This was necessary to know what executable is currently loaded and running so another thread doesn’t alter or change it. 
- Validate_arg used to validate the pointers of all the syscall arguments, not just exec. This method is used in all syscalls because we need to be sure that the arguments passed are all valid and not addressing non-user space. 
- Added a child_lock to lock accesses to the `children` list. This was necessary because if multiple threads are accessing the same list or necessary for synchronizations.  
- Created a helper function `find_child(struct thread *t, tid_t child_pid)` which finds the child with this pid in the threads children list and if not found then returns NULL. This function helped a lot with having to exit children or find children to wait. 

### Task 3 :
- Added a few more variables to our thread struct, if USERPROG we added variables such as struct list files;
```C
    // keeps track of the maximum fd created
    int current_fd;
    // list of children of this thread
    struct list children;
    // parent thread
    struct thread *parent_thread;
    // executable file
    struct file *exec_file;
```
- Created helper methods for each file syscall to make the code more readable and abstract. This allowed us to easily parse the arguments and pass it to our methods created. 
- Went into much more details and edge cases for arguments with our `validate_args()` to ensure the program doesn’t run into any error if the arguments are not valid.
- Checked each argument differently in each syscall for example for filesize check if the argument for fd is bigger than or equal to 0.
- We also found a new edge case to not only to validate the address of the first argument but also all the arguments. Also for the case of only one string as the argument, we did a byte-by-byte address validation of the entire string.
- Created a helper function `get_file_by_fd()` which goes and finds the file by it’s file descriptor. 
- We followed our design doc with having one FileLock and locking when using the the list of files and then releasing when done with the list. 
- Used most of methods given in Filesys to implement these syscalls

### What each member did?
- Hossein : Implementing the parser method for Task 1 and argument validation for syscalls
- Sina : Stack setup for arguments in Task 1, also syscalls other than exec & wait for task 2
- Sahba : Implementing some of the syscalls for File Systems in Task 3
- Pedram : Implementing rest of the syscalls for File Systems in Task 3 and implementing Exec & Wait Syscalls in Task 2
All of us helped with debugging our bugs and finding edge cases by analysing the tests given.

### What went well & what could be improved?
- We know that we have to start earlier on these projects due to having many many edge cases not visible by the design doc. Furthermore, we noticed that we have to catch these edge cases in the design doc not implementation. 
- We need to make our communication smoother and more robust within our group, to work more efficiently. Also add comments on code each member does so others know what their block does. 
- It went well in the way that we stuck mostly to our high level design doc and used it to know the steps of the algorithm going into the implementation. 
- We didn’t get stuck too long on one particular question, we moved on pretty quickly after completing each task. 
- We used material from homework  and discussions to assist us in understanding how to use locks for file systems and lists. 
