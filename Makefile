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
CXXFLAGS= $(CFLAGS) -Wall -std=c++11 -I/usr/local/opt/opencv3/include -I/usr/local/include -Isoundfile-2.2/include
LDFLAGS= -L/usr/local/lib -L/usr/local/opt/opencv3/lib
LDLIBS= -lopencv_videoio -lopencv_highgui -lopencv_core -lopencv_imgproc -lportaudio

qmix: qmix.o libsoundfile.a
	$(CXX) $(LDFLAGS) $(LDLIBS) -o $@ $< libsoundfile.a


qmix.o: qmix.cpp qmix.hpp

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(PY_CFLAGS) -c -o $@ $<

libsoundfile.a:
	cd soundfile-2.2 && $(MAKE) library
	cp soundfile-2.2/lib/libsoundfile.a ./

.PHONY: clean armadillo

clean:
	rm *.o
	rm qmix
	rm libsoundfile.a
