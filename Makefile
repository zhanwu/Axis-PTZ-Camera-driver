CC = g++

# ADB: Adding customizable include and lib paths.
INCLUDE   = $(shell pkg-config --cflags playercore libcurl)
LDFLAGS   = $(shell pkg-config --libs playercore libcurl)
CFLAGS    = -O3 -Wall -fpic $(INCLUDE)
EXTRALIBS = $(LDFLAGS) -lplayercore -lltdl -lpthread -lplayererror -lcurl

all: libCameraAxis.so libPtzAxis.so

%.o: %.cc
	$(CC) $(CFLAGS) -c $<

libCameraAxis.so: CameraAxis.o
	$(CC) -shared -o $@ $^ $(LDFLAGS)

libPtzAxis.so: PtzAxis.o
	$(CC) -shared -o $@ $^ $(LDFLAGS)

clean:
	rm -f *.o *.so
