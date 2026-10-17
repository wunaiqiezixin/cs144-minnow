#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  const auto& [seqno, SYN, payload, FIN] = message;
  /*
   * 收到 SYN 之后不再重复记录(超时重传、网络复制、旧连接残留):
   *   ISN 是这条连接的 “坐标系的原点” ，只能钉一次
   *     1. 对于超时重传（ISN 相同）：再记录一遍写的是同一个值 -- 忽略完全可行
   *     2. 对旧连接的重复 SYN 或伪造段（ISN 不同）：重复记录会把原点顶替，后面的 unwrap、ackno 全部错乱 -- 忽略是为了防御
   * —— 发送方“重传沿用 ISN” + 接收方“只认一个 SYN” = “一个连接内 ISN 固定”
   */
  if (SYN && !ISN_.has_value()) {
    ISN_ = seqno;
  }

  /*
   * 没有收到 SYN 之前的报文全部丢弃:
   *   1. 连接尚不存在 -- tcp 三次握手建立连接
   *   2. 没有 ISN 就没有坐标系 -- seqno <-> absolute_seqno
   *   3. 防御“旧连接的段”的污染：“四元组”会被反复利用
   *   4. 真实 tcp 对“不存在的连接”收到段时，会回 RST 拒绝; 这里简化为“直接丢弃”
   */
  if (!ISN_.has_value()) {
    return;
  }

  Wrap32 zero_point = ISN_.value();
  uint64_t checkpoint = inbound_stream.bytes_pushed() + 1;

  /* 将 seqno 变成 absolute_seqno */
  uint64_t absolute_seqno = seqno.unwrap(zero_point, checkpoint);
  /* 将 TCP 中的 absolute_seqno 变成 ByteStream/Reassembler 中的 indexs（从0开始）*/
  uint64_t first_index = absolute_seqno - !SYN;

  /* 将所有字节交给 Reassembler */
  reassembler.insert(first_index, static_cast<string>(payload), FIN, inbound_stream);
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  /* 窗口大小不超过 UINT16_MAX */
  uint16_t window_size = min(static_cast<uint64_t>(UINT16_MAX), inbound_stream.available_capacity());

  /* 如果接收方收到了 SYN 返回 ackno 给发送端，否则返回空 */
  if (ISN_) {
    /* 将 Stream 中的下标转换为 Wrap32：
     *   1. ackno 包含 SYN(占一个 seqno): 所以要 +1
     *   2. ackno 包含 FIN(占一个 seqno):
     *      ackno 对应 Reassebler 中的 first unassembled index，
     *      只有当 ackno 跨过最后一个序列号 FIN 时，才会 close,
     *      但是可能会很早就接收到 FIN 的段，所以只有当 is_closed() 为真时，要 + 1
     * - ackno = wrap(index + 1 + is_closed(), ISN_)
     */
    Wrap32 ackno = Wrap32::wrap(inbound_stream.bytes_pushed()+1+inbound_stream.is_closed(), ISN_.value());
    return { ackno, window_size };
  }
  return { {}, window_size };
}
