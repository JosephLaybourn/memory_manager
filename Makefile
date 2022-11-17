#GCC=g++
GCCFLAGS=-O -Wall -Werror -Wextra -std=c++11 -pedantic -Wconversion -Wold-style-cast 

OBJECTS0=ObjectAllocator.cpp PRNG.cpp
DRIVER0=driver-sample.cpp

VALGRIND_OPTIONS=-q --leak-check=full
DIFF_OPTIONS=-y --strip-trailing-cr --suppress-common-lines -b

OSTYPE := $(shell uname)
ifeq (,$(findstring CYGWIN,$(OSTYPE)))
CYGWIN=
else
CYGWIN=memmanager.exe
endif

gcc0:
	g++ -o $(PRG) $(CYGWIN) $(DRIVER0) $(OBJECTS0) $(GCCFLAGS)
gcc1:
	clang++ -o gcc1-$(PRG) $(CYGWIN) $(DRIVER0) $(OBJECTS0) $(GCCFLAGS)
gcc2:
	g++ -o gcc2-$(PRG) $(CYGWIN) $(DRIVER0) $(OBJECTS0)  -m32 $(GCCFLAGS)
0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22:
	echo "running test$@"
	watchdog 500 ./$(PRG) $@ >studentout$@
	diff out$@ studentout$@ $(DIFF_OPTIONS) > difference$@
mem0 mem1 mem2 mem3 mem4 mem5 mem6 mem7 mem8 mem9 mem10 mem11 mem12 mem13 mem14 mem15 mem19 mem20 mem21 mem22:
	echo "running memory test $@"
	watchdog 3000 valgrind $(VALGRIND_OPTIONS) ./$(PRG) $(subst mem,,$@) 1>/dev/null 2>difference$@
mem16 mem17 mem18:
	echo "running memory test $@"
	watchdog 8000 valgrind $(VALGRIND_OPTIONS) ./$(PRG) $(subst mem,,$@) 1>/dev/null 2>difference$@
clean : 
	rm *.exe student* difference*
