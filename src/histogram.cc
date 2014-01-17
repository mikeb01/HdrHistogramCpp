
#include <stdint.h>
#include <x86intrin.h>
#include <math.h>
#include <assert.h>

#include <iostream>
#include <vector>
#include <functional>

#include "histogram.h"

static int64_t power(int64_t base, int64_t exp)
{
    int result = 1;
    while(exp)
    {
        result *= base; exp--;
    }
    return result;
}

static inline int64_t valueFromIndex(int32_t bucketIndex, int32_t subBucketIndex)
{
    return ((int64_t) subBucketIndex) << bucketIndex;
}

Histogram::HistogramValue::HistogramValue() :
    valueIteratedTo{ 0 },
    valueIteratedFrom{ 0 },
    countAtValueIteratedTo{ 0 },
    countAddedInThisIterationStep{ 0 },
    totalCountToThisValue{ 0 },
    totalValueToThisValue{ 0 },
    percentile{ 0.0 },
    percentileLevelIteratedTo{ 0.0 }
{
}

Histogram::HistogramValue::~HistogramValue()
{
}

Histogram::Histogram(int64_t highestTrackableValue,
                     int64_t numberOfSignificantValueDigits) :
    highestTrackableValue{ highestTrackableValue },
    numberOfSignificantValueDigits{ numberOfSignificantValueDigits },
    counts{ 0 },
    totalCount{ 0 }
{
    init();
    counts.resize(countsArrayLength);
}

Histogram::~Histogram()
{
}

void Histogram::init()
{
    auto largestValueWithSingleUnitResolution = 2 * power(10, numberOfSignificantValueDigits);
    auto subBucketCountMagnitude = (int32_t) ceil(log(largestValueWithSingleUnitResolution)/log(2));

    subBucketHalfCountMagnitude = ((subBucketCountMagnitude > 1) ? subBucketCountMagnitude : 1) - 1;

    subBucketCount     = (int32_t) pow(2, (subBucketHalfCountMagnitude + 1));
    subBucketHalfCount = subBucketCount / 2;
    subBucketMask      = subBucketCount - 1;

    // determine exponent range needed to support the trackable value with no overflow:
    auto trackableValue = (int64_t) subBucketCount - 1;
    auto bucketsNeeded = 1;
    while (trackableValue < highestTrackableValue)
    {
        trackableValue <<= 1;
        bucketsNeeded++;
    }
    bucketCount = bucketsNeeded;
    countsArrayLength = (bucketCount + 1) * (subBucketCount / 2);
}

/////////////////// Properties /////////////////////

int64_t Histogram::getHighestTrackableValue() const
{
    return highestTrackableValue;
}

int64_t Histogram::getNumberOfSignificantValueDigits() const
{
    return numberOfSignificantValueDigits;
}

int64_t Histogram::getTotalCount() const
{
    return totalCount;
}

void Histogram::forAll(std::function<void (const int64_t value, const int64_t count)> func) const
{
    int32_t bucketIndex      = 0;
    int32_t subBucketIndex   = 0;
    int32_t countToIndex     = 0;
    int64_t valueAtThisIndex = 0;

    while (countToIndex < totalCount)
    {
        auto countAtThisIndex = getCountAtIndex(bucketIndex, subBucketIndex);
        func(valueAtThisIndex, countAtThisIndex);

        countToIndex += countAtThisIndex;
        subBucketIndex++;

        if (subBucketIndex >= subBucketCount)
        {
            subBucketIndex = subBucketHalfCount;
            bucketIndex++;
        }

        valueAtThisIndex = valueFromIndex(bucketIndex, subBucketIndex);
    }
}

void Histogram::forPercentiles(const int32_t tickPerHalfDistance, std::function<void (const double percentileFrom, const double percentileTo, const int64_t value)> func) const
{
    double percentileIteratedTo   = 0.0;
    double percentileIteratedFrom = 0.0;
    int64_t countToIndex = 0;
    const int64_t totalCount = getTotalCount();

    forAll([&] (int64_t value, int64_t count)
    {
        countToIndex += count;

        double currentPercentile = (100.0 * (double) countToIndex) / totalCount;

        if (currentPercentile >= percentileIteratedTo)
        {
            percentileIteratedFrom = percentileIteratedTo;
            int64_t percentileReportingTicks = tickPerHalfDistance * pow(2, (log(100 / (100.0 - (percentileIteratedTo))) / log(2)) + 1);
            percentileIteratedTo += 100.0 / percentileReportingTicks;

            func(percentileIteratedFrom, percentileIteratedTo, value);
        }
    });
}

