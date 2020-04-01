
Design Document for Project 1: Scheduling 



## Group Members

* Sina Dalir <sdalir@berkeley.edu>
* Pedram Pourdavood <pedrampd@berkeley.edu>
* Sahba Bostanbakhsh <sahba@berkeley.edu>
* Hossein Shirinabadi <hossein_shirinabadi@berkeley.edu>


### Task 1 : Efficient Alarm Clock


#### DataStructures & Functions:

In thread.h:
```C
// modified
struct thread {
    …
    int64_t sleep_time;
    struct list_elem sleepelem;
}
```
In timer.c:
```c
// global variable that holds the threads that are sleeping
static struct list sleeping_threads;

void timer_sleep (int64_t ticks) // modified

static void timer_interrupt (struct intr_frame *args) // modified
```

#### Algorithm:
In the `timer_sleep` function, we first disable interrupts using `intr_disable`. We then take the number of ticks given as the argument (`ticks`) and add it to the current number of timer ticks since the OS booted to calculate the time the thread should wake up; the current’s thread sleep_time will then be set to this value: `thread_current()->sleep_time = timer_ticks() + ticks`. Next, the current thread is added to the list `sleeping_threads` in order based on its `sleep_time`. Shorter sleep times come first. Lastly, we block the thread using `thread_block` and restore interrupt level to what it was before using `intr_set_level`.

In the `timer_interrupt` function, in order to process the sleeping threads, we loop through the `sleeping_threads` list in order of ascending `sleep_time`: if the `sleep_time` of the current element of the list is less than the current ticks (since OS booted), we disable interrupts and we remove that thread from the list and unblock it using `thread_unblock`, which adds the thread to the ready queue (`ready_list`). We also restore interrupt level to what it was before using `intr_set_level`

#### Synchronization:
All threads share the sleeping_threads list. So in order to prevent race conditions accessing the list, we disable interrupts before doing any operations on the list, and enable them to old settings after we're done. Therefore, we know nothing will interrupt our process in the middle.  

#### Rationale:

Our current design is better than our other ones because in this one we disable interrupts only in places that are required, not just our whole code block in `timer_sleep`. This part won’t be long in coding but more difficult in conceptualizing. One advantage of our algorithm is that it never allows for “busy waiting” and it lets us to take advantage of `sleeping_threads` for future uses.


### Task 2 : Priority Scheduler 

#### DataStructures & Functions:

In synch.h:
```C
struct lock
 {
struct list waiting_threads;  // list of threads waiting for this lock
struct list_elem elem; 
struct thread owner;  // thread that owns this lock
}
```

In thread.h:
```C
struct thread
{
struct list current_locks;  // locks held by this thread
struct list waiting_locks;  // locks this thread is waiting for
int nice; // between [-20,20]
struct list_elem lock_elem; 
int base_priority;  // priority before receiving donation
}
void thread_foreach (thread_action_func *func, void *aux) (used for updating priority)
void thread_set_nice (int nice UNUSED)
int thread_get_nice (void)
int thread_get_load_avg (void)
int thread_get_recent_cpu (void)
static struct thread *next_thread_to_run (void)
void thread_schedule_tail (struct thread *prev)
static void schedule (void)
static void timer_interrupt (struct intr_frame *args UNUSED)
void recursive_donation(struct thread* T, struct lock* L);
struct thread* find_highest_priority(struct list* list);
int calc_priority(struct thread* t, int load_Average);
void sema_up (struct semaphore *sema); 
void cond_signal (struct condition *cond, struct lock *lock UNUSED); 
```

#### Algorithm:
- Choosing the next thread to run:
    - Whenever `schedule` is called in either of thread_block, thread_exit, or thread_yield, we modify the `next_thread_to_run` function to choose the thread in the `ready_list` that has the highest effective priority, using the `find_highest_priority` function. If there are multiple threads having the same highest priority, we choose them in a “round-robin” fashion.
- Acquiring a Lock:
    - If a thread t is trying to acquire a lock L and the lock hasn’t been acquired by another thread, we then add the lock to the `current_lock` list of the thread, and also set the `owner` attribute of the lock to be the thread acquiring it. Finally, we grant the lock to the thread.
    - If the lock is held by another thread, the thread t needs to wait for the lock and be blocked. Therefore, we add the lock L to the `waiting_locks` list of the thread t, and also add the thread to the `waiting_threads` list of the lock L. Then, if the thread holding the lock (found using the `owner` attribute of the lock) has a lower priority than thread t, we will perform priority donation recursively described below:
        - Priority Donation: For priority donation we are going to use our recursive function `recursive_donation` to recurse down our nested locks to find all threads that the current threads need to donate priority to and change their current priority until they release the lock. This function uses the lock and finds the current owner of the lock, donates priority to the owner if its priority is less and checks the `waiting_locks` variable of the owner and call this function on all locks inside the list to recurse to all threads in the list. 
        - Before Donating any priority we make sure the current priority is less, if not we don’t donate any priority.
- Releasing a Lock:
    -  We first check if the lock being released is actually held by the owner of the lock. If so, we remove the lock from the `current_locks` list of the thread.
    - We then recalculate the effective priority of the thread owning the lock since the priority can be changed after releasing the lock. We check if `base_priority` is different from the `priority` attribute of the thread, and if so, we know that priority has been donated to this thread. Therefore, we set `priority` to be `base_priority`, which sets the effective priority to become what it used to be before donation.
    - We then need to find which thread needs to hold the lock next (and therefore which thread to unblock first). We do this by looping through all the threads in the `waiting_threads` list of the lock, and choosing the thread that has the highest priority. After giving the lock to this chosen thread, we change the `owner` attribute of the lock to be the new thread.
    - At last, we call `thread_yield` on the thread current (this is the thread that used to hold the lock) to yield to other threads that might have the highest priority, since this thread’s effective priority might have changed. 

