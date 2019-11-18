#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <pthread.h>
#include <cassert>
#include <vector>
#include <ctime>
#include <atomic>

#include "argo/argo.hpp"


double *in_array;
double *out_array;
int gnthreads;
std::atomic<double> *progvec;

struct thread_args {
    int tid;
    int arraysize;
    int mystart;
    int myend;
    int iterations;
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
        for(j=0; j<100; j++){
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
        if(((t % numnodes) == nodeid) && (t != lastprint)){
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
}

int main(int argc, char** argv){
    int i, nodeid, nnodes, nthreads, arraysize, chunksize, nodesize, iterations;
    struct timespec t_start, t_stop;
    double initvalue, alpha;

    // TODO: maybe allow user input?

    printf("Setting up ArgoDSM.\n");
    // On all nodes, init and allocate some data
    argo::init(1024*1024*1024UL, 1024*1024*1024UL);

    // Store some things locally
    nodeid = argo::node_id();
    nnodes = argo::number_of_nodes();
    nthreads = 8;
    gnthreads = nthreads;
    arraysize = 67108864;   //1GB
    //arraysize = 8388608;  //128MB
    //arraysize = 1048576;  //32MB
    nodesize = arraysize/nnodes;
    chunksize = arraysize/(nthreads*nnodes);
    initvalue = 21.0;
    alpha = 2.0;
    iterations = 100;

    // Allocate ArgoDSM memory for arrays
    in_array = argo::conew_array<double>(arraysize);
    out_array = argo::conew_array<double>(arraysize);

    // Set up thread data
    std::vector<pthread_t> threads(nthreads);
    std::vector<thread_args> args(nthreads);
    pthread_t progthread;
    progvec = new std::atomic<double> [nthreads];
    for(i=0; i<nthreads; i++){
        progvec[i] = 0.0;
    }

    // Initialize ArgoDSM data
    if(argo::node_id() == 0){
        printf("Initializing data.\n");
    }
    for(i=nodeid*nodesize; i < (nodeid+1)*nodesize; i++){
        in_array[i]=initvalue;
        out_array[i]=0;
    }

    // Distribute initialization and wait for other nodes
    argo::barrier();
    if(nodeid == 0){
        printf("Starting paratest with %d ArgoDSM nodes.\n", nnodes);
        // Start timekeeping on node 0
        printf("Running Paratest-nocontention.\n");
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
        printf("Checking results...\n");
        clock_gettime(CLOCK_MONOTONIC, &t_stop);
        for(i=0; i<arraysize; i++){
            assert(out_array[i]==(initvalue*alpha)*100);
        }
        printf("Done checking.\n");
        double time = (t_stop.tv_sec - t_start.tv_sec)*1e3 +
            (t_stop.tv_nsec - t_start.tv_nsec)*1e-6;
        double numops = (double)arraysize * (double)iterations * 2.0;
        double mflops = (numops/(time*10e-3))*10e-6;
        printf("Paratest-nocontention SUCCESSFUL.\n"
                "Time: %.02fms MFLOPS: %.02f\n", time, mflops);
    }

    // Delete necessary things and finalize ArgoDSM
    argo::barrier();
    argo::codelete_array(in_array);
    argo::codelete_array(out_array);
    argo::finalize();
}
