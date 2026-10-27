#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ), initial_RTO_ms_( initial_RTO_ms ), timer_( initial_RTO_ms)
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return bytes_in_flight_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retx_;
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  if (ready_to_send_.empty()) {
    return { };
  }
  auto msg = ready_to_send_.front();
  ready_to_send_.pop_front();
  return { msg };
}

/* 从 outbound_stream 中尽可能地获取字节，并将其封装成 TCPSenderMessage */
void TCPSender::push( Reader& outbound_stream )
{
  while (!fin_sent_) {
    TCPSenderMessage message;
    uint64_t effective_window = max(static_cast<uint64_t>(1), window_size_); // 窗口大小为 0 时，默认为 1

    uint64_t window_end = abs_acked_ + effective_window; // 窗口右边界

    // 空闲窗口的大小，满了直接返回
    uint64_t rest_window_size = 0;
    if (next_abs_seqno_ >= window_end) {
      return;
    }
    rest_window_size = window_end - next_abs_seqno_;

    // SYN
    if (!syn_sent_) {
      syn_sent_ = true;
      message.SYN = true;
      rest_window_size--;
    }

    // 计算载荷段(payload)的大小
    uint64_t payload_size = min(rest_window_size, outbound_stream.bytes_buffered());
    payload_size = min(payload_size, TCPConfig::MAX_PAYLOAD_SIZE);

    // 从 outbound_stream 中读取 payload
    string payload;
    read(outbound_stream, payload_size, payload);
    rest_window_size -= payload_size;
    message.payload = static_cast<Buffer>(payload);

    // FIN
    if (outbound_stream.is_finished() && rest_window_size > 0) {
      fin_sent_ = true;
      message.FIN = true;
      rest_window_size--;
    }

    // 空报文直接返回
    if (!message.sequence_length()) {
      return;
    }

    // Update
    message.seqno = Wrap32::wrap(next_abs_seqno_, isn_);

    next_abs_seqno_ += message.sequence_length();
    bytes_in_flight_ += message.sequence_length();

    ready_to_send_.push_back(message);
    outstanding_.push(message);

    // 如果计时器没有在跑，启动计时器
    if (!timer_.running()) {
      timer_.start();
    }
  }
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  Buffer empty;
  return { Wrap32::wrap(next_abs_seqno_, isn_), false, empty, false };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  const auto& [recv_ack, window_size] = msg;

  if (!recv_ack.has_value()) {
    window_size_ = window_size;
    return;
  }

  uint64_t recv_abs_ack = (recv_ack.value()).unwrap(isn_, abs_acked_);

  if (recv_abs_ack > next_abs_seqno_ or recv_abs_ack < abs_acked_) {
    return;
  }
  window_size_ = window_size;
  if (recv_abs_ack == abs_acked_) {
    return;
  }

  while (!outstanding_.empty()) {
    const auto& front = outstanding_.front();
    uint64_t end_seqno = front.seqno.unwrap(isn_, abs_acked_) + front.sequence_length();
    if (recv_abs_ack >= end_seqno)
      outstanding_.pop();
    else
      break;
  }

  // Update
  bytes_in_flight_ -= recv_abs_ack - abs_acked_;
  abs_acked_ = recv_abs_ack;
  consecutive_retx_ = 0;
  timer_.reset_rto();

  if (outstanding_.empty())
    timer_.stop();
  else
    timer_.start();
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  if (!timer_.running())
    return;

  timer_.advanced(ms_since_last_tick);

  if (!timer_.expired())
    return;

  ready_to_send_.push_front(outstanding_.front());

  if (window_size_ > 0) {
    consecutive_retx_++;
    timer_.backoff();
  }

  timer_.start();
}

Timer::Timer( uint64_t initial_RTO_ms )
  : initial_RTO_ms_(initial_RTO_ms), current_RTO_ms_(initial_RTO_ms)
{}

void Timer::start()
{
  elapesed_ms_ = 0;
  running_ = true;
}
