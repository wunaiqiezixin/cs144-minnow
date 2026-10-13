#include "reassembler.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  // window: [first_unassembled_, window_end)
  uint64_t window_end = first_unassembled_ + output.available_capacity();

  // current stream: [first_index, final_index)
  uint64_t final_index = first_index + data.size();
  if (is_last_substring) {
    end_ = final_index;
  }

  if (first_index >= window_end || final_index < first_unassembled_) {
    if (end_.has_value() && first_unassembled_ >= end_.value()) {
      output.close();
    }
    return;
  }
  // data 中的下标 i <--> Stram 中的下标 first_index + i
  uint64_t offset = 0;
  if (first_index < first_unassembled_) {
    offset = first_unassembled_ - first_index;
  }
  final_index = min(final_index, window_end);

  // data 中有效的字节：[first_index + offset, final_index)
  data = data.substr(offset, final_index - first_index - offset);

  // 将区间 [first_index + offset, final_index) 插入 buffer_
  first_index += offset;


  // 向左合并一次
  auto it_left = buffer_.upper_bound(first_index);
  bool left_merged = false;
  if (it_left != buffer_.begin()) { // if buffer_ is empty: buffer_.begin() == buffer_.end()
    it_left--;
    auto& [left_index, left_data] = *it_left;
    if (left_index + left_data.size() >= first_index) {
      left_merged = true;
      offset = left_index + left_data.size() - first_index;

      if (offset >= data.size()) { // 完全覆盖
        // ...
        data = it_left->second;
      } else { // 部分覆盖
        // [first_index + offset, first_index + data.size())
        data = left_data + data.substr(offset);
      }

      first_index = left_index;
      final_index = max(final_index, left_index+left_data.size());
    }
  }

  // 循环向右合并
  for (;;) { // Interval: { [first_index, final_index) , data}
    auto it_right = buffer_.upper_bound(first_index);
    if (it_right == buffer_.end()) {
      break;
    }
    auto& [right_index, right_data] = *it_right;
    if (final_index < right_index) {
      break;
    }

    offset = final_index-right_index;
    if (offset >= right_data.size()) { // 完全覆盖
      // ...
    } else { // 部分覆盖
      data += right_data.substr(offset);
      final_index = right_index+right_data.size();
    }
    buffer_.erase(it_right);
  }

  // 先考虑能否直接冲刷 buffer_，不能再考虑插入
  if (first_index == first_unassembled_) {
    output.push(data);
    first_unassembled_ += data.size();
    if (left_merged) {
      buffer_.erase(it_left);
    }
  } else if (!left_merged) {
    buffer_.insert({first_index, data});
  } else {
    it_left->second = data;
  }

  if (end_.has_value() && first_unassembled_ >= end_.value()) {
    output.close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  uint64_t pending = 0;
  for (const auto&[index, data] : buffer_) {
    pending += data.size();
  }
  return pending;
}
