Final Report for Project 3: File Systems
========================================


### Task 1: Buffer Cache
Below are some of the changes we made since our initial design doc:

- In our original design, we said that we were going to make a direct mapped cache, with the LRU replacement policy. But we later realized that a direct mapped cache doesn’t have a notion of a replacement policy. Therefore, we decided to use a fully mapped cache instead of a direct mapped cache, and still use the LRU replacement policy. As a result, instead of indexing into our cache, we iterate through the whole cache to see if the entry we’re looking for is in the cache or not.  

### Task 2: Extensible Files

- In our original design, we had decided to put all of the pointers for the direct and indirect blocks in an 2d array called ‘starts’. But after our design review with our TA, we were told that a better way to store pointers is to have separate attributes in the struct for the direct, indirect, and doubly indirect blocks. This is what our disk_inode looked like at the end:
 ```C 
struct inode_disk {
    block_sector_t direct_pointers[NUM_DIRECT_POINTERS];
    block_sector_t indirect_pointer;
    block_sector_t doubly_indirect_pointer;
    bool is_dir;
    off_t length;                            /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    uint32_t unused[111];             /* Not used. */
  };
```

- We also decided to not modify the free_map_allocate and free_map_release to allow allocating contiguous blocks. Instead, each time we wanted to allocate a block, we called free_map_allocate with the `cnt` value of 1. 

- We removed the struct inode_disk `data` attribute in the inode struct since we don’t need it anymore. Instead, each time we want to access the data of an inode on disk, we fetch it from cache using the sector number of the inode. We created a helper function called `get_inode_disk` that does this for us.

- We also decided to represent each indirect block as a struct. This struct has one array attribute called `pointers` that stores all the 128 pointers to the next level blocks. We created a helper function called `get_indirect_disk` that fetches the indirect block from cache.

 ```C 
struct indirect_disk {
  block_sector_t pointers[NUM_POINTERS_PER_INDIRECT];
};
```

### Task 3: Subdirectories
In our original design we decided to add a parent_sector to in each inode_disk to have the parent directory, but in our final design we changed our `resolve_path()` so it takes the current directory, the filepath and returns a struct `resolve_metadata`. This struct returns the needed data for each particular syscall. For example, for `mkdir` it returns the parent directory of the directory and the inode of the directory we want to add and it’s name. Furthermore, for example for chdir it returns the parent directory we want to change to and it’s name. We also modified and created specific methods for most filesys methods to adapt to our design. For example we changed `filesys_open()` to now open relative and absolute paths whether it’s a file or a directory. We also added a method named `setup_dots_dir()` which adds the `.` and `..` entries in each directory. We also changed `dir_readdir()` such that it doesn’t return the `.` and `..` entries. Furthermore, we changed `dir_create()` so whenever a directory is successfully created it sets the `is_dir` attribute of the inode_disk to true. In `filesys_iniy()` we also set the current_threads current directory attribute to the root directory. In `thread_create()` we also added that when a new thread is created we set it’s current_directory to its parent’s current_directory. After getting problems we figured that using other structs attributes we needed to add methods to be able to get/set inode & inode_disks attributes. For each new syscall we create new helper functions to make our code more readable and easier to understand. We changed most syscall such that it now allows creation and removal of directories as well on the side with files. 

```C 
struct resolve_metadata {
  struct dir *parent_dir;
  struct inode *last_inode;
  char last_file_name[NAME_MAX + 1];
};
struct resolve_metadata *result_metadata = malloc(sizeof(struct resolve_metadata));
char *get_last_filename (struct resolve_metadata *metadata);
struct inode *get_last_inode (struct resolve_metadata *metadata);
struct dir *get_parent_dir (struct resolve_metadata *metadata);
void setup_dots_dir(block_sector_t sector, struct dir *parent_dir);
```

### What each member did?
- Pedram: Created the cache structure and integrated it into the inode.c file. Implemented the `byte_to_sector` function to work with our new disk_inode structure. Helped with inode_close and inode_read/write. Helped with integrating directories and writing new syscalls in task3.

