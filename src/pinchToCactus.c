// This is almost an exact copy of caf/impl/pinchToCactus.c in cactus,
// except this doesn't need a cactus DB.
#include "sonLib.h"
#include "stPinchGraphs.h"
#include "stCactusGraphs.h"

#include <stdlib.h>

///////////////////////////////////////////////////////////////////////////
// Construct dead end component
///////////////////////////////////////////////////////////////////////////

static void attachPinchBlockEndToAnotherComponent(stPinchBlock *pinchBlock, bool orientation, stList *anotherComponent,
        stHash *pinchEndsToAdjacencyComponents) {
    assert(pinchBlock != NULL);
    assert(stPinchBlock_getLength(pinchBlock) == 1);
    stPinchEnd staticPinchEnd = stPinchEnd_constructStatic(pinchBlock, orientation);
    stList *component = stHash_remove(pinchEndsToAdjacencyComponents, &staticPinchEnd);
    assert(component != NULL);
    assert(stList_length(component) == 1);
    stPinchEnd *pinchEnd = stList_get(component, 0);
    assert(stPinchEnd_equalsFn(&staticPinchEnd, pinchEnd));
    stList_setDestructor(component, NULL);
    stList_destruct(component);
    stList_append(anotherComponent, pinchEnd);
    stHash_insert(pinchEndsToAdjacencyComponents, pinchEnd, anotherComponent);
}

static stList *stCaf_constructDeadEndComponent(stPinchThreadSet *threadSet, stHash *pinchEndsToAdjacencyComponents,
                                               stSet *leftEndAttached, stSet *rightEndAttached) {
    /*
     * Locates the ends of all the attached ends and merges together their 'dead end' components to create a single
     * 'dead end' component, as described in the JCB cactus paper.
     */
    //For each block end at the end of a thread, attach to dead end component if associated end is attached
    stList *deadEndAdjacencyComponent = stList_construct3(0, (void(*)(void *)) stPinchEnd_destruct);
    stPinchThreadSetIt threadIt = stPinchThreadSet_getIt(threadSet);
    stPinchThread *pinchThread;
    while ((pinchThread = stPinchThreadSetIt_getNext(&threadIt))) {
        if (stSet_search(leftEndAttached, pinchThread)) {
            stPinchSegment *pinchSegment = stPinchThread_getFirst(pinchThread);
            stPinchBlock *pinchBlock = stPinchSegment_getBlock(pinchSegment);
            if (stPinchBlock_getFirst(pinchBlock) == pinchSegment) { //We only want to do this once
                attachPinchBlockEndToAnotherComponent(pinchBlock, stPinchSegment_getBlockOrientation(pinchSegment),
                        deadEndAdjacencyComponent, pinchEndsToAdjacencyComponents);
            }
        }
        if (stSet_search(rightEndAttached, pinchThread)) {
            stPinchSegment *pinchSegment = stPinchThread_getLast(pinchThread);
            stPinchBlock *pinchBlock = stPinchSegment_getBlock(pinchSegment);
            if (stPinchBlock_getFirst(pinchBlock) == pinchSegment) { //And only once for the other end
                attachPinchBlockEndToAnotherComponent(pinchBlock, !stPinchSegment_getBlockOrientation(pinchSegment),
                        deadEndAdjacencyComponent, pinchEndsToAdjacencyComponents);
            }
        }
    }
    return deadEndAdjacencyComponent;
}

///////////////////////////////////////////////////////////////////////////
// Attach unatttached thread components
///////////////////////////////////////////////////////////////////////////

static bool threadIsAttachedToDeadEndComponent5Prime(stPinchThread *thread, stList *deadEndComponent,
        stHash *pinchEndsToAdjacencyComponents) {
    stPinchSegment *pinchSegment = stPinchThread_getFirst(thread);
    stPinchBlock *pinchBlock = stPinchSegment_getBlock(pinchSegment);
    assert(pinchBlock != NULL);
    stPinchEnd staticPinchEnd = stPinchEnd_constructStatic(pinchBlock, stPinchSegment_getBlockOrientation(pinchSegment));
    stList *adjacencyComponent = stHash_search(pinchEndsToAdjacencyComponents, &staticPinchEnd);
    assert(adjacencyComponent != NULL);
    return adjacencyComponent == deadEndComponent;
}

