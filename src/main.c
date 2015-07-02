#include "halToPinch.h"
#include "pinchToCactus.h"

int main(int argc, char *argv[]) {
    stHash *threadIdToName = NULL;
    stPinchThreadSet *threadSet = pairwiseHalToPinch(argv[1], argv[2], argv[3], &threadIdToName);

    stCactusNode *startCactusNode;
    stList *deadEndComponent;
    stCactusGraph *cactusGraph = getCactusGraphForThreadSet(threadSet, &startCactusNode,
                                                            &deadEndComponent, false, 0, 0.0, true,
                                                            0);
    return 0;
}