- Sina : Helped with task 2 such as implementing most inode.c functions. For example, inode_create, inode_read_at /inode_write_at including solving different cases of indirect, direct and doubly indirect blocks.. Also helped with task 3 with implementing new functions in filesys.c and integrating the directories in threads. Main function such as resolve_path which allows absolute and relative paths. Also for part 3, helped with implementing syscalls and changing old syscall to adapt to our new subdirectory system. Also helped with changing directory.c methods such as create, close, open and readdir. 

- Hossein : Helped with the part for writing to cache. Helped with writing student testing code.
- Sahba : Helped with designing the cache system. Helped with writing the `byte_to_sector` function, and some aspects of extending files. Helped with writing student testing code.

What went well & what could be improved?
For this project we got more familiar with debugging using gdb much more than previous projects. 
One thing that went well was that after going online, we got used to working with a group virtually and gained more experience with how it’s gonna be working in industry. 
We need to improve our understanding of the codebase more before we start implementing.
One main problem we had was that free_allocate changed our values that we gave it and that caused many problems in our cached data which made debugging harder because it would remove values that were supposed to be saved in our memory. 
Having many variables on the stack caused our virtual machine to slow down to a point where it couldn’t allocate more memory. This caused us to go change many of the methods we had previously and implement their variables on the heap. 


#### Testing Report:
We wrote tests for scenario 1 and 2 in the project report. To do so, we added a syscall called “get_cache(int index)” which basically gets an integer argument and does one of the following tasks:
Index = 0: resetting the cache. We did this by adding a function reset_cache() in cache.c file which basically goes through entire cache and sets the sector to 0. It also resets the LRU list.
Index =1: returns the number of hits in cache. We did this by adding a global variable “cache_hit” to cache.c and increasing it whenever we have a hit in cache.
Index =2: returns total number of accesses to cache. We add a global varaiable “cache_access” in cache.c to keep track of number of accesses.
Index =3: return the number of reads in disk by calling a function called “block_read_write_cnt(fs_device,0)” which is implemented in block.c
Index =4: return the number of writes in disk by calling a function called “block_read_write_cnt(fs_device,1)” which is implemented in block.c
In test1 which is called “CacheTest1.c”,we first create a file and write 30 entries of 512 bytes into it using a built in “random” function. Then we close the file and reset cache by calling syscall “get_cache(0)”. Then, we open the file and do the first read. We then call syscall “get_cache(1)” and “get_cache(2)” to get the number of hits and accesses to the cache on the first read. Then, we close the file and re-open it to do the second read. Then we compute the hit rate for both first and second read. If the second read has a higher hit rate, then this test would pass. 
In test2 which is called “CacheTest2.c”, we first write 64KB in a file byte-by-byte and then open it to read it entirely byte-by-byte. Using syscall “get_cache(3)” and “get_cache(4)”, we get total number of reads and writes from and to disk. This pass would pass if the number of writes is in order of 128 by ensuring the value is in range of 118-138. 

We ran into kernel panic in both tests and couldn’t figure out the reason behind it. The following shows the output and result of both tests:


CacheTest1.output:
Copying tests/filesys/extended/CacheTest1 to scratch partition...
Copying tests/filesys/extended/tar to scratch partition...
qemu-system-i386 -device isa-debug-exit -hda /tmp/6THkzvpeec.dsk -hdb tmp.dsk -m 4 -net none -nographic -monitor null
PiLo hda1
Loading............
Kernel command line: -q -f extract run CacheTest1
Pintos booting with 3,968 kB RAM...
367 pages available in kernel pool.
367 pages available in user pool.
Calibrating timer...  410,419,200 loops/s.
hda: 1,008 sectors (504 kB), model "QM00001", serial "QEMU HARDDISK"
hda1: 199 sectors (99 kB), Pintos OS kernel (20)
hda2: 242 sectors (121 kB), Pintos scratch (22)
hdb: 5,040 sectors (2 MB), model "QM00002", serial "QEMU HARDDISK"
hdb1: 4,096 sectors (2 MB), Pintos file system (21)
filesys: using hdb1
scratch: using hda2
Formatting file system...done.
Boot complete.
Extracting ustar archive from scratch device into file system...
Putting 'CacheTest1' into the file system...
Putting 'tar' into the file system...
Erasing ustar archive...
Executing 'CacheTest1':
(CacheTest1) begin
(CacheTest1) create "xyz"
(CacheTest1) open "xyz" for writing
(CacheTest1) close "xyz" after writing
(CacheTest1) cache reset
Kernel PANIC at ../../devices/block.c:112 in check_sector(): Access past end of device hdb1 (sector=3221409374, size=4096)

