CXX = g++
# OPT = -O3
OPT = -g # debugger mode
WARN = -Wall
ERR = -Werror
#DEBUG = -D_DEBUG

CXXFLAGS = $(OPT) $(WARN) $(ERR) $(INC) $(LIB) $(DEBUG) -std=c++11

# Check https://makefiletutorial.com/#fancy-rules for makefile tutorial 

SRC = $(wildcard *.cc)
OBJ = $(subst .cc,.o,$(SRC))

# OBJ = main.o cache.o

all: smp_cache
	@echo "Compilation Done\n"

smp_cache: $(OBJ)
	$(CXX) -o smp_cache $(CXXFLAGS) $(OBJ) -lm
	@echo "------------------------------------------------------------"
	@echo "        --- Cache COHERENCE PROTOCOL SIMULATOR ---"
	@echo "------------------------------------------------------------"

clean:
	rm -f *.o smp_cache

PROTOCOL = 3
#TRACE_FILE = ../trace/canneal.04t.debug
TRACE_FILE = ../trace/canneal.04t.longTrace
VALIDATION_FILE = ../val/mesi_filter_long.val2

run: all
	@echo "*** Running ./smp_cache 8192 8 64 4 $(PROTOCOL) $(TRACE_FILE) ***"
	./smp_cache 8192 8 64 4 $(PROTOCOL) $(TRACE_FILE)

val: all
	@echo "*** Comparing output with $(VALIDATION_FILE) ***"
	./smp_cache 8192 8 64 4 $(PROTOCOL) $(TRACE_FILE) | diff -iwy - $(VALIDATION_FILE)

pack:
	zip -j project1.zip *.cc *.h