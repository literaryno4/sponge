#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) { 
    if (seg.header().rst) {
        abort_connection();
        return;
    }
    _receiver.segment_received(seg);
    if (seg.header().fin && !_fin_sent) {
        _linger_after_streams_finish = false;
    }
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }

    // CHECK THIS : how to reply with ack ?
    ack_reply(seg.header().seqno + seg.length_in_sequence_space());
    send_tcp_segment();
    
    // handle keep-alive
    if (_receiver.ackno().has_value() and (seg.length_in_sequence_space() == 0) and seg.header().seqno == _receiver.ackno().value() -1) {
        _sender.send_empty_segment();
    }

    _time_since_last_segment_received = 0;
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    size_t written = _sender.stream_in().write(data);
    _sender.fill_window();
    send_tcp_segment();
    return written;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 

    _time_since_last_segment_received += ms_since_last_tick;
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        send_rst_segment();
        return;
    }
    if (_receiver.stream_out().input_ended() && !_fin_sent) {
        _linger_after_streams_finish = false;
    }
    if (_receiver.unassembled_bytes() == 0 && _receiver.stream_out().input_ended() && _sender.bytes_in_flight() == 0 && 
        (!_linger_after_streams_finish || _time_since_last_segment_received >= 10 * _cfg.rt_timeout)) {
        _sender.fill_window();
        _active = false;
    }
    _sender.tick(ms_since_last_tick); 
    send_tcp_segment();
}

void TCPConnection::end_input_stream() {
    _fin_sent = true;
    _sender.stream_in().end_input();
    _sender.fill_window();
    send_tcp_segment();
}

void TCPConnection::connect() {
    _sender.fill_window();
    send_tcp_segment();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            send_rst_segment();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::send_tcp_segment() {
    while (!_sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
        }
        seg.header().win = _receiver.window_size();
        _segments_out.push(seg);
    }
}

void TCPConnection::send_rst_segment() {
    TCPSegment seg;
    seg.header().rst = true;
    _segments_out.push(seg);
    abort_connection();
}

void TCPConnection::ack_reply(WrappingInt32 ackno) {
    TCPSegment seg;
    seg.header().ack = true;
    seg.header().ackno = ackno;
    seg.header().win = _receiver.window_size();
    _segments_out.push(seg);
}
    
void TCPConnection::abort_connection() {
    _receiver.stream_out().set_error();
    _sender.stream_in().set_error();
    _active = false;
}
