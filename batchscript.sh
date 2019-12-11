#! /bin/bash -l
#SBATCH -J paratest
#SBATCH -p cluster
#SBATCH -t 20:00
#SBATCH -o output.out
#SBATCH -N 4

NNODES=4
ITERATIONS=100
#ARRAYSIZE=134217728
ARRAYSIZE=8388608
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
#export I_MPI_PROCESS_MANAGER_mpd
#export I_MPI_HYDRA_DEBUG=on
#export I_MPI_DEBUG=100
#export I_MPI_FABRICS=shm:tcp

mpirun -np ${NNODES} -ppn 1 ./paratest -s ${ARRAYSIZE} -i ${ITERATIONS} -n 1
mpirun -np ${NNODES} -ppn 1 ./paratest -s ${ARRAYSIZE} -i ${ITERATIONS} -n 2
mpirun -np ${NNODES} -ppn 1 ./paratest -s ${ARRAYSIZE} -i ${ITERATIONS} -n 4
mpirun -np ${NNODES} -ppn 1 ./paratest -s ${ARRAYSIZE} -i ${ITERATIONS} -n 8


#        --mca btl openib,self,sm    \
	#        --mca osc pt2pt             \
	#        --mca osc ucx               \
	#        --mca pml ob1               \
	### OpenMPI
#mpirun  --map-by ppr:1:node         \
	#        --mca mpi_leave_pinned 1    \
	#        --mca osc pt2pt             \
	#        -n ${NNODES} ./paratest -s ${ARRAYSIZE} -i ${ITERATIONS} -n 1
#mpirun  --map-by ppr:1:node         \
	#        --mca mpi_leave_pinned 1    \
	#        --mca osc pt2pt             \
	#        -n ${NNODES} ./paratest -s ${ARRAYSIZE} -i ${ITERATIONS} -n 2
#mpirun  --map-by ppr:1:node         \
	#        --mca mpi_leave_pinned 1    \
	#        --mca osc pt2pt             \
	#        -n ${NNODES} ./paratest -s ${ARRAYSIZE} -i ${ITERATIONS} -n 4
#mpirun  --map-by ppr:1:node         \
	#        --mca mpi_leave_pinned 1    \
	#        --mca osc pt2pt             \
	#        -n ${NNODES} ./paratest -s ${ARRAYSIZE} -i ${ITERATIONS} -n 8
echo === PARATEST FINISHED ===
