// MIT License

// Copyright (c) 2022 Naoki Ikeda

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <iostream>
#include <vector>
#include <list>
#include <tuple>

namespace qoi15
{
    template <int shift>
    class BitShifter
    {
    public:
        BitShifter()
        {
            static_assert(shift > 0, "must be larger than 0");
        }

        uint16_t Get(const uint16_t value)
        {
            return value >> shift;
        }
        uint16_t Set(const uint16_t value)
        {
            return value << shift;
        }
    };

    template <int headerBit, int valueBit, uint8_t header, uint8_t mask>
    class RunLength
    {
    public:
        RunLength()
        {
            //2bit: header 0b00
            //3bit: value 0bXXX
        }

        bool CheckHeader(const uint8_t value)
        {
            return (value & (~mask)) == header;
        }

        std::list<uint8_t> Get(int length)
        {
            std::list<uint8_t> values;
            while (length != 0)
            {
                auto value = static_cast<uint8_t>(length & mask) | header;
                values.emplace_back(value);
                length = length >> valueBit;
            }
            return values;
        }

        int Set(const std::list<uint8_t> &values)
        {
            int length = 0;
            auto shift = 0;
            for (const auto &value : values)
            {
                length |= (value & mask) << shift;
                shift += valueBit;
            }

            return length;
        }
    };

    template <int headerBit, int valueBit, uint8_t header, uint8_t mask>
    class Differential
    {
        const int32_t MaxValue;

    public:
        Differential()
            : MaxValue(1 << (valueBit - 1))
        {
        }

        bool CheckHeader(const uint8_t value)
        {
            return (value & (~mask)) == header;
        }

        int32_t Sub(const uint16_t previous, const uint16_t current)
        {
            return static_cast<int>(current) - static_cast<int>(previous);
        }

        bool IsValid(const int32_t diff)
        {
            return std::abs(diff) <= MaxValue && diff != 0;//TODO: delete != 0 judgement
        }

        uint8_t Get(const int32_t diff)
        {
            uint8_t value = diff < 0 ? (diff + MaxValue) : (diff + (MaxValue - 1));
            value = value | header;
            return value;
        }

        int32_t Set(const uint8_t value)
        {
            int32_t diff = value & mask;
            diff = (diff < MaxValue) ? (diff - MaxValue) : (diff - (MaxValue - 1));
            return diff;
        }

        uint16_t Add(const uint16_t previous, const int32_t diff)
        {
            return previous + diff;
        }
    };

    template <int headerBit, int valueBit, uint8_t header, uint8_t mask>
    class Table
    {
        const int32_t TableSize;
        std::vector<uint16_t> ref_;
        const int32_t hashBit_;

    public:
        Table(const int hashBit = 1)
            : TableSize(1 << valueBit), ref_(TableSize, 0xFFFF), hashBit_(hashBit)
        {
        }

        bool CheckHeader(const uint8_t value)
        {
            return (value & (~mask)) == header;
        }

        uint8_t Hash(const uint16_t value)
        {
            return static_cast<uint8_t>((value >> hashBit_) & mask);
        }

        uint8_t Set(const uint8_t value)
        {
            auto hash = value & mask;
            return hash;
        }

        uint16_t Refer(const uint8_t hash)
        {
            return ref_[hash];
        }

        void Insert(const uint8_t hash, const uint16_t value)
        {
            ref_[hash] = value;
        }

        uint8_t Get(const uint8_t hash)
        {
            return header | hash;
        }
    };

    class Raw15bit
    {
    public:
        uint16_t Get(const uint16_t value)
        {
            return 0x8000 | value;
        }

        uint16_t Set(const uint16_t value)
        {
            return 0x7FFF & value;
        }

        bool IsValid(const uint16_t value)
        {
            return (value & 0x8000) != 0;
        }
    };

    class Chunker
    {
    public:
        Chunker() = default;
        Chunker(const Chunker &) = delete;
        virtual ~Chunker() = default;

        void Get(const uint16_t value, uint8_t &first, uint8_t &second, uint8_t &third)
        {
            first = static_cast<uint8_t>(value & 0x1F);
            second = static_cast<uint8_t>((value >> 5) & 0x1F);
            third = static_cast<uint8_t>((value >> 10) & 0x1F);
        }

