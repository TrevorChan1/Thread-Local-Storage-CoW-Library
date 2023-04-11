# Thread Local Storage Library
This repository consists of a C-based Thread Local Storage (TLS) library that implements copy on write. The basis around this library is to implement functions that allow individual threads to have their own private storage that can't be accessed by other threads (doing so will result in a segmentation fault). These threads can then clone other threads' storage and write to, read from, and delete their own storage. Threads can copy other threads' TLS (cloning it as their own), which will reference the same page in memory until a write is done.

The library implements storage as pages utilizing the mmap and mprotect functions in libc. This means all memory will be page-aligned and protected (any read or writes outside of tls_read and tls_write will cause a segmentation fault to prevent threads from accessing TLS's that they don't own).

## Functions
The TLS library implements a variety of functions creating and modifying the current thread's TLS.

### tls_create(size)
Function initializes a Thread Local Storage for the current thread of size *size* rounded up to the nearest number of pages. On the first run, it will initialize global variables like an array of tid and tls pairings to prepare for subsequent uses of the function. Returns 0 on a success and a -1 on a failure (invalid size or if the thread already has a TLS)

### tls_destroy()
Destroys the current thread's TLS (if it has one), decrementing reference counts to the pages it points to (freeing if reference count is at 0) and freeing any related data structures used by the TLS. Returns a 0 on a success and a -1 on a failure (does not have a TLS).

### tls_read(offset, length, buffer)
Reads *length* bytes from the current thread's TLS located at *offset* into the inputted *buffer*. This function goes page by page unprotecting, reading, then reprotecting the necessary pages used in reading. Returns a 0 on a success and a -1 on a failure (offset + length > TLS size, or if thread has no TLS).

### tls_write(offset, length, buffer)
Writes *length* bytes into the current thread's TLS starting at *offset* from the inputted *buffer*. This function goes page by page unprotecting, writing, then reprotecting the necessary pages used in writing. Returns a 0 on a success and a -1 on a failure (offset + length > TLS size, or if thread has no TLS).

If the reference count of any of the pages it writes to is >1, the tls_write will implement *Copy on Write*. This means that it will first create a copy of the page (decrementing the reference count of the previous, shared page), allocating memory for the new page and setting the reference count to 1. The function will then continue to write to the newly created page as normal.

### tls_clone(tid)
Clones the TLS of the thread identified using its tid for the current thread. Allocates a new data structure for the current thread's pages, but overall all of the pages it points to are shared (incrementing the reference count). Basically, cloned TLS's reference the same pages to save memory, then any write to a page with multiple references will copy to a newly allocated page before writing. Returns 0 on success and -1 on failure (current thread already has a TLS or thread *tid* doesn't have a TLS)

Didn't use any external sources other than the slides.