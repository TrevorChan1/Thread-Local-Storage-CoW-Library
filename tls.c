#include "tls.h"
#include <signal.h>
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define MAX_NUM_THREADS 128


// [[[*** Section 1: Defining data structures ***]]]

// TLS: Maps tid to local storage and locations
typedef struct thread_local_storage {
	pthread_t tid;
	unsigned int size;	// Size in bytes
	unsigned int page_num;	// Number of pages
	struct page ** pages;	// Array of pointers to pages
} TLS;

// Page: States address of page and number of threads using that specific page
struct page {
	unsigned int address;	// Start address of page
	int ref_count; 	// Counter for number of shared pages
};

// Mappings between tid and TLS (used to find TLS)
struct tid_tls_pair {
	pthread_t tid;
	TLS *tls;
};


// [[[*** Section 2: Defining global variables ***]]]

// Global datastructure of tid_tls_pairs (with arbitrary MAX_NUM_THREADS)
static TLS *tls_array[MAX_NUM_THREADS];
int page_size;


// [[[*** Section 3: Defining helper functions ***]]]

void tls_handle_page_fault(int sig, siginfo_t *si, void *context){
	// p_fault = ((unsigned int) si->si_addr) & ~(page_size - 1);
}

// Helper function that checks if the inputted thread has an existing tls_array entry
int lsa_exists(pthread_t tid) {
	int index = (int) tid;
	if (tls_array[index] == NULL)
		return 0;
	else
		return 1;
}

// Helper function that protects the current thread's LSA
void tls_protect(struct page *p){

	// Check if it was able to successfully protect the page
	if (mprotect((void*) p->address, page_size, PROT_NONE)) {
		fprintf(stderr, "ERROR: Failed to protect memory area page\n");
		exit(1);
	}
}

// Helper function that protects the current thread's LSA
void tls_unprotect(struct page *p){

	// Check if it was able to successfully unprotect the page (allow read / write)
	if (mprotect((void*) p->address, page_size, PROT_READ | PROT_WRITE)) {
		fprintf(stderr, "ERROR: Failed to unprotect memory area page\n");
		exit(1);
	}
}

// Function to be called on first call of tls_create to initialize datastructures and signal handlers
void tls_init() {

	// Initialize signal handler
	struct sigaction sigact;
	page_size = sysconf(_SC_PAGE_SIZE);
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = SA_SIGINFO;
	sigact.sa_sigaction = tls_handle_page_fault;
	sigaction(SIGBUS, &sigact, NULL);
	sigaction(SIGSEGV, &sigact, NULL);

	// Initialize global variables
	for (int i = 0; i < MAX_NUM_THREADS; i++){
		tls_array[i] = NULL;
	}

}


// [[[*** Section 4: Defining library functions ***]]]

// Function to create a TLS for the current thread
int tls_create(unsigned int size)
{
	// On first call initialize the data structures and signal handler
	static int is_first_call = 1;
	if (is_first_call) {
		is_first_call = 0;
		tls_init();
	}
	// Check if the current thread already has an LSA
	if (lsa_exists(pthread_self())){
		printf("ERROR: LSA already exists\n");
		return -1;
	}
	// If there it has LSA, check if the size is nonzero
	else {
		if (tls_array[(int) pthread_self()]->size > 0) {
			printf("ERROR: Current thread has non-zero storage\n");
			return -1;
		}
	}

	// Initialize the TLS for the current thread
	TLS * tls = (TLS *) malloc(sizeof(TLS));
	tls->page_num = 0;
	tls->pages = NULL;
	tls->size = size;
	tls->tid = pthread_self();

	// Initialize each page for the TLS
	int num_pages = size / page_size;
	if (size % page_size) num_pages++;

	// If size is 0, then leave pages as NULL in case want to clone!
	if (num_pages > 0){
		// Initialize pages array (only need enough to store num_pages pages since never changes)
		tls->pages = (struct page **) malloc(num_pages * sizeof(struct page));
		for (int i = 0; i < num_pages; i++){
			// Allocate memory for the page and initialize all values
			tls->pages[i] = (struct page *) malloc(sizeof(struct page));
			tls->pages[i]->address = mmap(0, page_size, PROT_NONE, MAP_ANON | MAP_PRIVATE, 0, 0);
			tls->pages[i]->ref_count = 1;
		}
		tls->page_num = num_pages;
	}

	// Set the current thread's value in the area to be the TLS
	tls_array[(int) pthread_self()] = tls;

	return 0;
}

