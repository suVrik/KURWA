#include "core/io/binary_writer.h"
#include "core/debug/assert.h"

namespace kw {

BinaryWriter::BinaryWriter(const char* path)
    : m_stream(path, std::ios::binary)
{
}

bool BinaryWriter::write(const void* data, size_t size) {
    KW_ASSERT(data != nullptr);
    return static_cast<bool>(m_stream.write(reinterpret_cast<const char*>(data), size));
}

} // namespace kw
