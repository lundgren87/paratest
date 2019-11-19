#! /bin/bash -l
#SBATCH -J paratest
#SBATCH -p cluster
#SBATCH -t 15:00
#SBATCH -o results/para_8N
#SBATCH -N 8

NNODES=8
ITERATIONS=500
ARRAYSIZE=67108864

echo SETTING UP ENVIRONMENT
cd ${HOME}/git/paratest/
echo ENVIRONMENT SET UP

if [ "$1" = "make" ] ; then
    echo BUILDING EXAMPLE
    make clean
    make
    echo BUILD FINISHED
fi

echo === RUNNING PARATEST ===
### Intel MPI
#mpirun -n 1 -ppn 1 ./paratest

### OpenMPI
mpirun  --map-by ppr:1:node         \
        --mca mpi_leave_pinned 1    \
        --mca osc pt2pt             \
        -n ${NNODES} ./paratest -s ${ARRAYSIZE} -i ${ITERATIONS} -n 1
mpirun  --map-by ppr:1:node         \
        --mca mpi_leave_pinned 1    \
        --mca osc pt2pt             \
        -n ${NNODES} ./paratest -s ${ARRAYSIZE} -i ${ITERATIONS} -n 2
mpirun  --map-by ppr:1:node         \
        --mca mpi_leave_pinned 1    \
        --mca osc pt2pt             \
        -n ${NNODES} ./paratest -s ${ARRAYSIZE} -i ${ITERATIONS} -n 4
mpirun  --map-by ppr:1:node         \
        --mca mpi_leave_pinned 1    \
        --mca osc pt2pt             \
        -n ${NNODES} ./paratest -s ${ARRAYSIZE} -i ${ITERATIONS} -n 8
echo === PARATEST FINISHED ===