// Function that destroys the current thread's TLS (decrements all reference counts and deletes if 0)
int tls_destroy()
{
	// Check if current thread has LSA
	if (!lsa_exists(pthread_self())){
		printf("ERROR: Current thread has no LSA\n");
		return -1;
	}

	// Iterate through all pages
	TLS * tls = tls_array[(int) pthread_self()];
	for (int i = 0; i < tls->page_num; i++){
		// Decrement the reference count (in case other threads point to it)
		tls->pages[i]->ref_count--;
		// If there are no more threads pointing at the page, free that page and the memory it points to
		if (tls->pages[i]->ref_count == 0){
			if (munmap(tls->pages[i]->address, page_size)){
				printf("ERROR: Failed to free page\n");
				return -1;
			}
			free(tls->pages[i]);
		}
		// Free the array of pages
		free(tls->pages);
	}

	// After freeing all the pages that need to be freed, free the TLS itself
	free(tls);
	tls_array[(int) pthread_self()] = NULL;

	return 0;
}

// Function that reads length bytes from the thread's TLS into buffer
int tls_read(unsigned int offset, unsigned int length, char *buffer)
{
	// Check if current thread has LSA
	if (!lsa_exists(pthread_self())){
		printf("ERROR: Current thread has no LSA\n");
		return -1;
	}

	// Check if the TLS is large enough to read all bytes to buffer
	if (tls_array[(int) pthread_self()]->size < (offset + length)){
		printf("ERROR: Read size larger than LSA size or offset out of range\n");
	}

	TLS * tls = tls_array[(int) pthread_self()];

	// Initialize variables for performing read
	int page_index = offset / page_size;
	int page_offset = offset % page_size;
	int num_pages_read = length / page_size;
	int bytes_read = 0;
	int bytes_left = length;
	if ((length + page_offset) / page_size) num_pages_read++;
	if ((length + page_offset) % page_size) num_pages_read++;

	// Iterate through all necessary pages and read into buffer
	for (int i = page_index; i < page_index + num_pages_read; i++){
		// Unprotect the page so it can be read from
		tls_unprotect(tls->pages[i]);

		// Initialize how many bytes to read this iteration
		int this_read = 0;
		if (bytes_left + page_offset >= page_size)
			this_read = page_size - page_offset;
		else
			this_read = bytes_left;
		
		// Copy this_read bytes into the buffer
		memcpy(buffer + bytes_read, tls->pages[i]->address + page_offset, this_read);

		// Prepare for the next iteration
		bytes_read += this_read;
		bytes_left -= this_read;
		if (page_offset) page_offset = 0;

		// Reprotect this page
		tls_protect(tls->pages[i]);

	}

	return 0;
}

// Function that writes length bytes from buffer into the thread's TLS
int tls_write(unsigned int offset, unsigned int length, const char *buffer)
{
	// Check if current thread has LSA
	if (!lsa_exists(pthread_self())){
		printf("ERROR: Current thread has no LSA\n");
		return -1;
	}

	// Check if the TLS is large enough to read all bytes to buffer
	if (tls_array[(int) pthread_self()]->size > (offset + length)){
		printf("ERROR: Write size larger than LSA can hold\n");
	}

	TLS * tls = tls_array[(int) pthread_self()];

	// Initialize variables for performing read
	int page_index = offset / page_size;
	int page_offset = offset % page_size;
	int num_pages_write = length / page_size;
	int bytes_written = 0;
	int bytes_left = length;
	if ((length + page_offset) / page_size) num_pages_write++;
	if ((length + page_offset) % page_size) num_pages_write++;

	// Iterate through all necessary pages and read into buffer
	for (int i = page_index; i < page_index + num_pages_write; i++){
		// Unprotect the page so it can be read from
		tls_unprotect(tls->pages[i]);

		// Initialize how many bytes to read this iteration
		int this_write = 0;
		if (bytes_left + page_offset >= page_size)
			this_write = page_size - page_offset;
		else
			this_write = bytes_left;
		
		// Copy this_read bytes into the buffer
		memcpy(tls->pages[i]->address + page_offset, buffer + bytes_written, this_write);

		// Prepare for the next iteration
		bytes_written += this_write;
		bytes_left -= this_write;
		if (page_offset) page_offset = 0;

		// Reprotect this page
		tls_protect(tls->pages[i]);

	}

	return 0;
}

// Function that clones tid's LSA for the current thread
int tls_clone(pthread_t tid)
{
	// Check if the current thread already has an LSA
	if (lsa_exists(pthread_self())){
		printf("ERROR: Current thread already has an LSA\n");
		return -1;
	}

	// Check if inputted thread has LSA
	if (!lsa_exists(tid)){
		printf("ERROR: Inputted thread has no LSA\n");
		return -1;
	}

	return 0;
}
