#! /bin/bash -l
#SBATCH -J paratest
#SBATCH -p cluster
#SBATCH -t 5:00
#SBATCH -o output.out
#SBATCH -N 4

echo SETTING UP ENVIRONMENT
cd ${HOME}/git/paratest/
echo ENVIRONMENT SET UP

echo BUILDING EXAMPLE
make clean
make
echo BUILD FINISHED

echo === RUNNING PARATEST ===
mpirun -n 4 -ppn 1 ./paratest
echo === PARATEST FINISHED ===
