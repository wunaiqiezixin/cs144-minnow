#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

using namespace std;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address ), ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  // 下一跳的原始 32 位 IP 地址
  uint32_t next_ip_address = next_hop.ipv4_numeric();

  // 如果 arp_table_ 存在到下一跳 MAC 的映射，将 dgram 成帧并放入 ready_to_send_ 队列
  if (arp_table_.find(next_ip_address) != arp_table_.end()) {
    EthernetHeader header = { arp_table_[next_ip_address].first, ethernet_address_, EthernetHeader::TYPE_IPv4 };
    EthernetFrame ethernet_frame = { header, serialize(dgram) };
    ready_to_send_.push(ethernet_frame);
    return;
  }

  // 构造 ARP 请求帧
  ARPMessage arp_message;
  arp_message.opcode = ARPMessage::OPCODE_REQUEST;
  arp_message.sender_ethernet_address = ethernet_address_;
  arp_message.sender_ip_address = ip_address_.ipv4_numeric();
  arp_message.target_ethernet_address = { };
  arp_message.target_ip_address = next_ip_address;
  EthernetHeader header = { ETHERNET_BROADCAST, ethernet_address_, EthernetHeader::TYPE_ARP };
  EthernetFrame arp_frame = { header, serialize(arp_message) };

  // 如果 5 秒内已经有其它的下一跳 IP 相同的数据报发送过 ARP 请求，则不再发送 ARP 请求
  if (await_for_reply_.find(next_ip_address) != await_for_reply_.end()) {
    auto& [dgram_vec, pendent_time] = await_for_reply_[next_ip_address];
    // 如果挂起时间大于等于 5 秒，重新发送 ARP 请求，并将挂起时间重置为 0
    if (pendent_time >= 5000) {
      ready_to_send_.push(arp_frame);
      pendent_time = 0;
    }
    dgram_vec.push_back(dgram);
  } else {
  // 没有下一跳 IP 相同的数据报发送过 ARP 请求
    ready_to_send_.push(arp_frame);
    await_for_reply_[next_ip_address] = { {dgram}, 0 };
  }
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  // 目标地址不匹配并且不是广播地址，不理会
  if (frame.header.dst != ETHERNET_BROADCAST && frame.header.dst != ethernet_address_)
    return { };

  // 如果收到的是 InternetDatagram
  if (frame.header.type == EthernetHeader::TYPE_IPv4) {
    InternetDatagram dgram;
    if (parse(dgram, frame.payload))
      return dgram;
    else
      return { };
  }

  // 如果收到的是 ARPMessage（广播）
  ARPMessage msg;
  if (parse(msg, frame.payload)) {
    // 不论是请求还是响应，都对发送方的 IP 和 MAC 做映射，保存到 arp_table_ 中
    arp_table_[msg.sender_ip_address] = { msg.sender_ethernet_address, elapsed_ms_ };

    // 如果收到的是一个 ARP 请求
    if (msg.opcode == ARPMessage::OPCODE_REQUEST &&
        msg.target_ip_address == ip_address_.ipv4_numeric()) {
      // 构造一个 ARP 响应帧，放入 ready_to_send_ 队列
      ARPMessage reply_msg;
      reply_msg.opcode = ARPMessage::OPCODE_REPLY;
      reply_msg.sender_ethernet_address = ethernet_address_;
      reply_msg.sender_ip_address = ip_address_.ipv4_numeric();
      reply_msg.target_ethernet_address = msg.sender_ethernet_address;
      reply_msg.target_ip_address = msg.sender_ip_address;
      EthernetFrame arp_reply_frame = { {msg.sender_ethernet_address, ethernet_address_, EthernetHeader::TYPE_ARP}, serialize(reply_msg) };
      ready_to_send_.push(arp_reply_frame);
    } else

    // 如果收到的是一个 ARP 响应
    if (msg.opcode == ARPMessage::OPCODE_REPLY &&
        await_for_reply_.find(msg.sender_ip_address) != await_for_reply_.end()) {
      // 取出所有待响应的 InternetDatagram，成帧，并放入 ready_to_send_ 队列
      auto dgram_vec = await_for_reply_[msg.sender_ip_address].first;
      await_for_reply_.erase(msg.sender_ip_address);
      for (auto& dgram : dgram_vec) {
        EthernetFrame ipv4_frame = { {msg.sender_ethernet_address, ethernet_address_, EthernetHeader::TYPE_IPv4}, serialize(dgram) };
        ready_to_send_.push(ipv4_frame);
      }
    }
  }
  return { };
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  // 递增总的流逝的时间
  elapsed_ms_ += ms_since_last_tick;

  // 增加 await_for_reply_ 中的挂起时间
  for (auto& item : await_for_reply_) {
    item.second.second += ms_since_last_tick;
  }

  // 删除 arp_table_ 中过期的映射(保证在 arp_table_ 中的映射全是有效的映射)
  for (auto it = arp_table_.begin(); it != arp_table_.end(); ) {
    if (elapsed_ms_ - it->second.second >= 30000)
      it = arp_table_.erase(it);
    else
      it++;
  }
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
  if (ready_to_send_.empty())
    return { };
  EthernetFrame ethernet_frame = ready_to_send_.front();
  ready_to_send_.pop();
  return ethernet_frame;
}