- Computing the effective priority: The effective priority is calculated by taking the maximum of the thread’s `priority` (which holds the donated priority) and the thread’s `base_priority` (which holds the priority of the thread before receiving donation. 

- Priority scheduling for semaphores, locks, and condition variables:
    - For `cond_signal` & `sema_up`, before popping from the list of `waiters`, we are going to sort the list on effective priority from highest to lowest. If there are equal highest priorities, we will do a round-robin on them . If not, pop the highest priority and set that thread’s status to RUNNING and switch the CPU to that thread.

- Changing thread’s priority: In `timer_interrupt` we check that every time the current ticks % 4 = 0 (i.e. every 4th ticks), we will update all threads priorities using the function ` thread_foreach()`, where we will pass in the function `calc_priority()`. This function uses `load_average`, each thread's `nice` and `recent_cpu` to calculate their new priority.

#### Synchronization:
Shared resources:
- struct list ready_list;
- struct list ready_list;
- struct list waiting_threads;
- waiters (for conditional variables and semaphores)

Whenever a thread tries to access any of the resources listed above, it will try to acquire a lock using a sema up function and while seeking the lock and gets blocked. Then we make sure the priority is shared to the thread that holds the lock for that critical section. Now if we are in a nested situation that requires multiple priority sharing, to get to run the thread who is holding the lock, the threads’ priority needs to be updated accordingly. The recursive function explained above seeks the thread who has holded the lock for the thread with the highest priority. We used a recursive call to handle a nested deadlock situation. Now since we tick first then do the priority donations therefore the priority should be synchronized as well. 

#### Rationale:
Our design is to use Lists to keep track of the threads (their priorities) and locks for each thread. We need to iterate over the list to keep the priorities updated and make sure the list is sorted. We don’t do this in a loop but instead we take advantage of the PintOS list and the sort method provided to sort the list which has an efficient sorting algorithm according to Pintos documentation. This of course will be at the expense of some runtime, but makes the code simpler to implement and causes faster bug detection. For priority donation we were between using a variable to keep track such as a list instead we created a method to recursively find all nested threads which we are going to donate to. 



### Design Document Additional Questions
1. In schedule when the next thread to run is chosen. Then switch_threads is called, which then goes and saves the current threads stack pointer, program counter and registers on the threads stack for later use. Then changes all necessary registers in the CPU to the next threads stack pointer, program counter and registers. This allows the kernel to switch easily and not lose any data from past threads. Everytime a thread is switched the new thread loads all of these attributes on to the CPU and starts running again. 
2. The page containing the stack and TCB is freed inside of `thread_schedule_tail()`. We can’t just free the page inside of thread_exit because if the thread we switched from is dying, we destroy its struct thread only.  We must delete the page later so that thread_exit() doesn't pull out the rug under itself.  We don't want to free initial_thread because its memory was not obtained via palloc().
3. Since `timer_interrupt()` function runs in an external interrupt context, the kernel puts it on the interrupted thread’s kernel stack.
4. Our Test starts with the main thread:
- Main Thread :
    - Creates a Lock L
    - Creates a semaphore S with initial value of 0
    - Pthread_create a thread “L” with priority 40 and pass in S & L 
    - Pthread_create a thread “M” with priority 50 and pass in S & L
    - Pthread_create a thread “H” with priority 60 and pass in L
    - Sema_up (S)
    - …


	
- Thread L :
    - Acquire Lock L
    - Sema_down(S) (which causes the thread to block)
    - release(L)
    - print(“THREAD L FINISHED”) 

- Thread M  :
    - Sema_down(S) (also causes the thread to block)
    - sema_up(S) 
    - print(“THREAD M FINISHED”) 

- Thread H  :
    - Acquire Lock L (has to wait for A to release)
    - print(“THREAD H FINISHED”) 

#### Correct Scheduler:
Following this test if the priority donation does not have a bug and actually uses the effective priority, then our test starts with the main creating the a Lock and Semaphore then creating the 3 threads each with different priorities of 40, 50 and 60. Then the main thread calls sema_up(S). Thread L acquires the lock L and tries to sema_down(S) but gets blocked. Then Thread M tries to sema_down(S) and also blocks. After that, Thread H tries to acquire the lock but has to wait for thread L. Due to this waiting thread H donates it’s priority to thread L which causes L to run again but it can’t due to the semaphore. The main thread then calls sema_up(S) which wakes up thread L (now that it has a high priority), then L releases the lock L and yields which wakes up thread H. Then H starts running again ,terminates and wakes up thread M because it has highest priority holding the semaphore not thread L. After that thread M executes and calls sema_up(S) which wakes up thread C. Then thread C terminates following the main thread.
The Output of the correct scheduler will be :
1. THREAD H FINISHED
2. THREAD M FINISHED
3. THREAD L FINISHED
4. MAIN still running 

#### Incorrect Scheduler:
If the current scheduler has a bug it will follow the same process just before the main calls sema_up(S). When the main thread calls sema_up(S), it will wake up thread M because it’s the highest priority thread that’s READY. Therefore M runs and calls sema_up(S) and terminates. After that, thread L wakes up and releases Lock L. It also yields which then wakes up thread H. Then, thread H terminates and also calls sema_up(S). After that thread L runs and terminates following the main thread. 

The Output of the incorrect scheduler will be :
1. THREAD M FINISHED
2. THREAD H FINISHED
3. THREAD L FINISHED
4. MAIN still running


