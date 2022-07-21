#include "wrapping_integers.hh"
#include <stdlib.h>

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

uint64_t base = 1ll << 32;

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    return WrappingInt32{static_cast<uint32_t>((n % base + isn.raw_value()) % base)};
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    uint32_t l = checkpoint % base;
    uint64_t bot = checkpoint - l;
    uint64_t nraw = n.raw_value();
    uint64_t isnraw = isn.raw_value();
    nraw = nraw >= isnraw ? nraw : nraw + base;
    uint64_t seqno = ((nraw - isnraw) % base);

    seqno += bot;
    if (seqno > checkpoint) {
        if (seqno < base) {
            return seqno;
        }
        return (seqno - checkpoint) < (checkpoint + base - seqno) ? seqno : seqno - base;
    } else {
        return checkpoint - seqno < seqno + base - checkpoint ? seqno : seqno + base;
    }
}
