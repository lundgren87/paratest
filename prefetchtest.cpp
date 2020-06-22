#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <cassert>
#include <vector>
#include <ctime>
#include <atomic>
#include <getopt.h>

#include "argo/argo.hpp"


double *array;

struct thread_args {
	int mystart;
	int myend;
};

/* Fetch pages located on remote node */
void *work(void *argptr){
	thread_args * args = static_cast<thread_args*>(argptr);
	double temp;

	// Store some things locally
	int mystart = args->mystart;
	int myend = args->myend;
	int doubles_per_page = 4096/sizeof(double);

	// Do work on assigned array section
	for(int i=mystart; i<myend; i++){
		temp = array[i*doubles_per_page];
		assert(temp == 42.0);
	}
	return nullptr;
}


int main(int argc, char** argv){
	int opt;
	struct timespec t_start, t_stop;

	// Set some basic values
	unsigned int hwthreads = std::thread::hardware_concurrency();
	int nthreads = hwthreads/2;
	int num_nodes = 0;
	unsigned long num_pages = 65536;	//256Mb to be kind
	unsigned long iterations = 10;		//Not too many

	// Parse command line parameters
	while ((opt = getopt(argc, argv, "m:s:i:n:h")) != -1) {
		switch (opt) {
			case 's':
				num_pages = atol(optarg);
				if(num_pages > 0 && ((num_pages & (num_pages - 1)) == 0)){
					break;
				}else{
					fprintf(stderr, "[-s num_pages] must be >0 and power of 2.\n");
					exit(EXIT_FAILURE);
				}
			case 'i':
				iterations = atol(optarg);
				if(iterations > 0){
					break;
				}else{
					fprintf(stderr, "-i iterations] must be >0.\n");
					exit(EXIT_FAILURE);
				}
			case 'n':
				nthreads = atoi(optarg);
				if(nthreads > hwthreads){
					fprintf(stderr, "Warning, %u cores available but %d threads requested. "
							"Performance may suffer as a result.\n", hwthreads, nthreads);
				}
				if(nthreads > 0 && (num_pages%nthreads) == 0){
					break;
				}else{
					fprintf(stderr, "[-n threads must be >0 and divide num_pages (%lu).\n",
							num_pages);
					exit(EXIT_FAILURE);
				}
			case 'm':
				num_nodes = atoi(optarg);
				if(num_nodes > 1 && (num_pages%(num_nodes-1)) == 0){
					break;
				}else{
					fprintf(stderr, "[-m (nodes-1) must be >0 and divide num_pages (%lu).\n",
							num_pages);
					exit(EXIT_FAILURE);
				}
			case 'h':
				fprintf(stderr, "Usage: mpirun [-np nodes] %s [-s num_pages] [-i iterations]"
						"[-n threads] [-m nodes] [-h help]\n", argv[0]);
				exit(EXIT_FAILURE);
			default:
				fprintf(stderr, "Usage: mpirun [-np nodes] %s [-s num_pages] [-i iterations]"
						"[-n threads] [-m nodes] [-h help]\n", argv[0]);
				exit(EXIT_FAILURE);
		}
	}

	// On all nodes, init ArgoDSM and store some things locally
	std::size_t argo_size = num_pages*4096*num_nodes;
	std::size_t cache_size = argo_size/num_nodes;
	argo::init(argo_size, cache_size);

	assert(num_nodes == argo::number_of_nodes()); //Chicken before egg etc
	int node_id = argo::node_id();
	
	// Allocate ArgoDSM memory for arrays
	if(num_pages % nthreads != 0){
		fprintf(stderr, "Error: nthreads must divide num_pages.\n");
		exit(EXIT_FAILURE);
	}else if(num_nodes < 2){
		fprintf(stderr, "Error: This benchmark requires two or more ArgoDSM nodes.\n");
		exit(EXIT_FAILURE);
	}else{
		printf("[%d] ArgoDSM allocating memory.\n", node_id);
		array = argo::conew_array<double>(num_pages*(4096/sizeof(double)));
	}
	
	unsigned long chunksize = num_pages/nthreads;
	printf("[%d] ArgoDSM initialized.\n", node_id);

	// Set up thread data
	std::vector<pthread_t> threads(nthreads);
	std::vector<thread_args> args(nthreads);

	// Initialize ArgoDSM data
	argo::barrier();
	if(argo::node_id() == 0){
		printf("Initializing data.\n");
		for(int i = 0; i < num_pages*(4096/sizeof(double)); i++){
			array[i] = 42.0;
		}
		printf("\nStarting prefetchtest with parameters:\n"
				"\tArgoDSM nodes:\t\t%4d"
				"\tNumber of pages:\t%12lu\n"
				"\tThreads per node:\t%4d"
				"\tIterations:\t\t%12lu\n"
				"\tHWthreads per node:\t%4d",
				num_nodes, num_pages, nthreads, iterations, hwthreads);
	}

	// Distribute initialization and wait for other nodes
	argo::barrier();
	if(node_id == 0){
		// Start timekeeping on node 0
		printf("\nRunning throughput benchmark...\n");
		clock_gettime(CLOCK_MONOTONIC, &t_start);
	}
	
	for(int i = 0; i < iterations; i++){
		if(node_id == 0){
			printf("Running iteration: %d\n", i+1);
		}else{
			//Set up work threads
			for(int j = 0; j < nthreads; j++){
				args[i].mystart = chunksize*i;
				args[i].myend = args[i].mystart+chunksize;
				pthread_create(&threads[i], nullptr, work, &args[i]);
			}

			// Join threads
			for (auto &t : threads) pthread_join(t, nullptr);
		}
		// Wait for other nodes
		argo::barrier();
	}

	// Check results
	if(node_id == 0){
		printf("\nExecution completed. Calculating stats.\n");
		clock_gettime(CLOCK_MONOTONIC, &t_stop);
		double exectime = (t_stop.tv_sec - t_start.tv_sec)*1e3 +
			(t_stop.tv_nsec - t_start.tv_nsec)*1e-6;
		unsigned long num_ops = num_pages * iterations;
		double mb_per_s = ((num_ops*4096)/(exectime*1e-3))/(1024*1024);
		double time_per_load = (exectime*1e3)/num_ops;
		printf("\nThroughput test completed..\n"
				"\tPages loaded:\t%12lu\n"
				"\tExec time:\t%10.02fms\n"
				"\tMB/s:\t\t%12.02f\n"
				"\tAvg load time:\t%10.02fµs\n\n",
				(unsigned long)num_pages, exectime, mb_per_s, time_per_load);
	}

	// Delete necessary things and finalize ArgoDSM
	argo::barrier();
	argo::codelete_array(array);
	argo::finalize();
}
