CC      = cc
CXX     = c++
CFLAGS  = -g -O3 -Wall -DHAVE_LOG_H -std=c++0x -pthread
LIBS    = -lm -lpthread
LDFLAGS = -g

OBJECTS = Conf.o DAPNETGateway.o DAPNETNetwork.o Log.o POCSAGMessage.o POCSAGNetwork.o StopWatch.o TCPSocket.o Thread.o Timer.o UDPSocket.o Utils.o REGEX.o

all:		DAPNETGateway

DAPNETGateway:	GitVersion.h $(OBJECTS)
		$(CXX) $(OBJECTS) $(CFLAGS) $(LIBS) -o DAPNETGateway

%.o: %.cpp
		$(CXX) $(CFLAGS) -c -o $@ $<

DAPNETGateway.o: GitVersion.h FORCE

.PHONY: GitVersion.h

FORCE:


install:
		install -m 755 DAPNETGateway /usr/local/bin/

clean:
		$(RM) DAPNETGateway *.o *.d *.bak *~

# Export the current git version if the index file exists, else 000...
GitVersion.h:
ifneq ("$(wildcard .git/index)","")
	echo "const char *gitversion = \"$(shell git rev-parse HEAD)\";" > $@
else
	echo "const char *gitversion = \"0000000000000000000000000000000000000000\";" > $@
endif

