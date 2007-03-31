#ifdef NDEBUG
#error "This is a test program and requires assertions to be enabled."
#endif

#include "ring_buffer.hpp"

int main()
{
    ring_buffer<int, 2> buf;
    assert(buf.size() == 0);
    assert(buf.empty());
    buf.push(1);
    assert(buf.front() == 1);
    assert(buf.back() == 1);
    assert(buf.size() == 1);
    assert(!buf.empty() && !buf.full());
    buf.push(2);
    assert(buf.front() == 1);
    assert(buf.back() == 2);
    assert(buf.size() == 2);
    assert(!buf.empty() && buf.full());
    buf.pop();
    assert(buf.front() == 2);
    assert(buf.back() == 2);
    assert(buf.size() == 1);
    assert(!buf.empty() && !buf.full());
    buf.push(3);
    assert(buf.front() == 2);
    assert(buf.back() == 3);
    assert(buf.size() == 2);
    assert(!buf.empty() && buf.full());
    buf.pop();
    assert(buf.size() == 1);
    buf.pop();
    assert(buf.size() == 0);
    assert(buf.empty());
}
