#include "RingBuffer.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace tcm {

// Round up to next power of 2 (returns v if already power of 2)
static size_t nextPow2(size_t v) {
    if (v == 0) return 1;
    // Check if already power of 2
    if ((v & (v - 1)) == 0) return v;
    size_t p = 1;
    while (p < v) p <<= 1;
    return p;
}

RingBuffer::RingBuffer(size_t capacity)
    : m_capacity(nextPow2(capacity == 0 ? 1 : capacity))
    , m_head(0)
    , m_tail(0)
    , m_size(0)
{
    m_buf.resize(m_capacity);
}

size_t RingBuffer::write(const uint8_t* data, size_t len) {
    if (len == 0) return 0;

    // If len > capacity, only keep the newest (last) bytes
    if (len > m_capacity) {
        data += (len - m_capacity);
        len = m_capacity;
    }

    const size_t mask = m_capacity - 1;

    for (size_t i = 0; i < len; ++i) {
        if (m_size == m_capacity) {
            // Buffer full: overwrite oldest by advancing head
            m_head = (m_head + 1) & mask;
        } else {
            ++m_size;
        }
        m_buf[m_tail & mask] = data[i];
        m_tail = (m_tail + 1) & mask;
    }

    return len;
}

size_t RingBuffer::read(uint8_t* buf, size_t len) {
    const size_t toRead = std::min(len, m_size);
    if (toRead == 0) return 0;

    const size_t mask = m_capacity - 1;
    for (size_t i = 0; i < toRead; ++i) {
        buf[i] = m_buf[m_head & mask];
        m_head = (m_head + 1) & mask;
    }
    m_size -= toRead;
    return toRead;
}

size_t RingBuffer::peek(uint8_t* buf, size_t len) const {
    const size_t toRead = std::min(len, m_size);
    if (toRead == 0) return 0;

    const size_t mask = m_capacity - 1;
    size_t pos = m_head;
    for (size_t i = 0; i < toRead; ++i) {
        buf[i] = m_buf[pos & mask];
        pos = (pos + 1) & mask;
    }
    return toRead;
}

size_t RingBuffer::available() const {
    return m_size;
}

size_t RingBuffer::capacity() const {
    return m_capacity;
}

bool RingBuffer::empty() const {
    return m_size == 0;
}

void RingBuffer::clear() {
    m_head = m_tail = m_size = 0;
}

} // namespace tcm
