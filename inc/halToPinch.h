#ifndef __HAL_TO_PINCH_H_
#define __HAL_TO_PINCH_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stPinchGraphs.h"

// Represents a unique sequence in the pinch/HAL graph.
typedef struct {
    char *genomeName;
    char *sequenceName;
} sequenceLabel;

void sequenceLabel_destruct(sequenceLabel *label);

sequenceLabel *sequenceLabel_construct(const char *genomeName,
                                       const char *sequenceName);

uint64_t sequenceLabel_hashKey(const sequenceLabel *label);

int sequenceLabel_equals(const sequenceLabel *label1, const sequenceLabel *label2);

// Gives the alignment between a pair of genomes represented as a
// pinch graph. NB: the coordinates are in Cactus coordinates, i.e. all
// coordinates are 2 ahead of the hal coordinates to make room for the
// thread ends.
stPinchThreadSet *pairwiseHalToPinch(char *halPath, char *srcGenomeName, char *destGenomeName,
                                     stHash **sequenceLabelToThread);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __HAL_TO_PINCH_H_
