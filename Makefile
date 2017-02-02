os:=$(shell uname | tr '[:upper:]' '[:lower:]')
DEBUG=false
CFLAGS=-march=native

ifeq ($(DEBUG),true)
	CFLAGS+= -DDEBUG -O0 -g
else
	CFLAGS += -O3 -ffast-math -g
endif
CC=clang
CXX=clang++
CXXFLAGS= $(CFLAGS) -Wall -std=c++11 -I/usr/local/opt/opencv3/include -I/usr/local/include
LDFLAGS= -L/usr/local/lib -L/usr/local/opt/opencv3/lib
LDLIBS= -lopencv_videoio -lopencv_highgui -lopencv_core -lopencv_imgproc -lopencv_imgcodecs -lsoundio -lfolly -lsndfile -lportaudio
LDLIBS+= -lsfml-window -lsfml-graphics -lsfml-system -lsfml-network

qmix: qmix.o mixer.o
	$(CXX) $(LDFLAGS) $(LDLIBS) -o $@ $< mixer.o


qmix.o: qmix.cpp qmix.hpp utils.hpp
mixer.o: mixer.cpp qmix.hpp utils.hpp mixer.hpp

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(PY_CFLAGS) -c -o $@ $<

.PHONY: clean

clean:
	rm *.o
	rm qmix
