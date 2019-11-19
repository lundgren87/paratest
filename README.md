# Simple tests for ArgoDSM development.
1. Edit Makefile to include the path to your ArgoDSM installation.
2. Make
3. mpirun -n nodes ./paratest -s arraysize -i iterations -a alpha -n threads

*(nodes x threads must divide arraysize)*
