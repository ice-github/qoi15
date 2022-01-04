#include <gtest/gtest.h>
#include <iostream>
#include <filesystem>

#define ENABLE_STATICS
#include <qoi15.hpp>
#include <opencv2/opencv.hpp>

TEST(helloworld, simple)
{
    std::cout << "hello world!" << std::endl;
}

class PNG16
{
    std::string resolvePath(const std::string &relPath)
    {
        auto baseDir = std::filesystem::current_path();
        while (baseDir.has_parent_path())
        {
            auto combinePath = baseDir / relPath;
            if (std::filesystem::exists(combinePath))
            {
                return combinePath.string();
            }
            baseDir = baseDir.parent_path();
        }
        throw std::runtime_error("File not found!");
    }

    cv::Mat mono_;

public:
    cv::Mat Get()
    {
        return mono_;
    }

    PNG16(const std::string &path)
    {
        //read file
        auto absPath = resolvePath(path);
        cv::Mat image = cv::imread(absPath);

        //24bit color to 16bit mono
        cv::Mat mono(cv::Size(image.cols, image.rows), CV_16UC1);

        for (auto y = 0; y < image.rows; y++)
        {
            for (auto x = 0; x < image.cols; x++)
            {
                auto c = 0.0;
                c += image.data[y * image.step + x * image.elemSize() + 0];
                c += image.data[y * image.step + x * image.elemSize() + 1];
                c += image.data[y * image.step + x * image.elemSize() + 2];
                auto value = static_cast<uint16_t>(c / (255 * 3) * 65535);
                ((uint16_t *)mono.data)[y * mono.cols + x] = value;
            }
        }
        mono_ = mono;
    }
};

TEST(png, color2mono)
{
    PNG16 png("Tests/Images/cat1.jpg");
    // cv::imwrite("output.png", png.Get());
}

TEST(BitShifter, simple)
{
    qoi15::BitShifter<1> bitShifter;

    EXPECT_EQ(0x7FFF, bitShifter.Get(0xFFFF));
    EXPECT_EQ(0xFFFE, bitShifter.Set(0x7FFF));
}

TEST(RunLength, simple)
{
    qoi15::RunLength<2, 3, 0x00, 0x07> runLength;
    EXPECT_EQ(true, runLength.CheckHeader(0x00));
    EXPECT_EQ(false, runLength.CheckHeader(0x18));
    EXPECT_EQ(false, runLength.CheckHeader(0x10));
    EXPECT_EQ(false, runLength.CheckHeader(0x08));

    auto runValues = runLength.Get(10);
    auto ite = runValues.begin();

    auto value1 = *ite;
    ite++;
    auto value2 = *ite;
    EXPECT_EQ(0b010, value1);
    EXPECT_EQ(0b001, value2);

    auto run = runLength.Set(runValues);
    EXPECT_EQ(10, run);
}

TEST(Differential, simple)
{
    qoi15::Differential<1, 4, 0x10, 0x0F> differential;
    EXPECT_EQ(false, differential.CheckHeader(0x00));
    EXPECT_EQ(true, differential.CheckHeader(0x18));
    EXPECT_EQ(true, differential.CheckHeader(0x10));
    EXPECT_EQ(false, differential.CheckHeader(0x08));

    uint16_t previous = 0x0100;
    uint16_t current = 0x0100 - 3;

    auto diff = differential.Sub(previous, current);
    EXPECT_EQ(true, differential.IsValid(diff));
    EXPECT_EQ(-3, diff);
    auto value = differential.Get(diff);
    EXPECT_EQ(0x15, value);

    diff = differential.Set(value);
    EXPECT_EQ(-3, diff);
    auto original = differential.Add(previous, diff);
    EXPECT_EQ(original, current);
}

TEST(Table, simple)
{
    qoi15::Table<2, 3, 0x08, 0x07> table(1);
    EXPECT_EQ(false, table.CheckHeader(0x00));
    EXPECT_EQ(false, table.CheckHeader(0x18));
    EXPECT_EQ(false, table.CheckHeader(0x10));
    EXPECT_EQ(true, table.CheckHeader(0x08));

    auto original = 0x010A;
    auto hash = table.Hash(original);
    EXPECT_EQ(0x05, hash);

    auto value = table.Get(hash);
    EXPECT_EQ(0x0D, value);

    EXPECT_EQ(0x05, table.Set(value));

    EXPECT_EQ(false, table.Refer(hash) == original);
    table.Insert(hash, original);
    EXPECT_EQ(true, table.Refer(hash) == original);
}

TEST(Raw15bit, simple)
{
    qoi15::Raw15bit raw15bit;

    auto target = 0xAAAA;
    EXPECT_EQ(true, raw15bit.IsValid(target));
    EXPECT_EQ(0x2AAA, raw15bit.Set(target));
}

TEST(Chunker, simple)
{
    qoi15::Chunker chunker;

    auto target = 0x5555;
    uint8_t first, second, third;
    chunker.Get(target, first, second, third);
    EXPECT_EQ(0x15, first);
    EXPECT_EQ(0x0A, second);
    EXPECT_EQ(0x15, third);
}

