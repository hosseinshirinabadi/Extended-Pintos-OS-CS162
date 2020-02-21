#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);

typedef struct file_status {
    int fd;
    char *file_name;
    struct file *file;
    struct list_elem elem;
} open_file;

// global lock for file system level
struct lock flock;


open_file *get_file (int fd) {
	struct list_elem *e;
	struct thread *current_thread = thread_current();
	for (e = list_begin(&current_thread->files); e != list_end(&current_thread->files); e = list_next(e)) {
	    open_file *current_file = list_entry(e, open_file, elem);
	    if (current_file->fd == fd) {
	    	return current_file;
	    }
  	}
  	return NULL;
}

bool create_helper (const char *file, unsigned initial_size) {
	return filesys_create (file, initial_size);
}

int open_helper (const char *file) {
	lock_acquire(&flock);
	struct file *opened_file = filesys_open(file);
	if (opened_file) {
		struct thread *current_thread = thread_current();
		int fd = (current_thread->current_fd)++;
		open_file *file_element = malloc(sizeof(open_file));
		file_element->fd = fd;
		file_element->file_name = file;
		file_element->file = opened_file;
		list_push_back(&current_thread->files, &file_element->elem);
		return fd;
	} else {
		return -1;
	}
}

int filesize_helper (int fd) {
	open_file *found_file = get_file(fd);
	if (found_file) {
		return file_length (found_file->file);
	} else {
		return -1;
	}
}

int write_helper (int fd, const void *buffer, unsigned size) {
	open_file *file = get_file(fd);
	if (file) {
		if (fd == 1) {
			putbuf (buffer, size);
    		return size;
		} else {
			return file_write(file, buffer, size);
		}
	} else {
		return 0;
	}
}



void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

  lock_init(&flock);

}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  uint32_t* args = ((uint32_t*) f->esp);

  /*
   * The following print statement, if uncommented, will print out the syscall
   * number whenever a process enters a system call. You might find it useful
   * when debugging. It will cause tests to fail, however, so you should not
   * include it in your final submission.
   */

  /* printf("System call number: %d\n", args[0]); */

  if (args[0] == SYS_EXIT) {
      f->eax = args[1];
      printf ("%s: exit(%d)\n", &thread_current ()->name, args[1]);
      thread_exit ();
  } else if (args[0] == SYS_HALT) {
      shutdown_power_off();
  } else if(args[0] == SYS_PRACTICE) {
      f->eax = args[1] + 1;
  } else if (args[0] == SYS_CREATE) {
  	  const char *file = args[1];
  	  unsigned initial_size = args[2];
  	  f->eax = create_helper(file, initial_size);
  } else if (args[0] == SYS_WRITE) {
      int fd = args[1];
      const void *buffer = args[2];
      unsigned size = args[3];
      f->eax = write_helper(fd, buffer, size);
  } else if (args[0] == SYS_OPEN) {
  	  const char *file_name = args[1];
  	  f->eax = open_helper(file_name);
  } else if (args[0] == SYS_REMOVE) {
  	  
  } else if (args[0] == SYS_READ) {
  	  
  } else if (args[0] == SYS_SEEK) {
  	  
  } else if (args[0] == SYS_TELL) {
  	  
  } else if (args[0] == SYS_CLOSE) {
  	  
  } else if (args[0] == SYS_CLOSE) {
  	  
  } 
}
