Design Document for Project 3: File Systems
===========================================

## Group Members

* Sina Dalir <sdalir@berkeley.edu>
* Pedram Pourdavood <pedrampd@berkeley.edu>
* Sahba Bostanbakhsh <sahba@berkeley.edu>
* Hossein Shirinabadi <hossein_shirinabadi@berkeley.edu>

### Task 1: Buffer Cache

#### DataStructures & Functions:
We add a new file called cache.c that contains the following:
```C
struct cache_block {
    block_sector_t sector;
    char data[BLOCK_SECTOR_SIZE];
    int dirty;
    int valid;
    struct lock cache_lock;
    int num_readers;
    int num_writers;
    struct list_elem elem;
}
struct cache_block cache[64];
struct list LRU;
```
#### Algorithms:
Cache Design: Direct Mapped Cache, with LRU replacement policy
- We first check whether the requested file exists in the cache or not. We do this by indexing into the `cache` array (by taking the mod 64 of the sector number). If the result is NULL, we know that this is the first time we’re accessing the file, therefore we need to fetch the requested block from disk to read/write from/to it. After fetching the block from disk, we create a new `cache_block` struct with appropriate information regarding this block (setting the dirty bit if needed, acquiring the lock, etc.), and add it to the correct entry of the `cache` array. We also add it to the front of the `LRU` list, for replacement purposes.
- If we find the requested block in the cache, we first check if the block is blocked by another reader or writer. If it is blocked then we simply wait. Otherwise we have a green to proceed and acquire the lock for this block. We then check the Dirty and valid bits to see if the data is any good or if it is still available on the disk. Now we can add to num_readers or num_writers (based on whatever the task is). This block in the cache was recently used so we just add it to the front of the LRU list for future eviction. If the thread requeq to write data, we need to change the dirty bit to 1, so later we can right back to disk when we want to evict that block from the cache. We can now pass the block to the thread that requested that block by indexing in the `cache` array to find the corresponding entry. 
- If the cache is full or if there’s a conflict, we need to evict the block that is in back of the `LRU` list. Before we do that, first we need to check if the block is blocked by any other threads. If yes we wait until the treads exits, then we can require the lock and check the dirty bit and if we need to write back to the disk then we can reset the struck variables and get the requested block from the sector on the disk and replace it into the cache. Then we proceed by doing the normal caching process, as mentioned above.

#### Synchronization:
For Synchronization we give each `cache_block` its own lock and check if the request block in the cache is blocked, if not we can require the lock. This way we are ensuring that the issue while accessing the cache block won’t cause any data loss when there are simultaneous accesses to the same sector. This will also allow us to run concurrent operations on different sectors. There could be many different issues such as:
 when we find the block and now we want to pass it to the thread that requested that block. We  don’t want  threads to write data while some other thread is accessing the block.
While evicting a block from the cache and there is a thread acquired the lock before there was any need for eviction, we need to wait for that thread to exit and based on the dirty bit we can right it back to the dick, if the block was not blocked we could have lose data that was written into cache while evicting process began.  
We also have a lock for the `LRU` list in order to reliably find the least recently used block. 

#### Rationale:
We discussed whether to use a Direct Mapped cache or an N-way/filly associative cache. We decided to go with the direct mapped cache; our reasoning was that even though it has a lower hit rate, it has higher performance than others and since we’re developing a cache for the sake of performance, it would be a reasonable choice. It would also be much easier to deal with synchronization issues in a direct mapped cache.
We also discussed various replacement policies. The two we mostly debated on were the LRU and the Clock policies. We decided to go with the LRU since it was simpler to implement and is a closer approximate to the MIN algorithm. Even though it is slower in theory, we relied on the efficiency of the Pintos list data structure. 

### Task 2: Extensible Files
#### DataStructures & Functions:
```C
struct inode {
    struct list_elem elem;
    block_sector_t sector;
    int open_cnt;
    bool removed;
    int deny_write_cnt;
    struct inode_disk data;
 };

struct inode_disk {
    block_sector_t *starts[12 + 1 + 1];
    off_t length;
    unsigned magic;
    uint32_t unused[125];
};

off_t inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset); //Modified
off_t inode_write_at (struct inode *inode, const void *buffer_, off_t size, off_t offset); //Modified
Bool inode_create (block_sector_t sector, off_t length); //Modified
struct inode * inode_open (block_sector_t sector); //Modified
Void inode_close (struct inode *inode); //Modified
Void inode_remove (struct inode *inode); //Modified

static void syscall_handler (struct intr_frame *f UNUSED);  // modified for the inumber syscall
```
#### Algorithms:

