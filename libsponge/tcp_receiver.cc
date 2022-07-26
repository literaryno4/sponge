#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader head = seg.header();
    if (!head.syn && !_synReceived) {
        return;
    }

    string data = seg.payload().copy();

    bool eof = false;

    if (head.syn && !_synReceived) {
        _synReceived = true;
        _isn = head.seqno;
        if (head.fin) {
            _finReceived = eof = true;
        }
        _reassembler.push_substring(data, 0, eof);
        return;
    }

    if (_synReceived && head.fin) {
        _finReceived = eof = true;
    }

    uint64_t checkpoint = _reassembler.ack_index();
    uint64_t abs_seqno = unwrap(head.seqno, _isn, checkpoint);
    uint64_t stream_idx = abs_seqno - static_cast<uint64_t>(_synReceived);
    _reassembler.push_substring(data, stream_idx, eof);
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if (!_synReceived) {
        return nullopt;
    }
    return wrap(_reassembler.ack_index() + 1 + (_reassembler.empty() && _finReceived), _isn);
}

size_t TCPReceiver::window_size() const { 
    return _capacity - stream_out().buffer_size();
}
