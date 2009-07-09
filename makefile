all: kvlite

kvlite: KVLite.cpp
	g++ -W -Wall -lpthread -o kvlite KVLite.cpp

clean:
	rm kvlite