        uint16_t Set(const uint8_t &first, const uint8_t &second, const uint8_t &third)
        {
            return (static_cast<uint16_t>(first)) |
                   (static_cast<uint16_t>(second) << 5) |
                   (static_cast<uint16_t>(third) << 10);
        }
    };

    class Repository
    {
    protected:
        Chunker chunker_;

    public:
        virtual void Set(const uint16_t value) = 0;
        virtual void Set(const uint8_t value) = 0;
        virtual void Flush() = 0;
    };

    class SpeedFirstRepository : public Repository
    {
        std::vector<uint16_t> buffer_;
        int counter_;

        std::list<uint8_t> temp_;
        int tempCounter_;

    public:
        SpeedFirstRepository(const int maxSize)
            : buffer_(maxSize), counter_(0), temp_(), tempCounter_(0)
        {
        }

        virtual void Set(const uint16_t value)
        {
            if (tempCounter_ > 0)
            {
                Flush();
            }

            buffer_[counter_++] = value;
        }

        virtual void Set(const uint8_t value)
        {
            temp_.emplace_back(value);
            tempCounter_++;
        }

        virtual void Flush()
        {
            auto ite = temp_.begin();
            while (tempCounter_ >= 3)
            {
                auto first = *ite;
                ite++;
                auto second = *ite;
                ite++;
                auto third = *ite;
                ite++;
                auto value = chunker_.Set(first, second, third);
                buffer_[counter_++] = value;

                tempCounter_ -= 3;
            }
            if (tempCounter_ > 0)
            {
                uint8_t fst[3] = {0};
                for (auto i = 0; i < tempCounter_; ++i)
                {
                    fst[i] = *ite;
                    ite++;
                }
                tempCounter_ = 0;
                auto value = chunker_.Set(fst[0], fst[1], fst[2]);
                buffer_[counter_++] = value;
            }
            temp_.clear();
        }

        int GetSize()
        {
            return counter_;
        }

        std::vector<uint16_t>::const_iterator GetIterator()
        {
            return buffer_.begin();
        }
    };

    template<int internalShift=1>
    class QOI15Encoder
    {
        BitShifter<internalShift> bitShifter_;
        RunLength<2, 3, 0x00, 0x07> runLength_;
#ifndef TABLE_FIRST
        Differential<1, 4, 0x10, 0x0F> differential_;
        Table<2, 3, 0x08, 0x07> table_;
#else
        Differential<2, 3, 0x08, 0x07> differential_;
        Table<1, 4, 0x10, 0x0F> table_;
#endif
        Raw15bit raw_;

        SpeedFirstRepository repository_;

#ifdef ENABLE_STATICS
        int runLengthCount_;
        int diffCount_;
        int tableCount_;
        int rawCount_;
#endif

    public:
        QOI15Encoder(const uint16_t *buffer, const int size)
            : repository_(size)
#ifdef ENABLE_STATICS
            , runLengthCount_(0), diffCount_(0), tableCount_(0), rawCount_(0)
#endif
        {
            uint16_t previous = 0xFFFF;
            auto runLength = 0;

            for (auto i = 0; i < size; ++i)
            {
                auto current = bitShifter_.Get(buffer[i]);

                if (previous == current)
                {
                    runLength++;
                    continue;
                }
                if (runLength != 0)
                {
                    //flush
                    auto runValues = runLength_.Get(runLength);
                    for (auto &runValue : runValues)
                    {
                        repository_.Set(runValue);
                    }
#ifdef ENABLE_STATICS
                    runLengthCount_ += runLength;
#endif
                    runLength = 0;
                }

                auto diff = differential_.Sub(previous, current);
                if (differential_.IsValid(diff))
                {
                    auto value = differential_.Get(diff);
                    repository_.Set(value);
                    previous = current;
#ifdef ENABLE_STATICS
                    diffCount_++;
#endif
                    continue;
                }

                auto hash = table_.Hash(current);
                if (table_.Refer(hash) == current)
                {
                    auto value = table_.Get(hash);
                    repository_.Set(value);
                    previous = current;
#ifdef ENABLE_STATICS
                    tableCount_++;
#endif
                    continue;
                }
                table_.Insert(hash, current);

                repository_.Set(raw_.Get(current));
                previous = current;
#ifdef ENABLE_STATICS
                rawCount_++;
#endif
            }
            if (runLength != 0)
            {
                //flush
                auto runValues = runLength_.Get(runLength);
                for (auto &runValue : runValues)
                {
                    repository_.Set(runValue);
                }
#ifdef ENABLE_STATICS
                    runLengthCount_ += runLength;
#endif
                runLength = 0;
            }

            repository_.Flush();
        }

