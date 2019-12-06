#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <unistd.h>
#include <cassert>
#include <vector>

#include "argo/argo.hpp"

double *in_array;
double *out_array;

int main(int argc, char** argv){
	int i, nodeid, nnodes;
	int arraysize, nodesize, argosize;
	double initvalue, alpha;

	// Set some basic values
	arraysize = 4096;
	initvalue = 21.0;
	alpha = 2.0;

	argosize = arraysize*sizeof(double)*2;
	argo::init(argosize+8192, argosize/2);

	// Store some things locally
	nodeid = argo::node_id();
	nnodes = argo::number_of_nodes();
	nodesize = arraysize/nnodes;

	// Allocate memory for arrays
	in_array = argo::conew_array<double>(arraysize);
	out_array = argo::conew_array<double>(arraysize);

	// Initialize ArgoDSM data, one section of each array per node
	if(nodeid == 0) printf("Initializing data...\n");
	argo::barrier();
	for(i = nodeid*nodesize; i < (nodeid+1)*nodesize; i++){
		in_array[i]=initvalue;
		out_array[i]=0;
	}

	// Distribute initialization and wait for other nodes
	argo::barrier();
	if(nodeid == 0) printf("Executing...\n");

	// Calculate and store results for one section of out_array per node
	for(i = nodeid*nodesize; i < (nodeid+1)*nodesize; i++){
		out_array[i] = in_array[i]*alpha;
	}

	// Wait for other nodes
	argo::barrier();

	// Check results	
	if(nodeid == 0){
		printf("Checking results...\n");
		for(i=0; i<arraysize; i++){
			if(out_array[i]!=initvalue*alpha){
				printf("Test failure: out_array[%d]=%.01f (should be %.01f).\n",
						i, out_array[i], initvalue*alpha);
				exit(EXIT_FAILURE);
			}
		}
		printf("\nTest successful!.\n");
	}

	// Delete necessary things and finalize ArgoDSM
	argo::barrier();
	argo::codelete_array(in_array);
	argo::codelete_array(out_array);
	argo::finalize();
}
