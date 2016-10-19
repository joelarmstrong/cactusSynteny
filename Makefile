.PHONY: clean test

include ${sonLibRootPath}/include.mk
CFLAGS?=${cflags} -std=c99
CPPFLAGS?=${cppflags}

SONLIB_INCS = -I${sonLibRootPath}/lib
SONLIB_LIBS = ${sonLibRootPath}/lib/sonLib.a

PINCH_INCS = -I${pinchesAndCactiDir}/inc
PINCH_LIBS = ${sonLibRootPath}/lib/stPinchesAndCacti.a ${sonLibRootPath}/lib/3EdgeConnected.a

HAL_INCS = -I${halDir}/lib
HAL_LIBS = ${halDir}/lib/halLib.a

HDF5_LIBS = ${hdf5Dir}/lib/libhdf5_cpp.a \
	    ${hdf5Dir}/lib/libhdf5.a -lz

CUTEST_LIBS = ${sonLibRootPath}/lib/cuTest.a

all: bin/synteny bin/syntenyTests

test: bin/syntenyTests
	./bin/syntenyTests

src/halToPinch.o: src/halToPinch.cpp
	h5c++ ${CPPFLAGS} -c -o src/halToPinch.o src/halToPinch.cpp \
	       -Iinc ${HAL_INCS} ${PINCH_INCS} ${SONLIB_INCS}

src/pinchToCactus.o: src/pinchToCactus.c
	${CC} ${CFLAGS} -o src/pinchToCactus.o -c src/pinchToCactus.c \
	       ${PINCH_INCS} ${SONLIB_INCS}

bin/synteny: src/main.c src/halToPinch.o src/pinchToCactus.o
	${CC} ${CFLAGS} -o $@ $^ -Iinc ${HAL_INCS} ${HAL_LIBS} \
	       ${PINCH_INCS} ${PINCH_LIBS} ${SONLIB_INCS} \
	       ${SONLIB_LIBS} ${HDF5_LIBS} -lstdc++ -lm

bin/syntenyTests: tests/*.c src/halToPinch.o
	${CC} ${CFLAGS} -o $@ $^ -Iinc ${HAL_INCS} ${HAL_LIBS} \
	       ${PINCH_INCS} ${PINCH_LIBS} ${SONLIB_INCS} \
	       ${SONLIB_LIBS} ${HDF5_LIBS} \
	       ${CUTEST_LIBS} -lstdc++ -lm

clean:
	rm bin/* src/*.o
