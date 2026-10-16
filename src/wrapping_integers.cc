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
  uint64_t offset = static_cast<uint64_t>(raw_value_ - zero_point.raw_value_);

  uint64_t mid = offset | (checkpoint >> 32 << 32);
  uint64_t step_size = 1ull << 32;

  if (mid >= checkpoint) {
    return ((mid < step_size) or (mid - checkpoint <= step_size/2)) ? mid : (mid - step_size);
  } else {
    return (checkpoint - mid <= step_size/2) ? mid : (mid + step_size);
  }
}
