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


open_file *get_file_by_fd (int fd) {
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

open_file *get_file_by_name (const char *file_name) {
	struct list_elem *e;
	struct thread *current_thread = thread_current();
	for (e = list_begin(&current_thread->files); e != list_end(&current_thread->files); e = list_next(e)) {
	    open_file *current_file = list_entry(e, open_file, elem);
	    if (current_file->file_name == file_name) {
	    	return current_file;
	    }
  	}
  	return NULL;
}



bool create_helper (const char *file, unsigned initial_size) {
	return filesys_create (file, initial_size);
}

int open_helper (const char *file) {
	// lock_acquire(&flock);
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
	open_file *found_file = get_file_by_fd(fd);
	if (found_file) {
		return file_length (found_file->file);
	} else {
		return -1;
	}
}

int write_helper (int fd, const void *buffer, unsigned size) {
	open_file *file = get_file_by_fd(fd);
	if (file) {
		return file_write(file, (const void * ) buffer, size);
	} else {
		return 0;
	}
}

bool remove_helper(const char *file_name) {
	return filesys_remove(file_name);
}

int read_helper(int fd, void *buffer, unsigned size) {
	open_file *file = get_file_by_fd(fd);
	if (file) {
		return file_read(file->file, buffer, size);
	} else {
		return -1;
	}
}

void seek_helper(int fd, unsigned position) {
	open_file *file = get_file_by_fd(fd);
	if (file) {
		file_seek (file->file, position);
	}
}

unsigned tell_helper(int fd) {
	open_file *file = get_file_by_fd(fd);
	if (file) {
		return file_tell(file->file);
	} else {
		return 0;
	}
}

void close_helper(int fd) {
	open_file *file = get_file_by_fd(fd);
	if (file) {
		list_remove(&file->elem);
		file_close(file->file);
		free(file);
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

   printf("System call number: %d\n", args[0]); 

  if (args[0] == SYS_EXIT) {
      f->eax = args[1];
      printf ("%s: exit(%d)\n", &thread_current ()->name, args[1]);
      thread_exit ();
  } 
  else if (args[0] == SYS_HALT) {
      shutdown_power_off();
  } else if (args[0] == SYS_PRACTICE) {
      f->eax = args[1] + 1;
  } else if (args[0] == SYS_CREATE) {
  	  const char *file = (char * ) args[1];
  	  unsigned initial_size = args[2];
  	  f->eax = create_helper(file, initial_size);
  } else if (args[0] == SYS_WRITE) {
      int fd = args[1];
      const void *buffer = args[2];
      unsigned size = args[3];
      if (fd == 1) {
      	putbuf ((const char *) args[2], size);
      	f->eax = size;
      } else {
      	f->eax = write_helper(fd, buffer, size);
      }
  } else if (args[0] == SYS_OPEN) {
  	  const char *file_name = (char *) args[1];
  	  f->eax = open_helper(file_name);
  } else if (args[0] == SYS_REMOVE) {
  	  const char *file_name = (char * ) args[1];
  	  f->eax = remove_helper(file_name);
  } else if (args[0] == SYS_READ) {
  	  int fd = args[1];
  	  unsigned size = args[3];
  	  if (fd == 0) {
  	  	uint8_t *buffer = (uint8_t * ) args[2];
  	  	for (unsigned i = 0; i < size; i++) {
  	  		buffer[i] = input_getc();
  	  	}
  	  	f->eax = size;
  	  } else {
  	  	void *buffer = (void *) args[2];
  	  	f->eax = read_helper(fd, buffer, size);
  	  }
  } else if (args[0] == SYS_SEEK) {
  	  int fd = args[1];
  	  unsigned position = args[2];
  	  seek_helper(fd, position);
  } else if (args[0] == SYS_TELL) {
  	  int fd = args[1];
  	  f->eax = tell_helper(fd);
  } else if (args[0] == SYS_CLOSE) {
  	  int fd = args[1];
  	  close_helper(fd); 
  }
}