static bool threadIsAttachedToDeadEndComponent3Prime(stPinchThread *thread, stList *deadEndComponent,
        stHash *pinchEndsToAdjacencyComponents) {
    stPinchSegment *pinchSegment = stPinchThread_getLast(thread);
    stPinchBlock *pinchBlock = stPinchSegment_getBlock(pinchSegment);
    assert(pinchBlock != NULL);
    stPinchEnd staticPinchEnd = stPinchEnd_constructStatic(pinchBlock, !stPinchSegment_getBlockOrientation(pinchSegment));
    stList *adjacencyComponent = stHash_search(pinchEndsToAdjacencyComponents, &staticPinchEnd);
    assert(adjacencyComponent != NULL);
    return adjacencyComponent == deadEndComponent;
}

static bool threadIsAttachedToDeadEndComponent(stPinchThread *thread, stList *deadEndComponent, stHash *pinchEndsToAdjacencyComponents) {
    return threadIsAttachedToDeadEndComponent5Prime(thread, deadEndComponent, pinchEndsToAdjacencyComponents)
            || threadIsAttachedToDeadEndComponent3Prime(thread, deadEndComponent, pinchEndsToAdjacencyComponents);
}

static void attachThreadToDeadEndComponent(stPinchThread *thread, stList *deadEndAdjacencyComponent,
                                           stHash *pinchEndsToAdjacencyComponents, bool markEndsAttached,
                                           stSet *leftEndAttached, stSet *rightEndAttached) {
    stPinchSegment *segment = stPinchThread_getFirst(thread);
    attachPinchBlockEndToAnotherComponent(stPinchSegment_getBlock(segment), stPinchSegment_getBlockOrientation(segment),
            deadEndAdjacencyComponent, pinchEndsToAdjacencyComponents);
    segment = stPinchThread_getLast(thread);
    attachPinchBlockEndToAnotherComponent(stPinchSegment_getBlock(segment), !stPinchSegment_getBlockOrientation(segment),
            deadEndAdjacencyComponent, pinchEndsToAdjacencyComponents);
    if (markEndsAttached) { //Get the ends and attach them
        stSet_insert(leftEndAttached, thread);
        stSet_insert(rightEndAttached, thread);
    }
}

int comparePinchThreadsByLength(const void *a, const void *b) {
    int64_t i = stPinchThread_getLength((stPinchThread *) a);
    int64_t j = stPinchThread_getLength((stPinchThread *) b);
    return i > j ? 1 : (i < j ? -1 : 0);
}

