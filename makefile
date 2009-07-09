all: kvlite

kvlite: KVLite.cpp md5.c
	g++ -W -Wall -lpthread -o kvlite KVLite.cpp md5.c

clean:
	rm kvlite