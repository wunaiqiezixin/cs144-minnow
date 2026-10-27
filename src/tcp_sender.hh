#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include <queue>
#include <deque>


class TCPSender
{
/* 计时器 */
class Timer
{
uint64_t initial_RTO_ms_;
uint64_t current_RTO_ms_;

bool running_ { false };
uint64_t elapesed_ms_ { 0 };

public:
  Timer( uint64_t initial_RTO_ms);
  void start();
  void stop() { running_ = false; }
  bool running() const { return running_; }
  bool expired() const { return running_ and elapesed_ms_ >= current_RTO_ms_; }
  void reset_rto() { current_RTO_ms_ = initial_RTO_ms_; }
  void backoff() { current_RTO_ms_ *= 2; }
  void advanced( uint64_t ms_since_last_tick ) { elapesed_ms_ += ms_since_last_tick; }
};

  Wrap32 isn_;
  uint64_t initial_RTO_ms_;

  std::deque<TCPSenderMessage> ready_to_send_ { }; // 存放待发送的报文
  std::queue<TCPSenderMessage> outstanding_ { };   // 存放已经发送未被确认的报文

  uint64_t next_abs_seqno_ { 0 };   // 下一个待分配的绝对序列号: [0, next_abs_seqno_) 已分配
  uint64_t abs_acked_ { 0 };        // 确认号: [0, abs_acked_) 已确认
  uint64_t window_size_ { 1 };      // 接收方窗口大小 (16 bit field)
  uint64_t bytes_in_flight_ { 0 };  // 已发送未被确认的字节数
  uint64_t consecutive_retx_ { 0 }; // 连续重传次数

  bool syn_sent_ { false }; // SYN 是否已经发送
  bool fin_sent_ { false }; // FIN 是否已经发送

  Timer timer_;

public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn );

  /* Push bytes from the outbound stream */
  void push( Reader& outbound_stream );

  /* Send a TCPSenderMessage if needed (or empty optional otherwise) */
  std::optional<TCPSenderMessage> maybe_send();

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage send_empty_message() const;

  /* Receive an act on a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called. */
  void tick( uint64_t ms_since_last_tick );

  /* Accessors for use in testing */
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
};
