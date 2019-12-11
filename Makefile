ARGO_INSTALL_DIR=/home/sven/git/argodsm/install

CXX=mpicxx
CXXFLAGS := -O3 -std=c++11 -L${ARGO_INSTALL_DIR}/lib -I${ARGO_INSTALL_DIR}/include 
LDFLAGS := -largo -largobackend-mpi -lrt

all: paratest

paratest: paratest.cpp
	${CXX} ${CXXFLAGS} -o $@ $< $(LDFLAGS)

clean:
	${RM} paratest