static void attachThreadComponentToDeadEndComponent(stList *threadComponent, stList *deadEndComponent,
                                                    stHash *pinchEndsToAdjacencyComponents, bool markEndsAttached,
                                                    int64_t minLengthForChromosome, double proportionOfUnalignedBasesForNewChromosome,
                                                    stSet *leftEndAttached, stSet *rightEndAttached) {
    /*
     * Algorithm walks the threads in the connected component, in descending order of length and
     * for each thread attaches it to the dead-end component if its length of bases in blocks not contained in chromosome
     * length fragments.
     */

    //First get threads in order, already attached first then unattached, sorted by descending length
    bool first = 1; //This is used to ensure at least one thread is attached in component.
    stList *l = stList_construct();
    stList *l2 = stList_construct();
    for (int64_t i = 0; i < stList_length(threadComponent); i++) {
        stPinchThread *pinchThread = stList_get(threadComponent, i);
        if (threadIsAttachedToDeadEndComponent(pinchThread, deadEndComponent, pinchEndsToAdjacencyComponents)) {
            first = 0;
            stList_append(l2, pinchThread);
        } else {
            stList_append(l, pinchThread);
        }
    }
    stList_sort(l, comparePinchThreadsByLength);
    stList_appendAll(l, l2);
    stList_destruct(l2);

    //Now iterate on threads, attaching as needed.
    stSet *blocksSeen = stSet_construct();
    stHash *basesAligned = stHash_construct2(NULL, free);
    while (stList_length(l) > 0) {
        stPinchThread *pinchThread = stList_pop(l);
        if (stPinchThread_getLength(pinchThread) < minLengthForChromosome && !first) {
            continue;
        }
        first = 0;
        int64_t *i = stHash_search(basesAligned, pinchThread);
        int64_t basesAlignedToChromosomeThreads = i != NULL ? *i : 0; //This is the number of bases already aligned in threads with attached ends (chromosomes);
        assert(basesAlignedToChromosomeThreads >= 0);
        //Walk the thread
        stPinchSegment *segment = stPinchThread_getFirst(pinchThread);
        assert(segment != NULL);
        assert(stPinchSegment_get5Prime(segment) == NULL);
        do {
            stPinchBlock *block;
            if ((block = stPinchSegment_getBlock(segment)) != NULL && !stSet_search(blocksSeen, block)) {
                stSet_insert(blocksSeen, block);
                stPinchBlockIt segIt = stPinchBlock_getSegmentIterator(block);
                stPinchSegment *segment2;
                while ((segment2 = stPinchBlockIt_getNext(&segIt)) != NULL) {
                    stPinchThread *pinchThread2 = stPinchSegment_getThread(segment2);
                    int64_t *i = stHash_search(basesAligned, pinchThread2);
                    if (i == NULL) {
                        i = st_calloc(1, sizeof(int64_t));
                        stHash_insert(basesAligned, pinchThread2, i);
                    }
                    *i += stPinchBlock_getLength(block);
                }
            }
            segment = stPinchSegment_get3Prime(segment);
        } while (segment != NULL);
        if (threadIsAttachedToDeadEndComponent(pinchThread, deadEndComponent, pinchEndsToAdjacencyComponents)) { //If this is already attached we can stop at this point
            continue;
        }
        i = stHash_search(basesAligned, pinchThread);
        int64_t totalBasesAligned = i != NULL ? *i : 0; //This is the number of bases already aligned in chromosomes;
        assert(totalBasesAligned >= basesAlignedToChromosomeThreads);
        if((totalBasesAligned - basesAlignedToChromosomeThreads) >= proportionOfUnalignedBasesForNewChromosome * totalBasesAligned) {
            attachThreadToDeadEndComponent(pinchThread, deadEndComponent, pinchEndsToAdjacencyComponents, markEndsAttached, leftEndAttached, rightEndAttached);
        }
    }
    stList_destruct(l);
    stSet_destruct(blocksSeen);
    stHash_destruct(basesAligned);
}

static void stCaf_attachUnattachedThreadComponents(stPinchThreadSet *threadSet, stList *deadEndComponent,
                                                   stHash *pinchEndsToAdjacencyComponents, bool markEndsAttached, int64_t minLengthForChromosome,
                                                   double proportionOfUnalignedBasesForNewChromosome, stSet *leftEndAttached, stSet *rightEndAttached) {
    /*
     * Locates threads components which have no dead ends part of the dead end component, and then
     * connects them, picking the longest thread to attach them.
     */
    stSortedSet *threadComponents = stPinchThreadSet_getThreadComponents(threadSet);
    assert(stSortedSet_size(threadComponents) > 0);
    stSortedSetIterator *threadIt = stSortedSet_getIterator(threadComponents);
    stList *threadComponent;
    while ((threadComponent = stSortedSet_getNext(threadIt)) != NULL) {
        attachThreadComponentToDeadEndComponent(threadComponent, deadEndComponent, pinchEndsToAdjacencyComponents, markEndsAttached,
                                                minLengthForChromosome, proportionOfUnalignedBasesForNewChromosome, leftEndAttached, rightEndAttached);
    }
    stSortedSet_destructIterator(threadIt);
    stSortedSet_destruct(threadComponents);
}