        std::tuple<std::vector<uint16_t>::const_iterator, int> Get()
        {
            return {repository_.GetIterator(), repository_.GetSize()};
        }

#ifdef ENABLE_STATICS
        void ShowStatics()
        {
            std::cout << "runLength: " << runLengthCount_ << std::endl;
            std::cout << "diff: " << diffCount_ << std::endl;
            std::cout << "table: " << tableCount_ << std::endl;
            std::cout << "raw: " << rawCount_ << std::endl;
        }
#endif        
    };

    class QOI15Decoder
    {
        BitShifter<1> bitShifter_;
        RunLength<2, 3, 0x00, 0x07> runLength_;
#ifndef TABLE_FIRST
        Differential<1, 4, 0x10, 0x0F> differential_;
        Table<2, 3, 0x08, 0x07> table_;
#else
        Differential<2, 3, 0x08, 0x07> differential_;
        Table<1, 4, 0x10, 0x0F> table_;
#endif
        Raw15bit raw_;

        SpeedFirstRepository repository_;

    public:
        Chunker chunker_;
        QOI15Decoder(const uint16_t *buffer, const int size, const int outputSize)
            : repository_(outputSize)
        {
            auto counter = 0;
            uint16_t previous = 0xFFFF;
            std::list<uint8_t> leftovers;

            std::list<uint8_t> runLengthValues;

            while (counter < size || leftovers.size() > 0)
            {
                if (leftovers.size() > 0)
                {
                    auto value = leftovers.front();
                    leftovers.pop_front();

                    if (runLength_.CheckHeader(value))
                    {
                        runLengthValues.emplace_back(value);
                    }
                    else
                    {
                        if (runLengthValues.size() > 0)
                        {
                            auto length = runLength_.Set(runLengthValues);
                            runLengthValues.clear();
                            for (auto i = 0; i < length; ++i)
                            {
                                repository_.Set(bitShifter_.Set(previous));
                            }
                        }
                    }

                    if (differential_.CheckHeader(value))
                    {
                        auto diff = differential_.Set(value);
                        auto current = differential_.Add(previous, diff);
                        repository_.Set(bitShifter_.Set(current));
                        previous = current;
                    }

                    if (table_.CheckHeader(value))
                    {
                        auto hash = table_.Set(value);
                        auto current = table_.Refer(hash);
                        repository_.Set(bitShifter_.Set(current));
                        previous = current;
                    }

                    continue;
                }

                auto value = buffer[counter++];

                if (raw_.IsValid(value))
                {
                    if (runLengthValues.size() > 0)
                    {
                        auto length = runLength_.Set(runLengthValues);
                        runLengthValues.clear();
                        for (auto i = 0; i < length; ++i)
                        {
                            repository_.Set(bitShifter_.Set(previous));
                        }
                    }

                    auto current = raw_.Set(value);
                    auto hash = table_.Hash(current);
                    table_.Insert(hash, current);
                    repository_.Set(bitShifter_.Set(current));
                    previous = current;
                    continue;
                }

                uint8_t first, second, third;
                chunker_.Get(value, first, second, third);
                leftovers.emplace_back(first);
                leftovers.emplace_back(second);
                leftovers.emplace_back(third);
            }

            if (runLengthValues.size() > 0)
            {
                auto length = runLength_.Set(runLengthValues);
                runLengthValues.clear();
                for (auto i = 0; i < length; ++i)
                {
                    repository_.Set(bitShifter_.Set(previous));
                }
            }
        }

        std::tuple<std::vector<uint16_t>::const_iterator, int> Get()
        {
            return {repository_.GetIterator(), repository_.GetSize()};
        }
    };
}
