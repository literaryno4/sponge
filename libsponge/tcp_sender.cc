#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _rto(_initial_retransmission_timeout)
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    if (_next_seqno == 0) {
        TCPSegment seg;
        seg.header().syn = true;
        send_syn_or_fin(seg);
    }

    send_data(_stream.buffer_size());

    if (_stream.input_ended() && _stream.buffer_empty()) {
        TCPSegment seg;
        seg.header().fin = true;
        send_syn_or_fin(seg);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 
    // del relative _outstanding_segs
    auto itEnd = _outstanding_segs.lower_bound(unwrap(ackno, _isn, _stream.bytes_read()));
    for (auto it = _outstanding_segs.begin(); it != itEnd;) {
        _bytes_in_flight -= it->second.first.length_in_sequence_space();
        it = _outstanding_segs.erase(it);
    }

    // send if possible
    send_data(window_size);
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    for (auto& outstanding_seg : _outstanding_segs) {
        outstanding_seg.second.second += ms_since_last_tick;
        if (outstanding_seg.second.second >= _rto) {
            _segments_out.push(outstanding_seg.second.first);
            outstanding_seg.second.second = 0;
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return {}; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    _segments_out.push(seg);
}

void TCPSender::send_data(size_t size) {
    size = size < _stream.buffer_size() ? size : _stream.buffer_size();
    size = size < TCPConfig::MAX_PAYLOAD_SIZE ? size : TCPConfig::MAX_PAYLOAD_SIZE;

    TCPSegment seg;
    Buffer buffer(_stream.read(size));
    seg.parse(buffer);
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(seg);
    _next_seqno += size;
    _outstanding_segs[_next_seqno - 1] = {seg, 0};
    _bytes_in_flight += seg.length_in_sequence_space();
}

void TCPSender::send_syn_or_fin(TCPSegment& seg) {
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(seg);
    ++_next_seqno;
    _bytes_in_flight += seg.length_in_sequence_space();
}
