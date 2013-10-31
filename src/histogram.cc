
#include <stdint.h>
#include <x86intrin.h>
#include <math.h>
#include <assert.h>

#include <iostream>
#include <vector>
#include <functional>

#include "histogram.h"

static uint64_t power(uint64_t base, uint64_t exp)
{
    int result = 1;
    while(exp)
    {
        result *= base; exp--;
    }
    return result;
}

static inline uint64_t valueFromIndex(uint32_t bucketIndex, uint32_t subBucketIndex)
{
    return ((uint64_t) subBucketIndex) << bucketIndex;
}

Histogram::Histogram( uint64_t highestTrackableValue,
                      uint64_t numberOfSignificantValueDigits ) :
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
    auto subBucketCountMagnitude = (uint32_t) ceil(log(largestValueWithSingleUnitResolution)/log(2));

    subBucketHalfCountMagnitude = ((subBucketCountMagnitude > 1) ? subBucketCountMagnitude : 1) - 1;

    subBucketCount     = (uint32_t) pow(2, (subBucketHalfCountMagnitude + 1));
    subBucketHalfCount = subBucketCount / 2;
    subBucketMask      = subBucketCount - 1;

    // determine exponent range needed to support the trackable value with no overflow:
    auto trackableValue = (uint64_t) subBucketCount - 1;
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

uint64_t Histogram::getHighestTrackableValue() const
{
    return highestTrackableValue;
}

uint64_t Histogram::getNumberOfSignificantValueDigits() const
{
    return numberOfSignificantValueDigits;
}

uint64_t Histogram::getTotalCount() const
{
    return totalCount;
}

void Histogram::forAll(std::function<void (const uint64_t value, const uint64_t count)> func) const
{
    uint32_t bucketIndex      = 0;
    uint32_t subBucketIndex   = 0;
    uint32_t countToIndex     = 0;
    uint64_t valueAtThisIndex = 0;

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

        valueAtThisIndex = subBucketIndex << bucketIndex;
    }
}

uint64_t Histogram::getMaxValue() const
{
    uint64_t maxValue = 0;

    forAll([&] (uint64_t value, uint64_t count)
    {
        if (0 != count)
        {
            maxValue = value;
        }
    });

    return maxValue;
}

uint64_t Histogram::getMinValue() const
{
    uint64_t minValue = 0;

    forAll([&] (uint64_t value, uint64_t count)
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
    uint64_t totalValue = 0;

    forAll([&] (uint64_t value, uint64_t count)
    {
        if (0 != count)
        {
            totalValue += count * medianEquivalentValue(value);
        }
    });

    return (totalValue * 1.0) / totalCount;
}

uint64_t Histogram::getValueAtPercentile(double requestedPercentile) const
{
    auto percentile        = fmin(fmax(requestedPercentile, 0), 100.0);
    auto countAtPercentile = (uint64_t) (((percentile / 100.0) * totalCount) + 0.5);
    countAtPercentile      = (uint64_t) countAtPercentile > 1 ? countAtPercentile : 1;

    auto totalToCurrentIJ = 0UL;
    for (uint32_t i = 0; i < bucketCount; i++)
    {
        uint32_t j = (i == 0) ? 0 : (subBucketCount / 2);
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

double Histogram::getPercentileAtOrBelowValue(uint64_t value) const
{
    auto totalToCurrentIJ = 0ULL;

    auto targetBucketIndex    = getBucketIndex(value);
    auto targetSubBucketIndex = getSubBucketIndex(value, targetBucketIndex);

    if (targetBucketIndex >= bucketCount)
    {
        return 100.0;
    }

    for (uint32_t i = 0; i <= targetBucketIndex; i++)
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

uint64_t Histogram::getCountBetweenValues(uint64_t lo, uint64_t hi) const
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

uint32_t Histogram::countsIndexFor(uint64_t value) const
{
    auto bucketIndex    = getBucketIndex(value);
    auto subBucketIndex = getSubBucketIndex(value, bucketIndex);
    auto countsIndex    = countsArrayIndex(bucketIndex, subBucketIndex);

    return countsIndex;
}

uint64_t Histogram::getCountAtValue(uint64_t value) const
{
    return counts[countsIndexFor(value)];
}

uint64_t Histogram::getCountAtIndex(uint32_t bucketIndex, uint32_t subBucketIndex) const
{
    return counts[countsArrayIndex(bucketIndex, subBucketIndex)];
}

uint32_t Histogram::getSubBucketIndex(uint64_t value, uint32_t bucketIndex) const
{
    return (uint32_t)(value >> bucketIndex);
}

uint32_t Histogram::countsArrayIndex(uint32_t bucketIndex, uint32_t subBucketIndex) const
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

void Histogram::recordValue(uint64_t value)
{
    incrementCountAtIndex(countsIndexFor(value));
    incrementTotalCount();
}

void Histogram::recordValue(uint64_t value, uint64_t expectedInterval)
{
    recordValue(value);
    if (expectedInterval <= 0 || value <= expectedInterval)
    {
        return;
    }
    uint64_t missingValue = value - expectedInterval;
    for (; missingValue >= expectedInterval; missingValue -= expectedInterval)
    {
        recordValue(missingValue);
    }
}


uint32_t Histogram::getBucketIndex(uint64_t value) const
{
    auto pow2ceiling = 64 - __lzcnt64(value | subBucketMask); // smallest power of 2 containing value
    return pow2ceiling - (subBucketHalfCountMagnitude + 1);
}

void Histogram::incrementCountAtIndex(uint32_t countsIndex)
{
    counts[countsIndex]++;
}

void Histogram::incrementTotalCount()
{
    totalCount++;
}

/////////////////// Utility /////////////////////

bool Histogram::valuesAreEquivalent(uint64_t a, uint64_t b) const
{
    return lowestEquivalentValue(a) == lowestEquivalentValue(b);
}

uint64_t Histogram::lowestEquivalentValue(uint64_t value) const
{
    auto bucketIndex    = getBucketIndex(value);
    auto subBucketIndex = getSubBucketIndex(value, bucketIndex);

    return valueFromIndex(bucketIndex, subBucketIndex);
}

uint64_t Histogram::medianEquivalentValue(uint64_t value) const
{
    return lowestEquivalentValue(value) + (sizeOfEquivalentRange(value) >> 1);
}

uint64_t Histogram::sizeOfEquivalentRange(uint64_t value) const
{
    auto bucketIndex    = getBucketIndex(value);
    auto subBucketIndex = getSubBucketIndex(value, bucketIndex);
    uint64_t distanceToNextValue = (1 << ((subBucketIndex >= subBucketCount) ? (bucketIndex + 1) : bucketIndex));
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