///////////////////////////////////////////////////////////////////////////
// Create a cactus graph from a pinch graph
///////////////////////////////////////////////////////////////////////////

static stCactusNode *getCactusNode(stPinchEnd *pinchEnd, stHash *pinchEndsToAdjacencyComponents, stHash *adjacencyComponentsToCactusNodes) {
    stList *adjacencyComponent = stHash_search(pinchEndsToAdjacencyComponents, pinchEnd);
    assert(adjacencyComponent != NULL);
    stCactusNode *cactusNode = stHash_search(adjacencyComponentsToCactusNodes, adjacencyComponent);
    assert(cactusNode != NULL);
    return cactusNode;
}

void *stCaf_mergeNodeObjects(void *a, void *b) {
    stList *adjacencyComponents1 = a;
    stList *adjacencyComponents2 = b;
    assert(adjacencyComponents1 != adjacencyComponents2);
    stList_appendAll(adjacencyComponents1, adjacencyComponents2);
    stList_setDestructor(adjacencyComponents2, NULL);
    stList_destruct(adjacencyComponents2);
    return adjacencyComponents1;
}

static void *makeNodeObject(stList *adjacencyComponent) {
    stList *adjacencyComponents = stList_construct3(0, (void(*)(void *)) stList_destruct);
    stList_append(adjacencyComponents, adjacencyComponent);
    return adjacencyComponents;
}

static bool isDeadEndStubComponent(stList *adjacencyComponent, stPinchEnd *pinchEnd) {
    if (stList_length(adjacencyComponent) != 1) {
        return 0;
    }
    stPinchSegment *pinchSegment = stPinchBlock_getFirst(stPinchEnd_getBlock(pinchEnd));
    return (stPinchEnd_traverse5Prime(stPinchEnd_getOrientation(pinchEnd), pinchSegment) ? stPinchSegment_get5Prime(pinchSegment)
            : stPinchSegment_get3Prime(pinchSegment)) == NULL;
}

static bool stCaf_reversalAtEnd(stCactusEdgeEnd *cactusEdgeEnd, void *extraArg) {
    assert(extraArg == NULL);
    assert(stCactusEdgeEnd_getObject(cactusEdgeEnd) != NULL);
    assert(stCactusEdgeEnd_getLink(cactusEdgeEnd) != NULL);
    assert(stCactusEdgeEnd_getObject(stCactusEdgeEnd_getLink(cactusEdgeEnd)) != NULL);
    return stPinchEnd_hasSelfLoopWithRespectToOtherBlock(stCactusEdgeEnd_getObject(cactusEdgeEnd),
            stPinchEnd_getBlock(stCactusEdgeEnd_getObject(stCactusEdgeEnd_getLink(cactusEdgeEnd))));
}

static bool stCaf_breakAtTooGreatEnoughMedianSeparation(stCactusEdgeEnd *cactusEdgeEnd, void *extraArg) {
    int64_t maximumMedianSpacingBetweenLinkedEnds = *(int64_t *)extraArg;
    assert(maximumMedianSpacingBetweenLinkedEnds >= 0 && maximumMedianSpacingBetweenLinkedEnds < INT64_MAX);
    stList *lengths = stPinchEnd_getSubSequenceLengthsConnectingEnds(stCactusEdgeEnd_getObject(cactusEdgeEnd), stCactusEdgeEnd_getObject(stCactusEdgeEnd_getLink(cactusEdgeEnd)));
    stList_sort(lengths, (int (*)(const void *, const void *))stIntTuple_cmpFn);
    bool b = stList_length(lengths) > 0 && stIntTuple_get(stList_get(lengths, stList_length(lengths)/2), 0) > maximumMedianSpacingBetweenLinkedEnds;
    stList_destruct(lengths);
    return b;
}

