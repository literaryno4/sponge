#include "stream_reassembler.hh"
#include <iostream>
#include <cassert>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity), _buffer() {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (eof) {
        _eof = true;
    }
    if (data.size() == 0) {
        check_eof();
        return;
    }

    // calcu data that can be add to
    size_t beginIdx = index, endIdx = index + data.size();
    if (beginIdx > endIdx) {
        return;
    }
    string dataToAdd = data;
    size_t leftDisOrder = _capacity -_output.buffer_size();
    if (leftDisOrder + _begin < endIdx) {
        _end = leftDisOrder + _begin;
        if (beginIdx >= _end) {
            return;
        } else {
            dataToAdd = dataToAdd.substr(0, _end - beginIdx);
        }
        _eof = false;
    } else {
        _end = max(_end, endIdx);
    }
    if (beginIdx < _begin) {
        if (endIdx <= _begin) {
            return;
        }
        dataToAdd = dataToAdd.substr(_begin - beginIdx);
        beginIdx = _begin;
    }
    endIdx = beginIdx + dataToAdd.size();
    
    // add to _buffer
    string prefix, suffix;
    auto itBegin = _buffer.upper_bound(Excerpt(beginIdx, beginIdx, string()));
    auto itEnd = _buffer.upper_bound(Excerpt(endIdx, endIdx, string()));

    if (itBegin != _buffer.begin()) {
        --itBegin;
        if ((*itBegin).endIdx >= beginIdx) {
            prefix = (*itBegin).data.substr(0, beginIdx - (*itBegin).beginIdx);
        } else {
            ++itBegin;
        }
    }
    if (itEnd != _buffer.begin()) {
        --itEnd;
        if ((*itEnd).endIdx > endIdx) {
            suffix = (*itEnd).data.substr(endIdx - (*itEnd).beginIdx);
        }
        ++itEnd;
    }
    size_t numDel = 0;
    for (auto it = itBegin; it != itEnd;) {
        numDel += (*it).data.size();
        it =_buffer.erase(it);
    }
    dataToAdd = prefix + dataToAdd + suffix;
    _unassembled_bytes += (dataToAdd.size() - numDel);
    _buffer.insert(Excerpt(beginIdx - prefix.size(), endIdx + suffix.size(), dataToAdd));

    // move to byte stream
    auto first = _buffer.begin();
    if ((*first).beginIdx == _begin) {
        _output.write(first->data);
        _begin += first->data.size();
        _unassembled_bytes -= first->data.size();
        _buffer.erase(first);
    }

    check_eof();
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _buffer.empty(); }

void StreamReassembler::check_eof() {
    if (_begin == _end && _buffer.empty() && _eof) {
        _output.end_input();
    }
}

