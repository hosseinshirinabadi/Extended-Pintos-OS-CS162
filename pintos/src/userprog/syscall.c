#include "userprog/syscall.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "lib/user/syscall.h"
#include "userprog/process.h"

static void syscall_handler (struct intr_frame *);
static open_file *get_file_by_fd (int fd);
static bool create_helper (const char *file, unsigned initial_size);
static void close_helper(int fd);
static unsigned tell_helper(int fd);
static void seek_helper(int fd, unsigned position);
static int read_helper(int fd, void *buffer, unsigned size);
static bool remove_helper(const char *file_name);
static int write_helper (int fd, const void *buffer, unsigned size);
static int filesize_helper (int fd);
static int open_helper (const char *file);
static bool validate_arg (void *arg);


// global lock for file system level
struct lock flock;

// finds the open file of the current thread that matches fd
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
		lock_release(&flock);
		return fd;
	} else {
		lock_release(&flock);
		return -1;
	}
}

int filesize_helper (int fd) {
	lock_acquire(&flock);
	open_file *found_file = get_file_by_fd(fd);
	if (found_file) {
		int length = file_length (found_file->file);
		lock_release(&flock);
		return length;
	} else {
		lock_release(&flock);
		return -1;
	}
}

int write_helper (int fd, const void *buffer, unsigned size) {
	lock_acquire(&flock);
	open_file *file = get_file_by_fd(fd);
	if (file) {
		int bytes_written = file_write(file->file, (const void * ) buffer, size);
		lock_release(&flock);
		return bytes_written;
	} else {
		lock_release(&flock);
		return 0;
	}
}

bool remove_helper (const char *file_name) {
	lock_acquire(&flock);
	bool success = filesys_remove(file_name);
	lock_release(&flock);
	return success;
}

int read_helper (int fd, void *buffer, unsigned size) {
	lock_acquire(&flock);
	open_file *file = get_file_by_fd(fd);
	if (file) {
		int bytes_read = file_read(file->file, buffer, size);
		lock_release(&flock);
		return bytes_read;
	} else {
		lock_release(&flock);
		return -1;
	}
}

void seek_helper (int fd, unsigned position) {
	lock_acquire(&flock);
	open_file *file = get_file_by_fd(fd);
	if (file) {
		file_seek (file->file, position);
		lock_release(&flock);
	} else {
		lock_release(&flock);
	}
}

unsigned tell_helper (int fd) {
	lock_acquire(&flock);
	open_file *file = get_file_by_fd(fd);
	if (file) {
		unsigned position = file_tell(file->file);
		lock_release(&flock);
		return position;
	} else {
		lock_release(&flock);
		return -1;
	}
}

void close_helper (int fd) {
	lock_acquire(&flock);
	open_file *file = get_file_by_fd(fd);
	if (file) {
		list_remove(&file->elem);
		file_close(file->file);
		free(file);
		lock_release(&flock);
	} else {
		lock_release(&flock);
	}
}

bool validate_arg (void *arg) {
	struct thread *current_thread = thread_current ();
  uint32_t *ptr = (uint32_t *) arg;
  return arg != NULL && is_user_vaddr(ptr) && pagedir_get_page (current_thread->pagedir, ptr) != NULL;
}


