
// Required includes
// #include <stdint.h>
// #include <iostream>
// #include <vector>

class Histogram final
{

public:

    class HistogramValue final
    {
        friend class Histogram;
    public:
        HistogramValue();
        ~HistogramValue();
    private:
        int64_t valueIteratedTo;
        int64_t valueIteratedFrom;
        int64_t countAtValueIteratedTo;
        int64_t countAddedInThisIterationStep;
        int64_t totalCountToThisValue;
        int64_t totalValueToThisValue;
        double percentile;
        double percentileLevelIteratedTo;
    };

    Histogram(int64_t highestTrackableValue,
              int64_t numberOfSignificantValueDigits);
    ~Histogram();

    int64_t getHighestTrackableValue() const;
    int64_t getNumberOfSignificantValueDigits() const;
    int64_t getTotalCount() const;
    int64_t getCountAtValue(int64_t value) const;

    void forAll(std::function<void (const int64_t value, const int64_t count)> func) const;
    // void forAllValues(std::function<void (const HistogramValue& histogramValue)> func) const;

    int64_t getMaxValue()  const;
    int64_t getMinValue()  const;
    double getMeanValue() const;
    int64_t medianEquivalentValue(int64_t value) const;
    int64_t sizeOfEquivalentRange(int64_t value) const;
    int64_t getValueAtPercentile(double percentile) const;
    double getPercentileAtOrBelowValue(int64_t value) const;
    int64_t getCountBetweenValues(int64_t lo, int64_t hi) const;

    void recordValue(int64_t value);
    void recordValue(int64_t value, int64_t expectedInterval);

    void print( std::ostream& stream ) const;
    bool valuesAreEquivalent(int64_t a, int64_t b) const;

private:
    int64_t identityCount;
    int64_t highestTrackableValue;
    int64_t numberOfSignificantValueDigits;
    int32_t subBucketHalfCountMagnitude;
    int32_t subBucketHalfCount;
    int64_t subBucketMask;
    int32_t subBucketCount;
    int32_t bucketCount;
    int32_t countsArrayLength;
    int64_t totalCount;
    std::vector< int64_t > counts;

    void init();

    int32_t getBucketIndex(int64_t value) const;
    int32_t getSubBucketIndex(int64_t value, int32_t bucketIndex) const;
    int32_t countsArrayIndex(int32_t bucketIndex, int32_t subBucketIndex) const;
    int32_t countsIndexFor(int64_t value) const;
    int64_t getCountAtIndex(int32_t bucketIndex, int32_t subBucketIndex) const;

    void incrementCountAtIndex(int32_t countsIndex);
    void incrementTotalCount();

    int64_t lowestEquivalentValue(int64_t a) const;
};

std::ostream& operator<< (std::ostream& stream, const Histogram& matrix);