TEST(SpeedFirstRepository, simple)
{
    qoi15::SpeedFirstRepository repository(100);

    repository.Set((uint8_t)0x1F);
    repository.Set((uint8_t)0x1F);
    repository.Set((uint16_t)0x7FFF);
    repository.Flush();

    EXPECT_EQ(2, repository.GetSize());
    auto ite = repository.GetIterator();
    EXPECT_EQ(0x3FF, *ite);
    ite++;
    EXPECT_EQ(0x7FFF, *ite);
}

TEST(qoi15, simple)
{
    std::vector<uint16_t> values =
        {
            0x0000, 0x0010, 0x0020, 0x0030, 0x0040, 0x0050, 0x0060,
            0x0100, 0x0110, 0x0120, 0x0130, 0x0140, 0x0150, 0x0160,
            0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000,
            0x0000, 0x0002, 0x0004, 0x0006, 0x0008, 0x000A, 0x000C,
            0x000E, 0x0010, 0x0012, 0x0014, 0x0016, 0x0018, 0x001A,
            0x0018, 0x0016, 0x0014, 0x0012, 0x0010, 0x000E, 0x000C};

    qoi15::QOI15Encoder encoder(&values[0], values.size());
    auto [ite1, size1] = encoder.Get();
    std::vector<uint16_t> encoded(size1);
    for (auto i = 0; i < size1; i++)
    {
        encoded[i] = *ite1;
        ite1++;
    }

    qoi15::QOI15Decoder decoder(&encoded[0], encoded.size(), values.size());
    auto [ite2, size2] = decoder.Get();
    EXPECT_EQ(values.size(), size2);

    std::vector<uint16_t> decoded(size2);
    for (auto i = 0; i < size2; i++)
    {
        decoded[i] = *ite2;
        EXPECT_EQ(decoded[i], values[i]);
        ite2++;
    }
}

TEST(qoi15, runlength)
{
    std::vector<uint16_t> values(513, 0xFFFE);

    qoi15::QOI15Encoder encoder(&values[0], values.size());
    auto [ite1, size1] = encoder.Get();
    std::vector<uint16_t> encoded(size1);
    for (auto i = 0; i < size1; i++)
    {
        encoded[i] = *ite1;
        ite1++;
    }

    qoi15::QOI15Decoder decoder(&encoded[0], encoded.size(), values.size());
    auto [ite2, size2] = decoder.Get();
    EXPECT_EQ(values.size(), size2);

    std::vector<uint16_t> decoded(size2);
    for (auto i = 0; i < size2; i++)
    {
        decoded[i] = *ite2;
        EXPECT_EQ(decoded[i], values[i]);
        ite2++;
    }
}

TEST(qoi15, image)
{
    PNG16 png("Tests/Images/cat1.jpg");
    uint16_t *buffer = (uint16_t *)(png.Get().data);
    auto width = png.Get().cols;
    auto height = png.Get().rows;
    auto size = width * height;

    for (auto i = 0; i < size; i++)
    {
        buffer[i] = buffer[i] & 0xFFFE;
    }

    qoi15::QOI15Encoder encoder(buffer, size);
    auto [ite1, size1] = encoder.Get();
    std::vector<uint16_t> encoded(size1);
    for (auto i = 0; i < size1; i++)
    {
        encoded[i] = *ite1;
        ite1++;
    }

    qoi15::QOI15Decoder decoder(&encoded[0], encoded.size(), size);
    auto [ite2, size2] = decoder.Get();
    EXPECT_EQ(size, size2);

    std::vector<uint16_t> decoded(size2);
    for (auto i = 0; i < size2; i++)
    {
        decoded[i] = *ite2;
        // EXPECT_EQ(decoded[i], buffer[i]);
        if (decoded[i] != buffer[i])
        {
            std::cout << i - 1 << ": " << decoded[i - 1] << " vs " << buffer[i - 1] << std::endl;
            std::cout << i << ": " << decoded[i] << " vs " << buffer[i] << std::endl;
            std::cout << i + 1 << ": " << decoded[i + 1] << " vs " << buffer[i + 1] << std::endl;

            break;
        }
        ite2++;
    }
}

TEST(qoi15, size)
{
    std::vector<std::string> paths{
        "Tests/Images/cat1.jpg",
        "Tests/Images/cat2.jpg",
        "Tests/Images/cat3.jpg",
        "Tests/Images/cat4.jpg",
        "Tests/Images/cat5.jpg",
        "Tests/Images/cat6.jpg",
        "Tests/Images/cat7.jpg"};

    for (const auto &path : paths)
    {
        PNG16 png(path);
        auto pngMat = png.Get();
        qoi15::QOI15Encoder<6> encoder((uint16_t *)(pngMat.data), pngMat.cols * pngMat.rows);
        auto [_, size] = encoder.Get();
        //std::cout << size << "/" << pngMat.cols * pngMat.rows << " = " << (float)size / (pngMat.cols * pngMat.rows) << std::endl;
        //encoder.ShowStatics();
        EXPECT_LT((float)size / (pngMat.cols * pngMat.rows), 1);
    }
}
