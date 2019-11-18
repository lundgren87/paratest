#! /bin/bash -l
#SBATCH -J paratest
#SBATCH -p cluster
#SBATCH -t 5:00
#SBATCH -o output.out
#SBATCH -N 4

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
        -n 4 ./paratest
echo === PARATEST FINISHED ===
