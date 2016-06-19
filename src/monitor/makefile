CC   = gcc
CXX  = g++

PRJ_PATH = ../..

C_FLAGS = -W 
C_FLAGS += -O2 
C_FLAGS += -fno-strict-aliasing
C_FLAGS += -Wall
C_FLAGS += -Wno-unused
C_FLAGS += -Wno-missing-field-initializers 
C_FLAGS += -Wno-missing-braces
C_FLAGS += -g 
C_FLAGS += -z 
C_FLAGS += -fPIC 

INC = -I../
INC += -I$(PRJ_PATH)/include/
LIB = -lz -lrt -lpthread 
LIB += -L$(PRJ_PATH)/lib/ -lshm

SOURCE = $(wildcard *.cpp)
OBJS    = $(patsubst %.cpp,%.o,$(SOURCE))

OUTPUT  := $(PRJ_PATH)/lib/libmonitor.a

all: $(OUTPUT)

.SUFFIXES: .o .cpp

$(OUTPUT):$(OBJS)
	 ar -rs $@ $(EX_LIB) $^
	 cp *.h $(PRJ_PATH)/include

.cpp.o:
	g++ $(C_FLAGS) $(LIB) $(INC) -c  $^ -o $(patsubst %.cpp,%.o,$^)

clean:
	rm -f $(OBJS) 
