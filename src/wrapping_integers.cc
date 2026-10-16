#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  /* Convert absolute seqno → seqno. */
  return { zero_point + n };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  /* Convert seqno → absolute seqno. */

  /*
   * 1. 计算 raw_value_ 相对于零点 zero_point 的偏移量 offset
   * 2. offset 的高 32 位一定为零，可以进行加减 (1ULL << 32) 变换
   * 3. 可以将数轴 [0, inf) 分割为长度为 2^{32} 大小的任意多个窗口
   * 4. 距离 checkpoint 最近的点，只会出现在 checkpoint 所在的窗口、或其左边窗口、或其右边窗口
   * 5. 将 offset 的高 32 位变得和 checkpoint 的高 32 位一样，就可以将 offset 移到 checkpoint 所在的窗口
   * 6. 比较 offset 和 checkpoint 的大小
   * 7. 如果 offset <= checkpoint, 考虑 offset 位于右边的窗口时，是否更接近
   * 8. 如果 offset > checkpoint, 则考虑 offset 位于左边的窗口时，是否更近
   *   （注意 checkpoint <= 2^{32} 的情况, 即 checkpoint 位于第一个窗口)
   */
  uint64_t offset = static_cast<uint64_t>(raw_value_ - zero_point.raw_value_);

  uint64_t mid = offset | (checkpoint >> 32 << 32);
  constexpr uint64_t step_size = 1ULL << 32;

  if (mid >= checkpoint) {
    return ((mid < step_size) or (mid - checkpoint <= step_size/2)) ? mid : (mid - step_size);
  } else {
    return (checkpoint - mid <= step_size/2) ? mid : (mid + step_size);
  }
}
