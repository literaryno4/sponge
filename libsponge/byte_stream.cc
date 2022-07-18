#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : byteStream_(), capacity_(capacity), remaining_capacity_(capacity_) {}

size_t ByteStream::write(const string &data) {
    if (input_ended()) {
        set_error();
        return 0;
    }
    size_t len = data.size();
    size_t toWritten;
    if (len < remaining_capacity_) {
        toWritten = len;
    } else {
        toWritten = remaining_capacity_;
    }
    for (size_t i = 0; i < toWritten; ++i) {
        byteStream_.push_back(data[i]);
    }
    remaining_capacity_ -= toWritten;
    total_written_ += toWritten;
    return toWritten;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t toPeek = len < byteStream_.size() ? len : byteStream_.size();
    return string(byteStream_.begin(), byteStream_.begin() + toPeek);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t toPop = len < byteStream_.size() ? len : byteStream_.size();
    for (size_t i = 0; i < toPop; ++i) {
        byteStream_.pop_front();
    }
    remaining_capacity_ += toPop;
    total_read_ += toPop;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string buffer = peek_output(len);
    pop_output(len);
    return buffer;
}

void ByteStream::end_input() { input_ended_ = true; }

bool ByteStream::input_ended() const { return input_ended_; }

size_t ByteStream::buffer_size() const { return byteStream_.size(); }

bool ByteStream::buffer_empty() const { return byteStream_.empty(); }

bool ByteStream::eof() const { return remaining_capacity_ == capacity_ && input_ended(); }

size_t ByteStream::bytes_written() const { return total_written_; }

size_t ByteStream::bytes_read() const { return total_read_; }

size_t ByteStream::remaining_capacity() const { return remaining_capacity_; }