void
syscall_init (void)
{
  lock_init(&flock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  uint32_t* args = ((uint32_t*) f->esp);

  if (!validate_arg(args) || !validate_arg((args + 1)) || 
      !validate_arg((args + 2)) || !validate_arg((args + 3))) {
  	    f->eax = -1;
        printf ("%s: exit(%d)\n", &thread_current ()->name, -1);
        thread_exit ();
  }

  /*
   * The following print statement, if uncommented, will print out the syscall
   * number whenever a process enters a system call. You might find it useful
   * when debugging. It will cause tests to fail, however, so you should not
   * include it in your final submission.
   */

  // printf("System call number: %d\n", args[0]);

  if (args[0] == SYS_EXIT) {
      f->eax = args[1];
      struct thread *cur = thread_current();
      if (cur->parent_thread != NULL) {
        child *child_status = find_child(cur->parent_thread, cur->tid);
        child_status->exit_code = args[1];
      }
      printf ("%s: exit(%d)\n", &thread_current ()->name, args[1]);
      thread_exit ();
  }
  else if (args[0] == SYS_HALT) {
      shutdown_power_off();

  } else if (args[0] == SYS_PRACTICE) {
      f->eax = args[1] + 1; 

  } else if (args[0] == SYS_EXEC) {
  	  const char *cmd_line = args[1];
  	  if (!validate_arg(cmd_line) || !validate_arg(cmd_line + 1)) {
        f->eax = -1;
        printf ("%s: exit(%d)\n", &thread_current ()->name, -1);
        thread_exit ();
  	  } else {
        f->eax = process_execute(cmd_line);
  	  }

  } else if (args[0] == SYS_WAIT) {
  	  pid_t pid = args[1];
      f->eax = process_wait(pid);

  } else if (args[0] == SYS_CREATE) {
  	  const char *file = (char * ) args[1];
  	  unsigned initial_size = args[2];
  	  if (!validate_arg(file)) {
  	    f->eax = -1;
	      printf ("%s: exit(%d)\n", &thread_current ()->name, -1);
	      thread_exit ();
  	  } else {
  	  	f->eax = create_helper(file, initial_size);
  	  }
  	  
  } else if (args[0] == SYS_WRITE) {
      int fd = args[1];
      const void *buffer = (void *) args[2];
      unsigned size = args[3];
      if (fd < 0 || !validate_arg(buffer)) {
      	f->eax = -1;
	      printf ("%s: exit(%d)\n", &thread_current ()->name, -1);
	      thread_exit ();
      } else {
      	if (fd == 1) {
	      	lock_acquire(&flock);
	      	putbuf ((const char *) args[2], size);
	      	lock_release(&flock);
	      	f->eax = size;
	      } else {
	      	f->eax = write_helper(fd, buffer, size);
	      }
      }

  } else if (args[0] == SYS_OPEN) {
  	  const char *file_name = (char *) args[1];
  	  if (validate_arg(file_name)) {
  	  	  f->eax = open_helper(file_name);
  	  } else {
  	  	  f->eax = -1;
	        printf ("%s: exit(%d)\n", &thread_current ()->name, -1);
	        thread_exit ();
  	  }

  } else if (args[0] == SYS_REMOVE) {
  	  const char *file_name = (char * ) args[1];
  	  if (validate_arg(file_name)) {
  	  	f->eax = remove_helper(file_name);
  	  } else {
  	  	f->eax = -1;
	      printf ("%s: exit(%d)\n", &thread_current ()->name, -1);
	      thread_exit ();
  	  }

  } else if (args[0] == SYS_FILESIZE) {
  	  int fd = args[1];
  	  if (fd < 0) {
  	  	f->eax = -1;
	      printf ("%s: exit(%d)\n", &thread_current ()->name, -1);
	      thread_exit ();
  	  } else {
  	  	f->eax = filesize_helper(fd);
  	  }

  } else if (args[0] == SYS_READ) {
  	  int fd = args[1];
  	  unsigned size = args[3];
  	  if (!validate_arg((char *) args[2])) {
		    f->eax = -1;
	      printf ("%s: exit(%d)\n", &thread_current ()->name, -1);
	      thread_exit ();
  	  } else {
  	  	if (fd == 0) {
  	      lock_acquire(&flock);
  	  	  uint8_t *buffer = (uint8_t * ) args[2];
  	  	  for (unsigned i = 0; i < size; i++) {
  	  		  buffer[i] = input_getc();
  	  	  }
  	  	  lock_release(&flock);
  	  	  f->eax = size;
  	    } else if (fd > 0) {
  	  	  void *buffer = (void *) args[2];
  	  	  f->eax = read_helper(fd, buffer, size);
  	    } else {
  	  	  f->eax = -1;
	        printf ("%s: exit(%d)\n", &thread_current ()->name, -1);
	        thread_exit ();
  	    }
  	  }

  } else if (args[0] == SYS_SEEK) {
  	  int fd = args[1];
  	  unsigned position = args[2];
  	  if (fd < 0) {
  	  	f->eax = -1;
	      printf ("%s: exit(%d)\n", &thread_current ()->name, -1);
	      thread_exit ();
  	  } else {
  	  	seek_helper(fd, position);
  	  }

  } else if (args[0] == SYS_TELL) {
  	  int fd = args[1];
  	  if (fd < 0) {
  	  	f->eax = -1;
	      printf ("%s: exit(%d)\n", &thread_current ()->name, -1);
	      thread_exit ();
  	  } else {
  	  	f->eax = tell_helper(fd);
  	  }

  } else if (args[0] == SYS_CLOSE) {
  	  int fd = args[1];
  	  if (fd < 0) {
  	  	f->eax = -1;
	      printf ("%s: exit(%d)\n", &thread_current ()->name, -1);
	      thread_exit ();
  	  } else {
  	  	close_helper(fd); 
  	  }
  	  
  } else {
  	f->eax = -1;
	  printf ("%s: exit(%d)\n", &thread_current ()->name, -1);
	  thread_exit ();
  }
}
