#include <stdexcept>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity )
 : capacity_( capacity ),
   buffer_{}, front_offset_(0),
   error_(false), closed_(false),
   bytes_pushed_(0), bytes_popped_(0)
{}

void Writer::push( string data )
{
  uint64_t available_capacity = Writer::available_capacity();

  if (data.size() > available_capacity) {
    data.resize(available_capacity);
  }

  if (data.size() == 0) {
    return;
  }

  bytes_pushed_ += data.size();
  buffer_.push(data);
}

void Writer::close()
{
  closed_ = true;
}

void Writer::set_error()
{
  error_ = true;
}

bool Writer::is_closed() const
{
  return closed_;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - (bytes_pushed_ - bytes_popped_);
}

uint64_t Writer::bytes_pushed() const
{
  return bytes_pushed_;
}

string_view Reader::peek() const
{
  if (buffer_.empty()) {
    return {};
  }

  string_view view = buffer_.front();
  return view.substr(front_offset_);
}

bool Reader::is_finished() const
{
  return closed_ and bytes_buffered() == 0;
}

bool Reader::has_error() const
{
  return error_;
}

void Reader::pop( uint64_t len )
{
  len = min(len, bytes_buffered());

  if (len == 0) {
    return;
  }

  bytes_popped_ += len;

  while (len > 0) {
    string_view block = buffer_.front();

    uint64_t k = min(len, block.size() - front_offset_);

    front_offset_ += k;
    if (front_offset_ == block.size()) {
      buffer_.pop();
      front_offset_ = 0;
    }

    len -= k;
  }
}

uint64_t Reader::bytes_buffered() const
{
  return bytes_pushed_ - bytes_popped_;
}

uint64_t Reader::bytes_popped() const
{
  return bytes_popped_;
}
