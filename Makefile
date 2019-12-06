ARGO_INSTALL_DIR=/home/sven/git/argodsm/install

CXX=mpicxx
CXXFLAGS := -O3 -std=c++11 -g -L${ARGO_INSTALL_DIR}/lib -I${ARGO_INSTALL_DIR}/include 
LDFLAGS := -largo -largobackend-mpi -lrt -g

all: paratest simpletest

paratest: paratest.cpp
	${CXX} ${CXXFLAGS} -o $@ $< $(LDFLAGS)
	
simpletest: simpletest.cpp
	${CXX} ${CXXFLAGS} -o $@ $< $(LDFLAGS)

clean:
	${RM} paratest simpletest
