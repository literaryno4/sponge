#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) { 
    _time_since_last_segment_received = 0;
    if (!_active) {
        return;
    }
    // abort if reset segment
    if (seg.header().rst) {
        abort_connection();
        return;
    }

    // give tcp segment to _receiver
    if (seg.length_in_sequence_space() > 0) {
        _receiver.segment_received(seg);
    }

    // give ackno and window size to sender to ack outstanding segments
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        //if (_receiver.stream_out().eof() && _sender.stream_in().eof() && _sender.bytes_in_flight() == 0 && _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2) {
        //    _active = false;
        //}
    }

    // if fin seg
    if (seg.header().fin) {
        _sender.stream_in().end_input();
    }

    // handle keep-alive
    if (_receiver.ackno().has_value() and (seg.length_in_sequence_space() == 0) and seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
        send_out();
    }



    // if received syn from client
    if (seg.header().syn && _sender.next_seqno_absolute() == 0) {
        _sender.fill_window();
        send_out();
    // send ack back immediately if seg not empty(), but not ack a ack
    } else if (!(seg.header().ack && seg.length_in_sequence_space() == 0)) {
        //if (!_sender.segments_out().empty()) {
         //   send_out();
        if (seg.header().win > 0) {
            if (_receiver.ackno().has_value()) {
                ack_reply(_receiver.ackno().value());
            } else {
                ack_reply(seg.header().seqno + seg.length_in_sequence_space());
            }
        }
    }
    
    // if received fin but not sent, no need lingering
    if (seg.header().fin && !_fin_sent) {
        _linger_after_streams_finish = false;
    }
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    if (data.size() == 0) {
        return 0;
    }
    size_t written = _sender.stream_in().write(data);
    _sender.fill_window();
    send_out();
    return written;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 
    _time_since_last_segment_received += ms_since_last_tick;

    _sender.tick(ms_since_last_tick); 

    if (_receiver.stream_out().input_ended() && !_fin_sent) {
        _linger_after_streams_finish = false;
    }
    if (_receiver.unassembled_bytes() == 0 && _receiver.stream_out().input_ended() && _sender.bytes_in_flight() == 0 && _fin_sent &&
        (!_linger_after_streams_finish || _time_since_last_segment_received >= 10 * _cfg.rt_timeout)) {
        //_sender.fill_window();
        _active = false;
    }

    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        send_rst_segment();
        return;
    }

    send_out();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    send_out();
    if (_sender.stream_in().eof() && _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2 && _sender.bytes_in_flight()  > 0) {
        _fin_sent = true;
    }
}

void TCPConnection::connect() {
    _sender.fill_window();
    send_out();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            // NOTICE: Should I send in _sender ???
            send_rst_segment();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::send_out() {
    while (!_sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
        } else {
            seg.header().win = std::numeric_limits<uint16_t>::max();
        }
        if (seg.length_in_sequence_space() == 0) {
            seg.header().seqno = _sender.next_seqno() - 1;
            seg.header().win = std::numeric_limits<uint16_t>::max();
        }
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
    if (_receiver.ackno().has_value()) {
        seg.header().ack = true;
        seg.header().ackno = ackno;
        seg.header().win = _receiver.window_size();
    } else {
        seg.header().win = std::numeric_limits<uint16_t>::max();
    }
    _segments_out.push(seg);
}
    
void TCPConnection::abort_connection() {
    _receiver.stream_out().set_error();
    _sender.stream_in().set_error();
    _active = false;
}
