#ifndef __HAL_TO_PINCH_H_
#define __HAL_TO_PINCH_H_

#include "stPinchGraphs.h"


// Gives the alignment between a pair of genomes represented as a
// pinch graph. NB: the coordinates are in Cactus coordinates, i.e. all
// coordinates are 2 ahead of the hal coordinates to make room for the
// thread ends.
stPinchThreadSet *pairwiseHalToPinch(char *halPath, char *srcGenomeName, char *destGenomeName,
                                     stHash **threadIdToName);

#endif // __HAL_TO_PINCH_H_
