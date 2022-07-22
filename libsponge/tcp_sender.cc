#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <cassert>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

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

    if (_stream.eof() && _window_size > 0 && !_fin_sent) {
        TCPSegment seg;
        seg.header().fin = true;
        send_syn_or_fin(seg);
        _fin_sent = true;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 
    if (window_size == 0) {
        detect_when_window_zero();
    }
    // del relative _outstanding_segs
    uint64_t ackno_abs = unwrap(ackno, _isn, _stream.bytes_read());
    if (ackno_abs > _next_seqno) {
        return;
    }

    _window_size = window_size > (_next_seqno - ackno_abs) ? window_size - (_next_seqno - ackno_abs) : 0;

    auto itEnd = _outstanding_segs.lower_bound(ackno_abs);
    bool ackedNew = false;
    for (auto it = _outstanding_segs.begin(); it != itEnd;) {
        _bytes_in_flight -= it->second.first.length_in_sequence_space();
        it = _outstanding_segs.erase(it);
        ackedNew = true;
    }

    // reset timer
    if (ackedNew) {
        _rto = _initial_retransmission_timeout;
        restart_timer();
        _consecutive_retransmissions = 0;
    }

    // send if possible
    send_data(window_size);
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    bool restartTimer = false;
    for (auto& outstanding_seg : _outstanding_segs) {
        outstanding_seg.second.second += ms_since_last_tick;
        if (outstanding_seg.second.second >= _rto) {
            _segments_out.push(outstanding_seg.second.first);
            ++_consecutive_retransmissions;
            _rto <<= 1;
            outstanding_seg.second.second = 0;
            restartTimer = true;
        }
    }
    if (restartTimer) {
        restart_timer();
    }
    // send if possible
    send_data(_window_size);
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    // send ack
    TCPSegment seg;
    _segments_out.push(seg);
}

void TCPSender::send_data(size_t size) {
    size = min(size, _stream.buffer_size());
    size = min(size, TCPConfig::MAX_PAYLOAD_SIZE);
    size = min(size, static_cast<size_t>(_window_size));
    if (size == 0) {
        return;
    }

    TCPSegment seg;
    Buffer buffer(_stream.read(size));
    seg.payload() = buffer;
    seg.header().seqno = wrap(_next_seqno, _isn);
    if (seg.length_in_sequence_space() < _window_size) {
        seg.header().fin = _stream.eof();
        _fin_sent = _stream.eof();
    }
    _segments_out.push(seg);
    _next_seqno += seg.length_in_sequence_space();
    _outstanding_segs[_next_seqno - 1] = {seg, 0};
    _bytes_in_flight += seg.length_in_sequence_space();
    _window_size -= seg.length_in_sequence_space();
}

void TCPSender::send_syn_or_fin(TCPSegment& seg) {
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(seg);
    ++_next_seqno;
    _outstanding_segs[_next_seqno - 1] = {seg, 0};
    _bytes_in_flight += seg.length_in_sequence_space();
    --_window_size;
}

void TCPSender::restart_timer() {
    for (auto& outstanding_seg: _outstanding_segs) {
        outstanding_seg.second.second = 0;
    }
}

void TCPSender::detect_when_window_zero() {
    TCPSegment seg;
    Buffer buffer(_stream.buffer_empty() ? string() : _stream.read(1));
    seg.payload() = buffer;
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(seg);
    _next_seqno += seg.length_in_sequence_space();
    _outstanding_segs[_next_seqno - 1] = {seg, 0};
    _bytes_in_flight += seg.length_in_sequence_space();
    _window_size -= seg.length_in_sequence_space();
}
