ARGO_INSTALL_DIR=/home/sven/git/argodsm/install

CXX=mpicxx
CXXFLAGS := -O3 -std=c++11 -Wl,-rpath,${ARGO_INSTALL_DIR}/lib -L${ARGO_INSTALL_DIR}/lib -I${ARGO_INSTALL_DIR}/include
LDFLAGS := -largo -largobackend-mpi

.PHONY: all
all: paratest prefetchtest

paratest: paratest.cpp
	${CXX} ${CXXFLAGS} -o $@ $< $(LDFLAGS)

prefetchtest: prefetchtest.cpp
	${CXX} ${CXXFLAGS} -o $@ $< $(LDFLAGS)

.PHONY: clean
clean:
	${RM} paratest prefetchtest
