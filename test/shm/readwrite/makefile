PRJ_PATH = ../../..

all:
	g++ -o read read.cpp -I$(PRJ_PATH)/include/  -L$(PRJ_PATH)/lib/ -static -lshm -pthread
	g++ -o write write.cpp -I$(PRJ_PATH)/include/  -L$(PRJ_PATH)/lib/ -static -lshm -pthread
clean:
	rm read write