Call stack: 0xc0028bcf.
The `backtrace' program can make call stacks useful.
Read "Backtraces" in the "Debugging Tools" chapter
of the Pintos documentation for more information.
Kernel PANIC recursion at ../../threads/synch.c:197 in lock_acquire().
Simulation terminated due to kernel panic.

CacheTest1.result:
FAIL
Kernel panic in run: PANIC at ../../devices/block.c:112 in check_sector(): Access past end of device hdb1 (sector=3221409374, size=4096)
Call stack: 0xc0028bcf
Translation of call stack:
0xc0028bcf: debug_panic (.../../lib/kernel/debug.c:38)

CacheTest2.output:
Copying tests/filesys/extended/CacheTest2 to scratch partition...
Copying tests/filesys/extended/tar to scratch partition...
qemu-system-i386 -device isa-debug-exit -hda /tmp/JKYq2hvoTl.dsk -hdb tmp.dsk -m 4 -net none -nographic -monitor null
PiLo hda1
Loading............
Kernel command line: -q -f extract run CacheTest2
Pintos booting with 3,968 kB RAM...
367 pages available in kernel pool.
367 pages available in user pool.
Calibrating timer...  209,510,400 loops/s.
hda: 1,008 sectors (504 kB), model "QM00001", serial "QEMU HARDDISK"
hda1: 199 sectors (99 kB), Pintos OS kernel (20)
hda2: 241 sectors (120 kB), Pintos scratch (22)
hdb: 5,040 sectors (2 MB), model "QM00002", serial "QEMU HARDDISK"
hdb1: 4,096 sectors (2 MB), Pintos file system (21)
filesys: using hdb1
scratch: using hda2
Formatting file system...done.
Boot complete.
Extracting ustar archive from scratch device into file system...
Putting 'CacheTest2' into the file system...
Putting 'tar' into the file system...
Erasing ustar archive...
Executing 'CacheTest2':
(CacheTest2) begin
(CacheTest2) create "xyz"
(CacheTest2) open "xyz" for writing
(CacheTest2) writing 64kB to "xyz" byte-by-byte
Kernel PANIC at ../../threads/thread.c:281 in thread_current(): assertion `is_thread (t)' failed.
Call stack: 0xc0028bcf.
The `backtrace' program can make call stacks useful.
Read "Backtraces" in the "Debugging Tools" chapter
of the Pintos documentation for more information.
Kernel PANIC recursion at ../../threads/thread.c:281 in thread_current().
PiLo hda1
Loading............
Kernel command line: -q -f extract run CacheTest2
Pintos booting with 3,968 kB RAM...
367 pages available in kernel pool.
367 pages available in user pool.
Calibrating timer...  400,588,800 loops/s.
hda: 1,008 sectors (504 kB), model "QM00001", serial "QEMU HARDDISK"
hda1: 199 sectors (99 kB), Pintos OS kernel (20)
hda2: 241 sectors (120 kB), Pintos scratch (22)
hdb: 5,040 sectors (2 MB), model "QM00002", serial "QEMU HARDDISK"
hdb1: 4,096 sectors (2 MB), Pintos file system (21)
filesys: using hdb1
scratch: using hda2
Formatting file system...done.
Boot complete.
Extracting ustar archive from scratch device into file system...
Erasing ustar archive...
Executing 'CacheTest2':
load: CacheTest2: open failed
Execution of 'CacheTest2' complete.
Timer: 57 ticks
Thread: 0 idle ticks, 57 kernel ticks, 0 user ticks
hdb1 (filesys): 0 reads, 4 writes
hda2 (scratch): 1 reads, 2 writes
Console: 886 characters output
Keyboard: 0 keys pressed
Exception: 0 page faults
Powering off...

CacheTest2.result:
FAIL
Kernel panic in run: PANIC at ../../threads/thread.c:281 in thread_current(): assertion `is_thread (t)' failed.
Call stack: 0xc0028bcf
Translation of call stack:
0xc0028bcf: debug_panic (.../../lib/kernel/debug.c:38)





In the process of writing tests, we learned how to write syscalls that could access underlying data structures (like cache or disk) and return the information we want.
