#include "packet.hpp"

#include <cstring>

namespace barony::net
{
namespace
{
void appendBigEndian(std::vector<std::uint8_t>& out, std::uint32_t value, std::size_t bytes)
{
    for (std::size_t i = bytes; i > 0; --i)
    {
        out.push_back(static_cast<std::uint8_t>((value >> ((i - 1) * 8)) & 0xffu));
    }
}
}

PacketWriter::PacketWriter(std::size_t reserve)
{
    data_.reserve(reserve);
}

void PacketWriter::clear()
{
    data_.clear();
}

void PacketWriter::writeU8(std::uint8_t value)
{
    data_.push_back(value);
}

void PacketWriter::writeU16(std::uint16_t value)
{
    appendBigEndian(data_, value, 2);
}

void PacketWriter::writeU32(std::uint32_t value)
{
    appendBigEndian(data_, value, 4);
}

void PacketWriter::writeI32(std::int32_t value)
{
    writeU32(static_cast<std::uint32_t>(value));
}

void PacketWriter::writeBytes(const void* data, std::size_t size)
{
    if (!data || size == 0)
    {
        return;
    }
    const auto* first = static_cast<const std::uint8_t*>(data);
    data_.insert(data_.end(), first, first + size);
}

void PacketWriter::writeString(std::string_view value)
{
    writeBytes(value.data(), value.size());
}

PacketView PacketWriter::view() const noexcept
{
    return { data_.data(), data_.size() };
}

const std::vector<std::uint8_t>& PacketWriter::bytes() const noexcept
{
    return data_;
}

PacketReader::PacketReader(const void* data, std::size_t size) noexcept
    : data_(static_cast<const std::uint8_t*>(data)), size_(size), valid_(data != nullptr || size == 0)
{
}

bool PacketReader::readU8(std::uint8_t& value) noexcept
{
    if (remaining() < 1)
    {
        valid_ = false;
        return false;
    }
    value = data_[offset_++];
    return true;
}

bool PacketReader::readU16(std::uint16_t& value) noexcept
{
    if (remaining() < 2)
    {
        valid_ = false;
        return false;
    }
    value = static_cast<std::uint16_t>((static_cast<std::uint16_t>(data_[offset_]) << 8)
        | static_cast<std::uint16_t>(data_[offset_ + 1]));
    offset_ += 2;
    return true;
}

bool PacketReader::readU32(std::uint32_t& value) noexcept
{
    if (remaining() < 4)
    {
        valid_ = false;
        return false;
    }
    value = (static_cast<std::uint32_t>(data_[offset_]) << 24)
        | (static_cast<std::uint32_t>(data_[offset_ + 1]) << 16)
        | (static_cast<std::uint32_t>(data_[offset_ + 2]) << 8)
        | static_cast<std::uint32_t>(data_[offset_ + 3]);
    offset_ += 4;
    return true;
}

bool PacketReader::readI32(std::int32_t& value) noexcept
{
    std::uint32_t raw = 0;
    if (!readU32(raw))
    {
        return false;
    }
    value = static_cast<std::int32_t>(raw);
    return true;
}

bool PacketReader::readBytes(void* dst, std::size_t size) noexcept
{
    if (!dst || remaining() < size)
    {
        valid_ = false;
        return false;
    }
    std::memcpy(dst, data_ + offset_, size);
    offset_ += size;
    return true;
}

bool PacketReader::skip(std::size_t size) noexcept
{
    if (remaining() < size)
    {
        valid_ = false;
        return false;
    }
    offset_ += size;
    return true;
}

std::size_t PacketReader::remaining() const noexcept
{
    return offset_ <= size_ ? size_ - offset_ : 0;
}

bool PacketReader::valid() const noexcept
{
    return valid_;
}
}
