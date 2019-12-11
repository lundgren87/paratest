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


double *in_array;
double *out_array;
int gnthreads;
std::atomic<double> *progvec;

struct thread_args {
	int tid;
	unsigned long arraysize;
	int mystart;
	int myend;
	unsigned long iterations;
	double alpha;
};

/* Perform update on partial array */
void *work(void *argptr){
	thread_args * args = static_cast<thread_args*>(argptr);
	int i, j, tid, mystart, myend, nodeid, iterations;
	double alpha, temp;

	// Store some things locally
	tid = args->tid;
	mystart = args->mystart;
	myend = args->myend;
	alpha = args->alpha;
	iterations = args->iterations;
	nodeid = argo::node_id();

	// Do work on assigned array section
	for(i=mystart; i<myend; i++){
		for(j=0; j<iterations; j++){
			out_array[i] += in_array[i]*alpha;
		}
		// Update progress occasionally
		if(i%500){
			temp = (double)(i-mystart)/(double)(myend-mystart);
			progvec[tid-(gnthreads*nodeid)].store(temp, std::memory_order_release);
		}
	}
	// Ensure progress is at 1.0 when done
	progvec[tid-(gnthreads*nodeid)].store(1.0, std::memory_order_release);

	return nullptr;
}

/* Progress tracker to be run in separate thread */
void *ptr(void *argptr){
	int i, t;
	struct timespec t_cur, t_start;
	int done[gnthreads] = {0};
	int nodeid = argo::node_id();
	int numnodes = argo::number_of_nodes();
	int success = 0;
	int lastprint = -1;
	double progress;

	clock_gettime(CLOCK_MONOTONIC, &t_start);
	// Loop until all threads are done
	while(1){
		// Check if all threads are done
		for(i=0; i<gnthreads; i++){
			if(done[i]<1) break;
			success = 1;
		}
		// If done, break to exit
		if(success) break;

		// Else, check if progress should be printed
		clock_gettime(CLOCK_MONOTONIC, &t_cur);
		t = t_cur.tv_sec-t_start.tv_sec;
		if(((t % (numnodes+1)) == (nodeid+1)) && (t != lastprint)){
			lastprint = t;
			// Print progress for each thread on this node
			printf("[%d] [", nodeid);
			for(i=0; i<gnthreads; i++){
				progress = progvec[i].load(std::memory_order_acquire);
				if(progress==1) done[i]=1;
				printf(" %3.0f%%", 100*progress);
			}
			printf("]\n");
		}
		pthread_yield();
	}
	printf("[%d] COMPLETED.\n", nodeid);
	return nullptr;
}