static stCactusGraph *stCaf_constructCactusGraph(stList *deadEndComponent, stHash *pinchEndsToAdjacencyComponents,
        stCactusNode **startCactusNode, bool breakChainsAtReverseTandems, int64_t maximumMedianSpacingBetweenLinkedEnds) {
    /*
     * Constructs a cactus graph from a set of pinch graph components, including the dead end component. Returns a cactus
     * graph, and assigns 'startCactusNode' to the cactus node containing the dead end component.
     */
    stCactusGraph *cactusGraph = stCactusGraph_construct2((void(*)(void *)) stList_destruct, NULL);
    stHash *adjacencyComponentsToCactusNodes = stHash_construct();
    stHash *pinchEndsToNonStaticPinchEnds = stHash_construct3(stPinchEnd_hashFn, stPinchEnd_equalsFn, NULL, NULL);

    //Make the nodes
    *startCactusNode = stCactusNode_construct(cactusGraph, makeNodeObject(deadEndComponent));
    stHash_insert(adjacencyComponentsToCactusNodes, deadEndComponent, *startCactusNode);
    stHashIterator *pinchEndIt = stHash_getIterator(pinchEndsToAdjacencyComponents);
    stPinchEnd *pinchEnd;
    while ((pinchEnd = stHash_getNext(pinchEndIt)) != NULL) {
        //Make a map of edge ends to themselves for memory lookup
        stHash_insert(pinchEndsToNonStaticPinchEnds, pinchEnd, pinchEnd);
        stList *adjacencyComponent = stHash_search(pinchEndsToAdjacencyComponents, pinchEnd);
        assert(adjacencyComponent != NULL);
        if (stHash_search(adjacencyComponentsToCactusNodes, adjacencyComponent) == NULL) {
            if (isDeadEndStubComponent(adjacencyComponent, pinchEnd)) { //Going to be a bridge to nowhere, so we join it - this ensures
                //that all dead end nodes of free stubs end up in the same node as their non-dead end counterparts.
                assert(pinchEnd == stList_get(adjacencyComponent, 0));
                stPinchEnd otherPinchEnd = stPinchEnd_constructStatic(stPinchEnd_getBlock(pinchEnd), !stPinchEnd_getOrientation(pinchEnd));
                stList *otherAdjacencyComponent = stHash_search(pinchEndsToAdjacencyComponents, &otherPinchEnd);
                assert(otherAdjacencyComponent != NULL && adjacencyComponent != otherAdjacencyComponent);
                stCactusNode *cactusNode = stHash_search(adjacencyComponentsToCactusNodes, otherAdjacencyComponent);
                if (cactusNode == NULL) {
                    cactusNode = stCactusNode_construct(cactusGraph, makeNodeObject(otherAdjacencyComponent));
                    stHash_insert(adjacencyComponentsToCactusNodes, otherAdjacencyComponent, cactusNode);
                }
                stList *adjacencyComponents = stCactusNode_getObject(cactusNode);
                stList_append(adjacencyComponents, adjacencyComponent);
                stHash_insert(adjacencyComponentsToCactusNodes, adjacencyComponent, cactusNode);
            } else {
                stHash_insert(adjacencyComponentsToCactusNodes, adjacencyComponent,
                        stCactusNode_construct(cactusGraph, makeNodeObject(adjacencyComponent)));
            }
        }
    }
    stHash_destructIterator(pinchEndIt);

    //Make the edges
    pinchEndIt = stHash_getIterator(pinchEndsToAdjacencyComponents);
    while ((pinchEnd = stHash_getNext(pinchEndIt)) != NULL) {
        if (stPinchEnd_getOrientation(pinchEnd)) { //Assure we make the edge only once
            assert(stPinchEnd_getBlock(pinchEnd) != NULL);
            stPinchEnd pinchEnd2Static = stPinchEnd_constructStatic(stPinchEnd_getBlock(pinchEnd), 0);
            stPinchEnd *pinchEnd2 = stHash_search(pinchEndsToNonStaticPinchEnds, &pinchEnd2Static);
            assert(pinchEnd2 != NULL);
            assert(pinchEnd != pinchEnd2);
            stCactusNode *cactusNode1 = getCactusNode(pinchEnd, pinchEndsToAdjacencyComponents, adjacencyComponentsToCactusNodes);
            stCactusNode *cactusNode2 = getCactusNode(pinchEnd2, pinchEndsToAdjacencyComponents, adjacencyComponentsToCactusNodes);
            stCactusEdgeEnd_construct(cactusGraph, cactusNode1, cactusNode2, pinchEnd, pinchEnd2);
        }
    }
    stHash_destructIterator(pinchEndIt);
    stHash_destruct(pinchEndsToNonStaticPinchEnds);
    stHash_destruct(adjacencyComponentsToCactusNodes);

    //Run the cactus-ifying functions
    stCactusGraph_collapseToCactus(cactusGraph, stCaf_mergeNodeObjects, *startCactusNode);
    stCactusGraph_collapseBridges(cactusGraph, *startCactusNode, stCaf_mergeNodeObjects);
    if(breakChainsAtReverseTandems) {
        *startCactusNode = stCactusGraph_breakChainsByEndsNotInChains(cactusGraph, *startCactusNode, stCaf_mergeNodeObjects, stCaf_reversalAtEnd, NULL);
    }
    if(maximumMedianSpacingBetweenLinkedEnds < INT64_MAX) {
        *startCactusNode = stCactusGraph_breakChainsByEndsNotInChains(cactusGraph, *startCactusNode, stCaf_mergeNodeObjects, stCaf_breakAtTooGreatEnoughMedianSeparation, &maximumMedianSpacingBetweenLinkedEnds);
    }

    return cactusGraph;
}

