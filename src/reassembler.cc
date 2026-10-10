#include "reassembler.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  // window: [first_unassembled, window_end)
  uint64_t window_end = first_unassembled_ + output.available_capacity();

  uint64_t final_index = first_index + data.size();
  if (is_last_substring) {
    end_ = final_index;
  }


  if (first_index >= window_end or final_index < first_unassembled_) {
    return;
  }
  uint64_t offset = 0;
  if (first_index < first_unassembled_) {
    offset = first_unassembled_ - first_index;
    first_index = first_unassembled_;
  }
  if (final_index > window_end) {
    final_index = window_end;
  }

  data = data.substr(offset, final_index - first_index);


  offset = first_index - first_unassembled_;
  uint64_t least_size = offset + data.size();
  if (buffer_.size() < least_size) {
    filled_.resize(least_size);
    buffer_.resize(least_size);
  }

  for (uint i = 0; i < data.size(); i++) {
    if (!filled_[i+offset]) {
      buffer_[i+offset] = data[i];
      filled_[i+offset] = true;
    }
  }

  // flush
  uint64_t bytes_flush = 0;
  while (bytes_flush < filled_.size() and filled_[bytes_flush]) {
    bytes_flush++;
  }

  if (bytes_flush > 0) {
    output.push(buffer_.substr(0, bytes_flush));
    buffer_.erase(0, bytes_flush);
    filled_.erase(filled_.begin(), filled_.begin() + bytes_flush);
    first_unassembled_ += bytes_flush;
  }

  if (end_.has_value() and first_unassembled_ >= end_.value()) {
    output.close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  uint64_t pending = 0;

  for (bool filled : filled_) {
    if (filled) {
      pending++;
    }
  }

  return pending;
}
