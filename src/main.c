#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include "halToPinch.h"
#include "pinchToCactus.h"

static void usage(char *argv0) {
    fprintf(stderr, "%s: find syntenic regions by progressively removing "
            "rearrangements\n", argv0);
    fprintf(stderr, "Usage: %s halFile genome1 genome2\n", argv0);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "-c --chainLengths x,y,z: output syntenic regions of "
            "length > x, y, z\n");
}

int main(int argc, char *argv[]) {
    // options and defaults
    int64_t defaultChainLengths[10] = {1, 2, 5, 10, 20, 50, 128, 1024, 10000, 100000};
    int64_t *chainLengths = defaultChainLengths;
    size_t chainLengthsLen = sizeof(defaultChainLengths)/sizeof(defaultChainLengths[0]);

    // parsing options
    struct option longopts[] = { {"chainLengths", required_argument, 0, 'c'},
                          {NULL, 0, 0, 0} };
    for (;;) {
        int key = getopt_long(argc, argv, "c", longopts, NULL);
        if (key == -1) {
            break;
        }
        switch (key) {
        case 'c':; // semicolon is a stupid trick so that the
                   // declaration below will compile
            stList *chainLengthStrs = stString_splitByString(optarg, ",");
            chainLengthsLen = stList_length(chainLengthStrs);
            chainLengths = malloc(stList_length(chainLengthStrs) * sizeof(int64_t));
            for (int64_t i = 0; i < stList_length(chainLengthStrs); i++) {
                errno = 0;
                chainLengths[i] = atoll(stList_get(chainLengthStrs, i));
                if (errno) {
                    st_errnoAbort("Could not parse integer chain length: %s",
                                  stList_get(chainLengthStrs, i));
                }
            }
            stList_destruct(chainLengthStrs);
            break;
        default:
            usage(argv[0]);
            return 1;
        }
    }

    if (argc < optind + 3) {
        usage(argv[0]);
        return 1;
    }

    char *halPath = argv[optind];
    char *srcGenomeName = argv[optind + 1];
    char *destGenomeName = argv[optind + 2];

    stHash *sequenceLabelToThread = NULL;
    stPinchThreadSet *threadSet = pairwiseHalToPinch(halPath, srcGenomeName,
                                                     destGenomeName,
                                                     &sequenceLabelToThread);

    stCactusNode *startCactusNode;
    stList *deadEndComponent;
    stCactusGraph *cactusGraph = getCactusGraphForThreadSet(threadSet, &startCactusNode,
                                                            &deadEndComponent, false, 0, 0.0, true,
                                                            0);

    for (size_t i = 0; i < chainLengthsLen; i++) {
        printf("removing chains of size %" PRIi64 "\n", chainLengths[i]);
        /* printChains(cactusGraph, threadToName); */
        /* removeSmallChains(cactusGraph, chainLengths[i]); */
    }
    stCactusGraph_destruct(cactusGraph);
    stPinchThreadSet_destruct(threadSet);
    if (chainLengths != defaultChainLengths) {
        free(chainLengths);
    }
    stHash_destruct(sequenceLabelToThread);
    return 0;
}
