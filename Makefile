os:=$(shell uname | tr '[:upper:]' '[:lower:]')
DEBUG=true
CFLAGS=-march=native

ifeq ($(DEBUG),true)
	CFLAGS+= -DDEBUG -O0 -g
else
	CFLAGS += -O3 -ffast-math
endif
CC=clang
CXX=clang++
CXXFLAGS= $(CFLAGS) -Wall -std=c++11 -I/usr/local/opt/opencv3/include -I/usr/local/include
LDFLAGS= -L/usr/local/lib -L/usr/local/opt/opencv3/lib
LDLIBS= -lopencv_videoio -lopencv_highgui -lopencv_core -lopencv_imgproc -lzbar

qmix: qmix.o
	$(CXX) $(LDFLAGS) $(LDLIBS) -o $@ $<


qmix.o: qmix.cpp qmix.hpp

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(PY_CFLAGS) -c -o $@ $<


armadillo:
	cd ../../ext/armadillo/build && $(MAKE)

.PHONY: clean armadillo

clean:
	rm *.o
	rm qmix
