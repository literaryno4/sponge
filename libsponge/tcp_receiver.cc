#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    // for syn
    if (seg.header().syn) {
        _isn = seg.header().seqno;
        _reassembler.push_substring(
            seg.payload().copy(), unwrap(seg.header().seqno, _isn, stream_out().bytes_written()), seg.header().fin);
        _ackno = _isn + stream_out().bytes_written() + 1;
        syn = true;
        if (seg.header().fin) {
            _ackno = _ackno + 1;
        }
        return;
    }

    // for payload
    if (syn) {
        _reassembler.push_substring(
            seg.payload().copy(), unwrap(seg.header().seqno, _isn, stream_out().bytes_written()) - 1, seg.header().fin);
        _ackno = _isn + stream_out().bytes_written() + 1;
        if (stream_out().input_ended() && _reassembler.empty()) {
            _ackno = _ackno + 1;
        }
    }
}


optional<WrappingInt32> TCPReceiver::ackno() const {
    if (_ackno != WrappingInt32(0)) {
        return _ackno;
    }
    return {};
}

size_t TCPReceiver::window_size() const { 
    return _capacity - stream_out().buffer_size();
}

