CC      = cc
CXX     = c++
CFLAGS  = -g -O3 -Wall -std=c++0x -pthread
LIBS    = -lm -lpthread -lmosquitto
LDFLAGS = -g

OBJECTS = Conf.o DAPNETGateway.o DAPNETNetwork.o Log.o MQTTConnection.o POCSAGMessage.o POCSAGNetwork.o REGEX.o StopWatch.o \
	  TCPSocket.o Thread.o Timer.o UDPSocket.o Utils.o

all:		DAPNETGateway

DAPNETGateway:	$(OBJECTS)
		$(CXX) $(OBJECTS) $(CFLAGS) $(LIBS) -o DAPNETGateway

%.o: %.cpp
		$(CXX) $(CFLAGS) -c -o $@ $<

install:
		install -m 755 DAPNETGateway /usr/local/bin/

clean:
		$(RM) DAPNETGateway *.o *.d *.bak *~