// void Histogram::forAllValues(std::function<void (const HistogramValue& histogramValue)> func) const
// {
//     HistogramValue histogramValue{};

//     forAll([&] (int64_t value, int64_t count)
//     {
//         histogramValue.valueIteratedTo   = highestEquivalentValue(value);
//         histogramValue.valueIteratedFrom = histogramValue.valueIteratedTo;

//         histogramValue.countAtValueIteratedTo        = count;
//         histogramValue.countAddedInThisIterationStep = count;

//         histogramValue.totalCountToThisValue += count;
//         histogramValue.totalValueToThisValue += count * medianEquivalentValue(value);

//         func(histogramValue);
//     });

//     return;
// }

int64_t Histogram::getMaxValue() const
{
    int64_t maxValue = 0;

    forAll([&] (int64_t value, int64_t count)
    {
        if (0 != count)
        {
            maxValue = value;
        }
    });

    return maxValue;
}

int64_t Histogram::getMinValue() const
{
    int64_t minValue = 0;

    forAll([&] (int64_t value, int64_t count)
    {
        if (0 != count && 0 == minValue)
        {
            minValue = value;
        }
    });

    return minValue;
}

double Histogram::getMeanValue() const
{
    int64_t totalValue = 0;

    forAll([&] (int64_t value, int64_t count)
    {
        if (0 != count)
        {
            totalValue += count * medianEquivalentValue(value);
        }
    });

    return (totalValue * 1.0) / totalCount;
}

int64_t Histogram::getValueAtPercentile(double requestedPercentile) const
{
    auto percentile        = fmin(fmax(requestedPercentile, 0), 100.0);
    auto countAtPercentile = (int64_t) (((percentile / 100.0) * totalCount) + 0.5);
    countAtPercentile      = (int64_t) countAtPercentile > 1 ? countAtPercentile : 1;

    auto totalToCurrentIJ = 0UL;
    for (int32_t i = 0; i < bucketCount; i++)
    {
        int32_t j = (i == 0) ? 0 : (subBucketCount / 2);
        for (; j < subBucketCount; j++)
        {
            totalToCurrentIJ += getCountAtIndex(i, j);
            if (totalToCurrentIJ >= countAtPercentile)
            {
                return valueFromIndex(i, j);
            }
        }
    }

    return 0;
}

double Histogram::getPercentileAtOrBelowValue(int64_t value) const
{
    auto totalToCurrentIJ = 0ULL;

    auto targetBucketIndex    = getBucketIndex(value);
    auto targetSubBucketIndex = getSubBucketIndex(value, targetBucketIndex);

    if (targetBucketIndex >= bucketCount)
    {
        return 100.0;
    }

    for (int32_t i = 0; i <= targetBucketIndex; i++)
    {
        auto j = (i == 0) ? 0 : (subBucketCount / 2);
        auto subBucketCap = (i == targetBucketIndex) ? (targetSubBucketIndex + 1) : subBucketCount;

        for (; j < subBucketCap; j++)
        {
            totalToCurrentIJ += getCountAtIndex(i, j);
        }
    }

    return (100.0 * totalToCurrentIJ) / getTotalCount();
}

int64_t Histogram::getCountBetweenValues(int64_t lo, int64_t hi) const
{
    auto count = 0ULL;

    auto loBucketIndex    = getBucketIndex(lo);
    auto loSubBucketIndex = getSubBucketIndex(lo, loBucketIndex);
    auto valueAtLo        = valueFromIndex(loBucketIndex, loSubBucketIndex);

    auto hiBucketIndex    = getBucketIndex(hi);
    auto hiSubBucketIndex = getSubBucketIndex(hi, hiBucketIndex);
    auto valueAtHi        = valueFromIndex(hiBucketIndex, hiSubBucketIndex);

    if (loBucketIndex >= bucketCount || hiBucketIndex >= bucketCount)
    {
        return 0;
    }

    for (auto i = loBucketIndex; i <= hiBucketIndex; i++)
    {
        auto j = (i == 0) ? 0 : (subBucketCount / 2);
        for (; j < subBucketCount; j++)
        {
            auto valueAtIndex = valueFromIndex(i, j);
            if (valueAtIndex > valueAtHi)
            {
                return count;
            }
            if (valueAtIndex >= valueAtLo)
            {
                count += getCountAtIndex(i, j);
            }
        }
    }

    return count;
}