int main(int argc, char** argv){
	int i, opt, nodeid, nnodes, nthreads;
	unsigned long arraysize, chunksize, nodesize, iterations, argosize;
	unsigned int hwthreads;
	struct timespec t_start, t_stop, t_init;
	double initvalue, alpha;

	// Set some basic values
	hwthreads = std::thread::hardware_concurrency();
	nthreads = hwthreads/2;
	arraysize = 67108864;   //1GB
	initvalue = 21.0;
	alpha = 2.0;
	iterations = 500;

	// Parse command line parameters
	while ((opt = getopt(argc, argv, "s:i:a:n:m:h")) != -1) {
		switch (opt) {
			case 's':
				arraysize = atol(optarg);
				if(arraysize > 0 && ((arraysize & (arraysize - 1)) == 0)){
					break;
				}else{
					fprintf(stderr, "[-s arraysize] must be >0 and power of 2.\n");
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
			case 'a':
				alpha = atof(optarg);
				break;
			case 'n':
				nthreads = atoi(optarg);
				if(nthreads > hwthreads){
					fprintf(stderr, "Warning, %u cores available but %d threads requested. "
							"Performance may suffer as a result.\n", hwthreads, nthreads);
				}
				if(nthreads > 0 && (arraysize%nthreads) == 0){
					break;
				}else{
					fprintf(stderr, "[-n threads must be >0 and divide arraysize (%lu).\n",
							arraysize);
					exit(EXIT_FAILURE);
				}
			case 'h':
				fprintf(stderr, "Usage: mpirun [-np nodes] %s [-s arraysize] [-i iterations]"
						"[-a alpha] [-n threads] [-h help]\n", argv[0]);
				exit(EXIT_FAILURE);
			default:
				fprintf(stderr, "Usage: mpirun [-np nodes] %s [-s arraysize] [-i iterations]"
						"[-a alpha] [-n threads] [-h help]\n", argv[0]);
				exit(EXIT_FAILURE);
		}
	}

	// On all nodes, init ArgoDSM and store some things locally
	argosize = arraysize*sizeof(double)*2;
	argo::init(argosize, argosize);

	gnthreads = nthreads;
	nodeid = argo::node_id();
	nnodes = argo::number_of_nodes();
	nodesize = arraysize/nnodes;
	chunksize = arraysize/(nthreads*nnodes);
	printf("[%d] ArgoDSM initialized.\n", nodeid);

	// Allocate ArgoDSM memory for arrays
	if(arraysize % (nnodes*nthreads) != 0){
		fprintf(stderr, "Error: nnodes*nthreads must divide arraysize.\n");
		exit(EXIT_FAILURE);
	}else{
		printf("[%d] ArgoDSM allocating memory.\n", nodeid);
		in_array = argo::conew_array<double>(arraysize);
		out_array = argo::conew_array<double>(arraysize);
	}

	// Set up thread data
	std::vector<pthread_t> threads(nthreads);
	std::vector<thread_args> args(nthreads);
	pthread_t progthread;
	progvec = new std::atomic<double> [nthreads];
	for(i=0; i<nthreads; i++){
		progvec[i] = 0.0;
	}

	// Initialize ArgoDSM data
	argo::barrier();
	if(argo::node_id() == 0){
		clock_gettime(CLOCK_MONOTONIC, &t_init);
		printf("\nStarting paratest with parameters:\n"
				"\tArgoDSM nodes:\t\t%4d"
				"\tArraysize:\t%12lu\n"
				"\tThreads per node:\t%4d"
				"\tIterations:\t%12lu\n"
				"\tHWthreads per node:\t%4d"
				"\tDaxpy alpha:\t%12f\n",
				nnodes, arraysize, nthreads, iterations, hwthreads, alpha);
		printf("\nInitializing data...\n");
	}
	for(i=nodeid*nodesize; i < (nodeid+1)*nodesize; i++){
		if(i%(arraysize/16)==0){
			printf("[%d] Initialized %.02f%% of my assignment...\n",
					nodeid,
					((double)(i-(nodeid*nodesize))/(double)(nodesize))*100);
		}
		in_array[i]=initvalue;
		out_array[i]=0;
	}
	printf("[%d] Done initializing.\n", nodeid);

	// Distribute initialization and wait for other nodes
	argo::barrier();
	if(nodeid == 0){
		// Start timekeeping on node 0
		printf("\nRunning Paratest-nocontention...\n");
		clock_gettime(CLOCK_MONOTONIC, &t_start);
	}

	//Set up work threads
	for(i=0; i<nthreads; i++){
		args[i].tid = nodeid*nthreads + i;
		args[i].mystart = chunksize*(nodeid*nthreads+i);
		args[i].myend = args[i].mystart+chunksize;
		args[i].iterations = iterations;
		args[i].arraysize = arraysize;
		args[i].alpha = alpha;
		pthread_create(&threads[i], nullptr, work, &args[i]);
	}
	pthread_create(&progthread, nullptr, ptr, nullptr);

	// Join threads
	for (auto &t : threads) pthread_join(t, nullptr);
	pthread_cancel(progthread);

	// Wait for other nodes
	argo::barrier();

	// Check results
	if(nodeid == 0){
		printf("\nExecution completed. Checking results...\n");
		clock_gettime(CLOCK_MONOTONIC, &t_stop);
		for(i=0; i<arraysize; i++){
			if(out_array[i]!=(initvalue*alpha)*iterations){
				printf("ERROR: out_array[%d]==%.2f (should be %.2f).\n",
						i, out_array[i], (initvalue*alpha)*iterations);
				exit(EXIT_FAILURE);
			}

		}
		double inittime = (t_start.tv_sec - t_init.tv_sec)*1e3 +
			(t_start.tv_nsec - t_init.tv_nsec)*1e-6;
		double exectime = (t_stop.tv_sec - t_start.tv_sec)*1e3 +
			(t_stop.tv_nsec - t_start.tv_nsec)*1e-6;
		double numops = (double)arraysize * (double)iterations * 2.0;
		double mflops = (numops/(exectime*10e-3))*10e-6;
		printf("\nParatest-nocontention SUCCESSFUL.\n"
				"\tInit time:\t%10.02fms"
				"\tOperations:\t%12lu\n"
				"\tExec time:\t%10.02fms"
				"\tMFLOPS:\t\t%12.02f\n\n", inittime, (unsigned long)numops, exectime, mflops);
	}

	// Delete necessary things and finalize ArgoDSM
	argo::barrier();
	argo::codelete_array(in_array);
	argo::codelete_array(out_array);
	argo::finalize();
	delete progvec;
}
