// Simple wrapper script to convert a hal alignment into a pinch graph.
#include "hal.h"
#include "sonLib.h"
#include "halToPinch.h"
extern "C" {
#include "stPinchGraphs.h"
}

using namespace std;
using namespace hal;

sequenceLabel *sequenceLabel_construct(const char *genomeName,
                                       const char *sequenceName) {
    sequenceLabel *ret = (sequenceLabel *) malloc(sizeof(sequenceLabel));
    ret->genomeName = stString_copy(genomeName);
    ret->sequenceName = stString_copy(sequenceName);
    return ret;
}

uint64_t sequenceLabel_hashKey(const sequenceLabel *label) {
    return stHash_stringKey(label->genomeName) ^ stHash_stringKey(label->sequenceName);
}

int sequenceLabel_equals(const sequenceLabel *label1, const sequenceLabel *label2) {
    return !(strcmp(label1->genomeName, label2->genomeName)
             || strcmp(label1->sequenceName, label2->sequenceName));
}

void sequenceLabel_destruct(sequenceLabel *label) {
    free(label->genomeName);
    free(label->sequenceName);
    free(label);
}

static stPinchThread *getThreadForSequence(stPinchThreadSet *threadSet,
                                           const Sequence *sequence,
                                           stHash *sequenceLabelToThread) {
    int64_t threadId = (int64_t) sequence;
    if (stPinchThreadSet_getThread(threadSet, threadId) == NULL) {
        stPinchThread *thread = stPinchThreadSet_addThread(threadSet, threadId, 1,
                                                           sequence->getSequenceLength() + 2);
        // Add in the caps. Realistically there isn't any pressing
        // need to have these, but it makes it easier to adapt the
        // existing cactus code.
        stPinchThread_split(thread, 1);
        stPinchThread_split(thread, sequence->getSequenceLength() + 1);
        stPinchSegment *_5PrimeSegment = stPinchThread_getFirst(thread);
        stPinchSegment *_3PrimeSegment = stPinchThread_getLast(thread);
        assert(stPinchSegment_getLength(_5PrimeSegment) == 1);
        assert(stPinchSegment_getLength(_3PrimeSegment) == 1);
        stPinchBlock_construct3(_5PrimeSegment, 1);
        stPinchBlock_construct3(_3PrimeSegment, 0);

        const char *genomeName = sequence->getGenome()->getName().c_str();
        const char *sequenceName = sequence->getName().c_str();
        sequenceLabel *threadLabel = sequenceLabel_construct(genomeName,
                                                             sequenceName);
        stHash_insert(sequenceLabelToThread, threadLabel, thread);
    }
    return stPinchThreadSet_getThread(threadSet, threadId);
}

static void addPinch(stPinchThreadSet *threadSet, stHash *sequenceLabelToThread,
                     const Sequence *srcSequence, hal_index_t srcStart, hal_index_t srcStop,
                     const Sequence *destSequence, hal_index_t destStart, hal_index_t destStop,
                     bool destReversed) {
    stPinchThread *srcThread = getThreadForSequence(threadSet, srcSequence,
                                                    sequenceLabelToThread);
    stPinchThread *destThread = getThreadForSequence(threadSet, destSequence,
                                                     sequenceLabelToThread);
    hal_index_t length = srcStop - srcStart;
    (void) destStop; // to stop gcc whining about unused parameter if NDEBUG is defined
    assert(length == destStop - destStart);
    // Coordinates are shifted by 2 to follow the Cactus convention.
    stPinchThread_pinch(srcThread, destThread, srcStart + 2, destStart + 2, length, destReversed);
}

stPinchThreadSet *pairwiseHalToPinch(char *halPath, char *srcGenomeName,
                                     char *destGenomeName,
                                     stHash **sequenceLabelToThread) {
    stPinchThreadSet *threadSet = stPinchThreadSet_construct();

    *sequenceLabelToThread = stHash_construct3((uint64_t (*)(const void *)) sequenceLabel_hashKey,
                                               (int (*)(const void *, const void *)) sequenceLabel_equals,
                                               (void (*)(void *)) sequenceLabel_destruct,
                                                NULL);

    AlignmentConstPtr alignment = hdf5AlignmentInstanceReadOnly();
    alignment->open(halPath);
    const Genome *srcGenome = alignment->openGenome(srcGenomeName);
    const Genome *destGenome = alignment->openGenome(destGenomeName);
    SegmentIteratorConstPtr segIt;
    hal_size_t numSegments;
    if (srcGenome->getParent() != NULL) {
        segIt = srcGenome->getTopSegmentIterator();
        numSegments = srcGenome->getNumTopSegments();
    } else {
        segIt = srcGenome->getBottomSegmentIterator();
        numSegments = srcGenome->getNumBottomSegments();
    }

    while ((size_t) segIt->getArrayIndex() < numSegments) {
        // For each source segment, map the segment to the dest
        // genome, then add the pinch to the graph.
        set<MappedSegmentConstPtr> segSet;
        segIt->getMappedSegments(segSet, destGenome);
        const Sequence *srcSequence = segIt->getSequence();

        for (set<MappedSegmentConstPtr>::iterator destSegIt = segSet.begin();
             destSegIt != segSet.end();
             destSegIt++) {
            SlicedSegmentConstPtr srcSegPart = (*destSegIt)->getSource();
            hal_index_t srcSeqRelativeStart = srcSegPart->getStartPosition() - srcSequence->getStartPosition();
            // pinch region stop is exclusive so we need to add 1 to the end pos.
            hal_index_t srcSeqRelativeStop = srcSegPart->getEndPosition() - srcSequence->getStartPosition() + 1;
            const Sequence *destSequence = (*destSegIt)->getSequence();
            hal_index_t destSeqRelativeStart;
            hal_index_t destSeqRelativeStop;
            if ((*destSegIt)->getReversed()) {
                destSeqRelativeStart = (*destSegIt)->getEndPosition() - destSequence->getStartPosition();
                // pinch region stop is exclusive so we need to add 1 to the end pos.
                destSeqRelativeStop = (*destSegIt)->getStartPosition() - destSequence->getStartPosition() + 1;                
            } else {
                destSeqRelativeStart = (*destSegIt)->getStartPosition() - destSequence->getStartPosition();
                // pinch region stop is exclusive so we need to add 1 to the end pos.
                destSeqRelativeStop = (*destSegIt)->getEndPosition() - destSequence->getStartPosition() + 1;
            }
            addPinch(threadSet, *sequenceLabelToThread, srcSequence, srcSeqRelativeStart, srcSeqRelativeStop, destSequence, destSeqRelativeStart, destSeqRelativeStop, (*destSegIt)->getReversed());
        }
        segIt->toRight();
    }
    return threadSet;
}