/////////////////// Index Calcuations /////////////////////

int32_t Histogram::countsIndexFor(int64_t value) const
{
    auto bucketIndex    = getBucketIndex(value);
    auto subBucketIndex = getSubBucketIndex(value, bucketIndex);
    auto countsIndex    = countsArrayIndex(bucketIndex, subBucketIndex);

    return countsIndex;
}

int64_t Histogram::getCountAtValue(int64_t value) const
{
    return counts[countsIndexFor(value)];
}

int64_t Histogram::getCountAtIndex(int32_t bucketIndex, int32_t subBucketIndex) const
{
    return counts[countsArrayIndex(bucketIndex, subBucketIndex)];
}

int32_t Histogram::getSubBucketIndex(int64_t value, int32_t bucketIndex) const
{
    return (int32_t)(value >> bucketIndex);
}

int32_t Histogram::countsArrayIndex(int32_t bucketIndex, int32_t subBucketIndex) const
{
    assert(subBucketIndex < subBucketCount);
    assert(bucketIndex < bucketCount);
    assert(bucketIndex == 0 || (subBucketIndex >= subBucketHalfCount));

    // Calculate the index for the first entry in the bucket:
    // (The following is the equivalent of ((bucketIndex + 1) * subBucketHalfCount) ):
    auto bucketBaseIndex = (bucketIndex + 1) << subBucketHalfCountMagnitude;
    // Calculate the offset in the bucket:
    auto offsetInBucket = subBucketIndex - subBucketHalfCount;
    // The following is the equivalent of ((subBucketIndex  - subBucketHalfCount) + bucketBaseIndex;
    return bucketBaseIndex + offsetInBucket;
}

/////////////////// Value Recording /////////////////////

void Histogram::recordValue(int64_t value)
{
    incrementCountAtIndex(countsIndexFor(value));
    incrementTotalCount();
}

void Histogram::recordValue(int64_t value, int64_t expectedInterval)
{
    recordValue(value);
    if (expectedInterval <= 0 || value <= expectedInterval)
    {
        return;
    }
    int64_t missingValue = value - expectedInterval;
    for (; missingValue >= expectedInterval; missingValue -= expectedInterval)
    {
        recordValue(missingValue);
    }
}


int32_t Histogram::getBucketIndex(int64_t value) const
{
    auto pow2ceiling = 64 - __lzcnt64(value | subBucketMask); // smallest power of 2 containing value
    return pow2ceiling - (subBucketHalfCountMagnitude + 1);
}

void Histogram::incrementCountAtIndex(int32_t countsIndex)
{
    counts[countsIndex]++;
}

void Histogram::incrementTotalCount()
{
    totalCount++;
}

/////////////////// Utility /////////////////////

bool Histogram::valuesAreEquivalent(int64_t a, int64_t b) const
{
    return lowestEquivalentValue(a) == lowestEquivalentValue(b);
}

int64_t Histogram::lowestEquivalentValue(int64_t value) const
{
    auto bucketIndex    = getBucketIndex(value);
    auto subBucketIndex = getSubBucketIndex(value, bucketIndex);

    return valueFromIndex(bucketIndex, subBucketIndex);
}

int64_t Histogram::medianEquivalentValue(int64_t value) const
{
    return lowestEquivalentValue(value) + (sizeOfEquivalentRange(value) >> 1);
}

int64_t Histogram::sizeOfEquivalentRange(int64_t value) const
{
    auto bucketIndex    = getBucketIndex(value);
    auto subBucketIndex = getSubBucketIndex(value, bucketIndex);
    int64_t distanceToNextValue = (1 << ((subBucketIndex >= subBucketCount) ? (bucketIndex + 1) : bucketIndex));
    return distanceToNextValue;
}

void Histogram::print(std::ostream& stream) const
{
    stream << "identityCount: "                  << identityCount <<
            ", highestTrackableValue: "          << highestTrackableValue <<
            ", numberOfSignificantValueDigits: " << numberOfSignificantValueDigits <<
            ", subBucketHalfCountMagnitude: "    << subBucketHalfCountMagnitude <<
            ", subBucketHalfCount: "             << subBucketHalfCount <<
            ", subBucketMask: "                  << subBucketMask <<
            ", subBucketCount: "                 << subBucketCount <<
            ", bucketCount: "                    << bucketCount <<
            ", countsArrayLength: "              << countsArrayLength;
}

std::ostream& operator<< (std::ostream& stream, const Histogram& histogram)
{
    histogram.print(stream);
    return stream;
}