First we change the attribute `start` in `inode_disk` to be `starts` which is a pointer to `block_sector_disk arrays`. We need this change to be able to implement direct, indirect and double indirect blocks. Now, instead of having a contiguous filesys, we have to iterate through all the sectors on disk (or in cache) to read/write. Therefore, we have to modify all the functions in the `inode.c` file to reflect the changes to our modified `inode` struct.
For `inode_create` and `inode_write_at` functions, we also have to modify the `free_map_allocate` function in order to find any free blocks available instead of contiguous blocks. For `inode_write_at`, if we’re writing past EOF, we need to extend our blocks by allocating new sectors for the file. We also need to initialize the `starts` variable to be initialized to x sectors, where x is equal to number of direct, indirect, and double indirect blocks.
For `inode_read_at()` & ` inode_write_at(), we are using` byte_to_sector()` to get the correct sector to read or write in. And offset the rest to get to the correct byte. If it’s an indirect block we start by the sector found and iterate through our block_sector array. This way we can read or write to non-contiguous blocks but give the illusion of contiguous blocks.

- int inumber(int fd): To implement this syscall, we first validate the fd passed in to make sure that it’s a valid that points to an inode. Then, we call `inode_get_number` on the found inode which will return the `sector` number of the inode. 
#### Synchronization:
Instead of having a global lock or a lock for each file we will have a lock for each cache block. Since multiple threads should be able to read data but only one thread  is able to write to the file at the time. Also while initializing a bitmap we will have global lock that should be acquired whenever that bitmap is used by a process, so that when multiple processes want to create inodes or extend files, there won’t be any race conditioning issues.
#### Rationale:
We made many choices which got us to our final design. One main thing was where to place the lock. We decided each bitmap will have a lock to allow for us to stop contiguous reads and writes. We chose a double block_sector_t array over having three variables. This way we can just iterate through even if there is only direct over having to initialize and keep track of three variables. The coding is going to be a lot more than past projects. Most of inode’s methods will have to be modified. Our algorithm is good in the way that it’ll be easy to modify if more features get added. Time Complexity of the functions we modify and create don’t cause much time. They are all in constant or linear time. 


### Task 3: Subdirectories

#### DataStructures & Functions:
```C
struct inode_disk {
    ….
    bool is_directory;
    block_sector_t parent_sector;
}

struct thread {
    ….
    struct dir *current_directory;
}

char *strtok_r(char *str, const char *delim, char **saveptr);  // used in parsing the absolute and relative file names

char * parseFilePath(char* filePath);
```
#### Algorithms:
In each thread, we keep track of the current working directory in order to make changing directories independent of each process. Also, when a process calls `exec`, the child process will have the same current directory as the parent. 
We are using `parseFilePath()` to check the filename or filepaths. This function will be used in many other functions to always validate the filepath and filename given to see if it exists or filepath is correct. Using `get_next_part()` , `strtok_r()`  and the same implementation ideas in homework 3 helps a lot with implementing this function. We will also update the necessary syscall such as open, close and remove to accept relative and absolute paths. If the path is an absolute path, we start from the root directory and parse each part of the path to move through directories. If it’s a relative path, we add the path to the end of the current directory path, and use that to find the file we’re looking for. 
We also modify `sys_remove()` so that when a file is removed we also remove it from its corresponding directory by referencing it using `parent_sector`. Also if the fd given is a directory and it’s empty we also allow removal of that. If the directory is not empty we error. 
1. bool chdir(const char *dir): After parsing the possible relative or absolute directory name, we change the `current_directoy` variable of the current thread to be `dir`.
2. bool mkdir(const char *dir): create a new `dir_entry` in the current directory with the name of `dir`. We add the entry to the list of directory entries under the current directory. 
3. bool readdir(int fd, char *name): we will get the inode from the given fd and check whether it’s a directory or not. If not return false, if yes, go though the entries of the directory and find the entry with the given name. If an entry with the name given doesn’t exist return false. If entry was found, return true. We also allow the name to be larger than DIR_MAX_LEN, therefore we change it to be larger than 14 because of absolute paths. 
4. bool isdir(int fd): We just need to check the `is_directory` of the `inode_disk` represented by the `indoe` pointed by fd.
#### Synchronization:
Our implementation for the last two tasks makes sure that while a process is running on a specific directory another process can not access or modify that directory, and it has to wait until the other process exits or releases the lock in the cache.
#### Rationale:
We decided to make a list which keeps track of directory entries in a directory. We also added a parent pointer in each inode_disk so that navigating through directories would be easier for checking and comparing directory entries. 
#### Question 1:
For `write_behind`, we would need to have a clock and on every few ticks, we would check the dirty bit of the cache entries and flush those that have their bits set. We do this by creating a thread in the beginning of caching; this background thread wakes up on every few ticks and goes on and flush dirty blocks to the disk. We then set the dirty bits to 0 since we’ve written to disk and are not dirty anymore.  
For `read_ahead` we create a histogram to get data of how frequently other sectors are used with the current sector. Therefore, when a sector is not cached in our memory we will go to disk to get that sector, but by having read_ahead, we will also bring the top 10 other sectors that have the most used points inside our histogram. We update the histogram each time a block is read/written before or after another block. This will make the spatial locality of our system better, since the blocks are not contagious anymore. 
