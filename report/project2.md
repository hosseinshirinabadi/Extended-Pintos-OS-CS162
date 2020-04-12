Final Report for Project 2: Threads
===================================


## Group Members

* Sina Dalir <sdalir@berkeley.edu>
* Pedram Pourdavood <pedrampd@berkeley.edu>
* Sahba Bostanbakhsh <sahba@berkeley.edu>
* Hossein Shirinabadi <hossein_shirinabadi@berkeley.edu>


### Task 1 : Efficient Alarm Clock

For this task, we mostly stuck to our original design. Some of the minor changes are listed below:

We decided to only disable interrupts when needed so that we won’t be slowing down other processes.

Challenges : Not many challenges since we had the algorithm in our design doc followed that and it was enough.



### Task 2 : Priority Scheduler 

For this task, we also mostly stuck to our original design. The changes made are listed below:

We decided to not store a list of `waiting_locks` in the thread struct but just one `waiting_lock` because a thread can only be blocked due to one lock at a time not multiple. 
We also thought ahead for the other scheduler such as MLFQS so we added a nice and other functions before design doc review, but after we removed those. 
In thread.h:
```C
struct thread
{
struct list current_locks;  // locks held by this thread
struct lock*  waiting_lock;  // lock this thread is waiting for
int base_priority;  // priority before receiving donation
}
```

We also decided to not use the `waiting_threads` variable inside the lock struct. Instead, we used the ‘waiters’ list in the semaphore struct.
We didn’t use our `highest_priority()` function by suggestion of our TA we just get the max element in our ready_list by calling `list_max()` using our comparator named `priority_comparator()`. Therefore, instead of going from the start of the ordered list to get the top we just get the max. Much easier. 
```C
static bool priority_comparator(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);    // Used to compare the priority of two threads
```

In `init_thread()` we just initialize the thread struct by setting the base_priority to priority, initializing `current_locks` and `waiting_lock`
In `lock_aquire()` we also disabled interrupts. The main difference is that we only set `waiting_lock` to the lock of the thread if another lock is holding it. We also call `recursive_donation()` only when the lock holder has a lower priority then the current_thread, which donates to all threads that need to be donated. 



```C
void recursive_donation(struct thread *T);   //Donates to all the threads that are in the recursive and chain donation tree. 
```

We also changed `thread_set_priority()` such that in the case that the priority is a donated priority and the value given is less than the current priority, we don’t set it. This stops this function from messing up the donation process. 
`lock_try_acquire()` was also changed such that if the lock is acquired, then set the threads `waiting_lock` to NULL and set the lock’s holder to this thread.
In `lock_release()` we also disabled interrupts and changed the case that when a thread whose priority was donated to it releases a lock, it’s priority has to return to its base or other donated priorities. For this case we made a function called `maxPriorityPostDonation()`. This function iterates through the current_locks of the thread and finds the maximum priority of all of the waiting threads for each lock. This allows us to catch the case when multiple threads have donated to a thread but when one lock is released it’s priority won’t go back to it’s base_priority but the next highest donated priority instead. If current_locks is empty or the max is less than base_priority then set priority to base_priority. 
```C
int maxPriorityPostDonation(struct thread* t); // return the next highest priority donated, if less than base_priority then return base_priority. 
```

In `sema_up()` & `cond_signal()` instead of sorting and getting the highest priority thread, we just get the max priority thread in waiters by using `list_max()` with our comparators `condition_comparator` and `priority_comparator`. 



### What did each member do?
- Hossein : Worked on Task 3 
- Sina : Worked on Task 1 time sleep and Task 2 scheduler 
- Sahba : Worked on Task 3 
- Pedram : Worked on Task 1 time sleep and Task 2 scheduler



### What went well & what could be improved?

In this project, we had a more efficient collaboration and teamwork.
We wrote our design doc with much more thought, which made the implementation much more easier.
Due to this crisis going on it was a bit hard having everyone at once and being able to share thoughts with the whole team.
Going through the test cases really help understand the edge cases. This led to a cleaner and more robust code & implementation.



	


	
