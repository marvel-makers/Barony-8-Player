#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace barony::net
{
struct PacketView
{
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;

    explicit operator bool() const noexcept { return data != nullptr && size != 0; }
};

class PacketWriter
{
public:
    explicit PacketWriter(std::size_t reserve = 64);

    void clear();
    void writeU8(std::uint8_t value);
    void writeU16(std::uint16_t value);
    void writeU32(std::uint32_t value);
    void writeI32(std::int32_t value);
    void writeBytes(const void* data, std::size_t size);
    void writeString(std::string_view value);

    PacketView view() const noexcept;
    const std::vector<std::uint8_t>& bytes() const noexcept;

private:
    std::vector<std::uint8_t> data_;
};

class PacketReader
{
public:
    PacketReader(const void* data, std::size_t size) noexcept;

    bool readU8(std::uint8_t& value) noexcept;
    bool readU16(std::uint16_t& value) noexcept;
    bool readU32(std::uint32_t& value) noexcept;
    bool readI32(std::int32_t& value) noexcept;
    bool readBytes(void* dst, std::size_t size) noexcept;
    bool skip(std::size_t size) noexcept;

    std::size_t remaining() const noexcept;
    bool valid() const noexcept;

private:
    const std::uint8_t* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t offset_ = 0;
    bool valid_ = true;
};

constexpr std::uint32_t fourCC(char a, char b, char c, char d) noexcept
{
    return (static_cast<std::uint32_t>(static_cast<std::uint8_t>(a)) << 24)
        | (static_cast<std::uint32_t>(static_cast<std::uint8_t>(b)) << 16)
        | (static_cast<std::uint32_t>(static_cast<std::uint8_t>(c)) << 8)
        | static_cast<std::uint32_t>(static_cast<std::uint8_t>(d));
}
}
