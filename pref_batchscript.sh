#! /bin/bash -l
#SBATCH -J prefetchtest
#SBATCH -p cluster
#SBATCH -t 10:00
#SBATCH -o output.out
#SBATCH -N 2

NNODES=2
ITERATIONS=1
#NUM_PAGES=1024
#NUM_PAGES=16384
#NUM_PAGES=262144
NUM_PAGES=524288

echo SETTING UP ENVIRONMENT
export ARGO_LOAD_SIZE=16
echo ENVIRONMENT SET UP

if [ "$1" = "make" ] ; then
	echo BUILDING EXAMPLE
	make clean
	make
	echo BUILD FINISHED
fi

echo === RUNNING PREFETCHTEST ===
mpirun  --map-by ppr:1:node         \
        --mca mpi_leave_pinned 1    \
        --mca osc pt2pt             \
        -n ${NNODES} ./prefetchtest -s ${NUM_PAGES} -i ${ITERATIONS} -n 1 -m ${NNODES}
echo === PREFETCHTEST FINISHED ===
