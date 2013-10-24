#include <iostream>
#include <vector>
#include <functional>
#include <UnitTest++.h>
#include <histogram.h>

TEST(ShouldUseAccessors)
{
    Histogram h{ 100000000, 3 };

    CHECK_EQUAL(3, h.getNumberOfSignificantValueDigits());
    CHECK_EQUAL(100000000, h.getHighestTrackableValue());
}

TEST(ShouldRecordValue)
{
    uint64_t testValue;
    Histogram h{ 100000000, 3 };

    h.recordValue(testValue);
    CHECK_EQUAL(1, h.getCountAtValue(testValue));
    CHECK_EQUAL(1, h.getTotalCount());
}

const uint64_t HIGHEST_TRACKABLE_VALUE = 3600000000;
const uint64_t SIGNIFICANT_DIGITS = 3;

void loadHistograms(Histogram& histogramA, Histogram& histogramB)
{
    for (int i = 0; i < 10000; i++)
    {
        histogramA.recordValue(1000L);
        histogramB.recordValue(1000L, 10000L);
    }

    histogramA.recordValue(100000000L);
    histogramB.recordValue(100000000L, 10000L);
}

void loadHistogramWithCorrection(Histogram& histogram)
{
}

TEST(ShouldGetTotalCount)
{
    Histogram histogram{ HIGHEST_TRACKABLE_VALUE, SIGNIFICANT_DIGITS };
    Histogram histogramCorrected{ HIGHEST_TRACKABLE_VALUE, SIGNIFICANT_DIGITS };
    loadHistograms(histogram, histogramCorrected);

    CHECK_EQUAL(10001, histogram.getTotalCount());
    CHECK_EQUAL(20000, histogramCorrected.getTotalCount());
}

TEST(ShouldGetMaxValue)
{
    Histogram histogram{ HIGHEST_TRACKABLE_VALUE, SIGNIFICANT_DIGITS };
    Histogram histogramCorrected{ HIGHEST_TRACKABLE_VALUE, SIGNIFICANT_DIGITS };
    loadHistograms(histogram, histogramCorrected);

    CHECK(histogram.valuesAreEquivalent(100000000L, histogram.getMaxValue()));
    CHECK(histogramCorrected.valuesAreEquivalent(100000000L, histogramCorrected.getMaxValue()));
}

TEST(ShouldGetMinValue)
{
    Histogram histogram{ HIGHEST_TRACKABLE_VALUE, SIGNIFICANT_DIGITS };
    Histogram histogramCorrected{ HIGHEST_TRACKABLE_VALUE, SIGNIFICANT_DIGITS };
    loadHistograms(histogram, histogramCorrected);

    CHECK(histogram.valuesAreEquivalent(1000L, histogram.getMinValue()));
    CHECK(histogramCorrected.valuesAreEquivalent(1000L, histogramCorrected.getMinValue()));
}

TEST(ShouldGetMeanValue)
{
    Histogram histogram{ HIGHEST_TRACKABLE_VALUE, SIGNIFICANT_DIGITS };
    Histogram histogramCorrected{ HIGHEST_TRACKABLE_VALUE, SIGNIFICANT_DIGITS };
    loadHistograms(histogram, histogramCorrected);

    double expectedMean = ((1000L * 10000.0) + (100000000L * 1.0))/10001;
    CHECK_CLOSE(expectedMean, histogram.getMeanValue(), expectedMean * 0.001);

    double expectedCorrectedMean = (1000.0 + 50000000.0)/2;
    CHECK_CLOSE(expectedCorrectedMean, histogramCorrected.getMeanValue(), expectedCorrectedMean * 0.001);
}