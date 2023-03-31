#include "tls.h"
#include <signal.h>
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



// [[[*** Section 3: Defining helper functions ***]]]

void tls_handle_page_fault(int sig, siginfo_t *si, void *context){
	// p_fault = ((unsigned int) si->si_addr) & ~(page_size - 1);
}

// Helper function that checks if the current thread has an existing tls_array entry
int lsa_exists() {
	int tid = (int) pthread_self();
	if (tls_array[tid] == NULL)
		return 0;
	else
		return 1;
}

// Function to be called on first call of tls_create to initialize datastructures and signal handlers
void tls_init() {

	// Initialize signal handler
	struct sigaction sigact;
	// page_size = getpagesize();
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

int tls_create(unsigned int size)
{
	// On first call initialize the data structures and signal handler
	static int is_first_call = 1;
	if (is_first_call) {
		is_first_call = 0;
		tls_init();
	}

	

	return 0;
}

int tls_destroy()
{
	return 0;
}

int tls_read(unsigned int offset, unsigned int length, char *buffer)
{
	return 0;
}

int tls_write(unsigned int offset, unsigned int length, const char *buffer)
{
	return 0;
}

int tls_clone(pthread_t tid)
{
	return 0;
}
