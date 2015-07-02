#ifndef __PINCH_TO_CACTUS_H_
#define __PINCH_TO_CACTUS_H_
#include "stCactusGraphs.h"
#include "sonLib.h"

stCactusGraph *getCactusGraphForThreadSet(stPinchThreadSet *threadSet, stCactusNode **startCactusNode,
                                          stList **deadEndComponent, bool attachEndsInFlower, int64_t minLengthForChromosome,
                                          double proportionOfUnalignedBasesForNewChromosome,
                                          bool breakChainsAtReverseTandems, int64_t maximumMedianSpacingBetweenLinkedEnds);

#endif //  __PINCH_TO_CACTUS_H_
