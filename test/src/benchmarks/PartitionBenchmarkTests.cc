#include "Operators.h"
#include "Physical.h"
#include <gtest/gtest.h>
#include <AssertVideo.h>

using namespace visualcloud;
using namespace std::chrono;

class PartitionBenchmarkTestFixture : public testing::Test {
public:
    PartitionBenchmarkTestFixture()
            : name("result"),
              pi_div_2(102928, 2*32763)
    { }

    const rational pi_div_2;
    const char *name;

    void partitioningBenchmark(std::string dataset, size_t size, size_t frames, size_t height, size_t width) {
        auto source = std::string("../../benchmarks/datasets/") + dataset + '/' + dataset + std::to_string(size) + "K.h264";
        auto start = steady_clock::now();

        Decode<EquirectangularGeometry>(source)
                >> Partition(Dimension::Theta, pi_div_2)
                >> Encode<YUVColorSpace>()
                >> Store(name);

        LOG(INFO) << source << " time:" << ::duration_cast<milliseconds>(steady_clock::now() - start).count() << "ms";

        EXPECT_VIDEO_VALID(name);
        //EXPECT_VIDEO_FRAMES(name, frames);
        //EXPECT_VIDEO_RESOLUTION(name, height, width);
        EXPECT_EQ(remove(name), 0);
    }
};

TEST_F(PartitionBenchmarkTestFixture, testPartitionBenchmark_1K) {
    partitioningBenchmark("timelapse", 1, 2700, 512, 1024/4);
}

TEST_F(PartitionBenchmarkTestFixture, testPartitionBenchmark_2K) {
    partitioningBenchmark("timelapse", 2, 2700, 1024, 2048/4);
}

TEST_F(PartitionBenchmarkTestFixture, testPartitionBenchmark_4K) {
    partitioningBenchmark("timelapse", 4, 2700, 1920, 3840/4);
}
