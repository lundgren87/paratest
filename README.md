# Simple tests for ArgoDSM development.
1. Edit Makefile to include the path to your ArgoDSM installation.
2. Make
3. mpirun -np nodes ./paratest -s arraysize -i iterations -a alpha -n threads
*(nodes x threads must divide arraysize)*
4. mpirun -np nodes ./prefetchtest -n pages -i iterations -n threads -m nodes
*(-np nodes must match -m nodes)*
