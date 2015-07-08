// Simple tests to make sure halToPinch is actually returning the
// correct pinch graph.
#include <stdlib.h>
#include "halToPinch.h"
#include "CuTest.h"

// Check if two positions are aligned (in 0-based coordinates, not
// 2-based Cactus coordinates).
static bool assertPositionsAreHomologous(stPinchThread *thread1, int64_t pos1,
                                         stPinchThread *thread2, int64_t pos2) {
    // Shift coordinates by two to match cactus coordinates--note that
    // this doesn't modify pos1 or pos2 outside this scope.
    pos1 += 2;
    pos2 += 2;
    stPinchSegment *segment1 = stPinchThread_getSegment(thread1, pos1);
    stPinchSegment *segment2 = stPinchThread_getSegment(thread2, pos2);
    stPinchBlock *block1 = stPinchSegment_getBlock(segment1);
    stPinchBlock *block2 = stPinchSegment_getBlock(segment2);
    if (block1 == NULL || block1 != block2) {
        return false;
    }
    int64_t relativePos1, relativePos2;
    if (stPinchSegment_getBlockOrientation(segment1) != stPinchSegment_getBlockOrientation(segment2)) {
        // reverse orientation
        relativePos1 = pos1 - stPinchSegment_getStart(segment1);
        int64_t segmentEnd2 = stPinchSegment_getStart(segment2)
            + stPinchSegment_getLength(segment2) - 1;
        relativePos2 = segmentEnd2 - pos2;
    } else {
        // same orientation
        relativePos1 = pos1 - stPinchSegment_getStart(segment1);
        relativePos2 = pos2 - stPinchSegment_getStart(segment2);
    }
    return relativePos1 == relativePos2;
}

static void test_pairwiseHalToPinch(CuTest *testCase) {
    stHash *sequenceLabelToThread = NULL;
    stPinchThreadSet *threadSet = pairwiseHalToPinch("tests/testData/test.hal",
                                                     "leaf1", "leaf2",
                                                     &sequenceLabelToThread);
    CuAssertTrue(testCase,
                 stHash_size(sequenceLabelToThread) == stPinchThreadSet_getSize(threadSet));
    stPinchThread *thread1 = stHash_search(sequenceLabelToThread,
                                           sequenceLabel_construct("leaf1", "Sequence"));
    CuAssertTrue(testCase, thread1 != NULL);
    stPinchThread *thread2 = stHash_search(sequenceLabelToThread,
                                           sequenceLabel_construct("leaf2", "Sequence"));
    CuAssertTrue(testCase, thread2 != NULL);
    CuAssertTrue(testCase, stHash_search(sequenceLabelToThread,
                                         sequenceLabel_construct("leaf3", "Sequence")) == NULL);
    CuAssertTrue(testCase, assertPositionsAreHomologous(thread1, 0, thread2, 19));
    CuAssertTrue(testCase, assertPositionsAreHomologous(thread1, 19, thread2, 40));
}

static CuSuite *halToPinchTestSuite(void) {
    CuSuite *suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, test_pairwiseHalToPinch);
    return suite;
}

int main(void) {
    CuString *output = CuStringNew();
    CuSuite *suite = CuSuiteNew();
    CuSuiteAddSuite(suite, halToPinchTestSuite());

    CuSuiteRun(suite);
    CuSuiteSummary(suite, output);
    CuSuiteDetails(suite, output);
    printf("%s\n", output->buffer);
    return suite->failCount > 0;
}
