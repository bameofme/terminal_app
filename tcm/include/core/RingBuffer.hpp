#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace tcm {

class RingBuffer {
public:
    static constexpr size_t DEFAULT_CAPACITY = 65536; // 64KB, power of 2

    explicit RingBuffer(size_t capacity = DEFAULT_CAPACITY);

    // Write data into buffer. If full, overwrites oldest data.
    // Returns number of bytes written (always == len when overwrite enabled)
    size_t write(const uint8_t* data, size_t len);

    // Read up to len bytes. Returns actual bytes read.
    size_t read(uint8_t* buf, size_t len);

    // Peek without consuming
    size_t peek(uint8_t* buf, size_t len) const;

    // Available bytes to read
    size_t available() const;

    // Total capacity
    size_t capacity() const;

    // True if empty
    bool empty() const;

    // Reset to empty
    void clear();

private:
    std::vector<uint8_t> m_buf;
    size_t m_capacity;  // must be power of 2
    size_t m_head = 0;  // read position
    size_t m_tail = 0;  // write position
    size_t m_size = 0;  // current fill
};

} // namespace tcm
