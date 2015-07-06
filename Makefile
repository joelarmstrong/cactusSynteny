.PHONY: clean

CFLAGS = -g -std=c99 -Wall -Werror -Wextra
CPPFLAGS = -g -Wall -Werror -Wextra

PROGRESSIVE_CACTUS_DIR=/cluster/home/jcarmstr/progressiveCactus-fresh-fresh

SONLIB_INCS = -I${PROGRESSIVE_CACTUS_DIR}/submodules/sonLib/lib
SONLIB_LIBS = ${PROGRESSIVE_CACTUS_DIR}/submodules/sonLib/lib/sonLib.a

PINCH_INCS = -I${PROGRESSIVE_CACTUS_DIR}/submodules/pinchesAndCacti/inc
PINCH_LIBS = ${PROGRESSIVE_CACTUS_DIR}/submodules/sonLib/lib/stPinchesAndCacti.a

MATCHINGANDORDERING_LIBS = ${PROGRESSIVE_CACTUS_DIR}/submodules/sonLib/lib/matchingAndOrdering.a ${PROGRESSIVE_CACTUS_DIR}/submodules/sonLib/lib/3EdgeConnected.a

HAL_INCS = -I${PROGRESSIVE_CACTUS_DIR}/submodules/hal/lib
HAL_LIBS = ${PROGRESSIVE_CACTUS_DIR}/submodules/hal/lib/halLib.a

HDF5_LIBS = ${PROGRESSIVE_CACTUS_DIR}/submodules/hdf5/lib/libhdf5_cpp.a \
	    ${PROGRESSIVE_CACTUS_DIR}/submodules/hdf5/lib/libhdf5.a -lz

all: bin/synteny

src/halToPinch.o: src/halToPinch.cpp
	h5c++ ${CPPFLAGS} -c -o src/halToPinch.o src/halToPinch.cpp ${HAL_INCS} ${PINCH_INCS} ${SONLIB_INCS}

src/pinchToCactus.o: src/pinchToCactus.c
	gcc ${CFLAGS} -o src/pinchToCactus.o -c src/pinchToCactus.c ${PINCH_INCS} ${SONLIB_INCS}

bin/synteny: src/main.c src/halToPinch.o src/pinchToCactus.o
	gcc ${CFLAGS} -o $@ $^ -Iinc ${HAL_INCS} ${HAL_LIBS} \
	       ${PINCH_INCS} ${PINCH_LIBS} ${SONLIB_INCS} \
	       ${SONLIB_LIBS} ${HDF5_LIBS} ${MATCHINGANDORDERING_LIBS} -lstdc++

clean:
	rm bin/synteny src/*.o