///////////////////////////////////////////////////////////////////////////
// Function that draws together above functions to generate a cactus graph from a pinch graph.
///////////////////////////////////////////////////////////////////////////

stCactusGraph *getCactusGraphForThreadSet(stPinchThreadSet *threadSet, stCactusNode **startCactusNode,
        stList **deadEndComponent, bool attachEndsInFlower, int64_t minLengthForChromosome,
        double proportionOfUnalignedBasesForNewChromosome,
        bool breakChainsAtReverseTandems, int64_t maximumMedianSpacingBetweenLinkedEnds) {
    // Keep two sets for keeping track of which ends are attached for
    // each pinch thread. (Previously this was done by the cactus DB).
    stSet *leftEndAttached = stSet_construct();
    stSet *rightEndAttached = stSet_construct();

    //Get adjacency components
    stHash *pinchEndsToAdjacencyComponents;
    stList *adjacencyComponents = stPinchThreadSet_getAdjacencyComponents2(threadSet, &pinchEndsToAdjacencyComponents);
    stList_setDestructor(adjacencyComponents, NULL);
    stList_destruct(adjacencyComponents);

    //Merge together dead end component
    *deadEndComponent = stCaf_constructDeadEndComponent(threadSet, pinchEndsToAdjacencyComponents, leftEndAttached, rightEndAttached);

    //Join unattached components of graph by dead ends to dead end component, and make other ends 'attached' if necessary
    stCaf_attachUnattachedThreadComponents(threadSet, *deadEndComponent, pinchEndsToAdjacencyComponents, attachEndsInFlower,
                                           minLengthForChromosome, proportionOfUnalignedBasesForNewChromosome,
                                           leftEndAttached, rightEndAttached);

    //Create cactus
    stCactusGraph *cactusGraph = stCaf_constructCactusGraph(*deadEndComponent, pinchEndsToAdjacencyComponents, startCactusNode,
            breakChainsAtReverseTandems, maximumMedianSpacingBetweenLinkedEnds);

    //Cleanup (the memory is owned by the cactus graph, so this does not break anything)
    stHash_destruct(pinchEndsToAdjacencyComponents);

    stSet_destruct(leftEndAttached);
    stSet_destruct(rightEndAttached);

    return cactusGraph;
}
