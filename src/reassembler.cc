#include "reassembler.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  // current window: [first_unassembled_, window_end)
  uint64_t window_end = first_unassembled_ + output.available_capacity();
  // current stream: [first_index, final_index)
  uint64_t final_index = first_index + data.size();

  if (is_last_substring) {
    end_ = final_index;
  }

  if (first_index >= window_end || final_index < first_unassembled_) {
    // 注意 data 为空或与前面处理的流完全重复的情况
    if (end_.has_value() && first_unassembled_ >= end_.value()) {
      output.close();
    }
    return;
  }

  // data 的有效部分：[new_begin, new_end)
  uint64_t new_begin = 0, new_end = data.size();
  if (first_index < first_unassembled_) {
    new_begin += first_unassembled_ - first_index;
  }
  if (final_index > window_end) {
    new_end -= first_index+data.size()-window_end;
  }
  data = data.substr(new_begin, new_end-new_begin);

  uint64_t merged_begin = first_index + new_begin;
  uint64_t merged_end = first_index + new_end;
  string& merged_data = data;

  // 循环向右合并
  for (;;) {
    // 终止条件：该区间为最后一个区间 or 该区间和后一个区间无重叠
    auto it = buffer_.lower_bound(merged_begin);
    if (it == buffer_.end()) {
      break;
    }
    auto& [right_index, right_data] = *it;
    if (merged_end < right_index) {
      break;
    }

    uint64_t overlap_size = merged_end - right_index;
    if (overlap_size >= right_data.size()) { // 完全覆盖
      // ok.
    } else { // 部分覆盖
      merged_end += right_data.size()-overlap_size;
      merged_data += right_data.substr(overlap_size);
    }
    // 移除右边区间
    buffer_.erase(it);
  }

  // 向左合并一次
  bool left_merged = false;
  auto it = buffer_.upper_bound(merged_begin);
  if (it != buffer_.begin()) {
    it--;
    auto& [left_index, left_data] = *it;
    if (left_index + left_data.size() >= merged_begin) {
      left_merged = true;
      uint64_t overlap_size = left_index + left_data.size() - merged_begin;

      if (overlap_size >= merged_data.size()) {
        merged_data = left_data;
      } else {
        left_data += merged_data.substr(overlap_size);
        merged_data = left_data;
      }

      merged_begin = left_index;
      merged_end = max(merged_end, left_index+left_data.size());
    }
  }

  // flush
  if (merged_begin == first_unassembled_) {
    output.push(merged_data);
    first_unassembled_ += merged_data.size();
    if (left_merged) {
      buffer_.erase(it);
    }
  } else if(!left_merged) {
    buffer_.emplace(merged_begin, std::move(merged_data));
  }

  // close
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
