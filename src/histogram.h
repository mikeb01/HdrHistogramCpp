
// Required includes
// #include <stdint.h>
// #include <iostream>
// #include <vector>

class Histogram final
{

public:
    Histogram( uint64_t highestTrackableValue,
               uint64_t numberOfSignificantValueDigits );
    ~Histogram();

    uint64_t getHighestTrackableValue() const;
    uint64_t getNumberOfSignificantValueDigits() const;
    uint64_t getTotalCount() const;
    uint64_t getCountAtValue(uint64_t value) const;

    void forAll(std::function<void (const uint64_t value, const uint64_t count)> func) const;
    uint64_t getMaxValue()  const;
    uint64_t getMinValue()  const;
    double getMeanValue() const;
    uint64_t medianEquivalentValue(uint64_t value) const;
    uint64_t sizeOfEquivalentRange(uint64_t value) const;

    void recordValue(uint64_t value);
    void recordValue(uint64_t value, uint64_t expectedInterval);

    void print( std::ostream& stream ) const;
    bool valuesAreEquivalent(uint64_t a, uint64_t b) const;

private:
    uint64_t identityCount;
    uint64_t highestTrackableValue;
    uint64_t numberOfSignificantValueDigits;
    uint32_t subBucketHalfCountMagnitude;
    uint32_t subBucketHalfCount;
    uint64_t subBucketMask;
    uint32_t subBucketCount;
    uint32_t bucketCount;
    uint32_t countsArrayLength;
    uint64_t totalCount;
    std::vector< uint64_t > counts;

    void init();

    uint32_t getBucketIndex(uint64_t value) const;
    uint32_t getSubBucketIndex(uint64_t value, uint32_t bucketIndex) const;
    uint32_t countsArrayIndex(uint32_t bucketIndex, uint32_t subBucketIndex) const;
    uint32_t countsIndexFor(uint64_t value) const;
    uint64_t getCountAtIndex(uint32_t bucketIndex, uint32_t subBucketIndex) const;

    void incrementCountAtIndex(uint32_t countsIndex);
    void incrementTotalCount();

    uint64_t lowestEquivalentValue(uint64_t a) const;
};

std::ostream& operator<< (std::ostream& stream, const Histogram& matrix);