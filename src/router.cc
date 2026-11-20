#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  route_table_.push_back({ route_prefix, prefix_length, next_hop, interface_num });
}

void Router::route()
{
  // 处理每个接口接收到的每个数据报，通过最长前缀匹配原则，找到其下一跳的 IP
  for (auto& networkInterface : interfaces_) {
    auto maybe_recv = networkInterface.maybe_receive();
    while (maybe_recv.has_value()) {
      InternetDatagram dgram = maybe_recv.value();
      // 如果 ttl 小于 2，直接丢弃该数据报，处理下一个
      if (dgram.header.ttl <= 1) {
        maybe_recv = networkInterface.maybe_receive();
        continue;
      }

      uint32_t dst = dgram.header.dst;

      // 遍历路由表找 IP 地址 dst 最长前缀匹配
      optional<pair<size_t, size_t>> lpm; // < 路由表下标，前缀匹配长度 >
      for (size_t i = 0; i < route_table_.size(); i++) {
        const auto& [route_prefix, prefix_length, next_hop, interface_num] = route_table_[i];
        // 前缀匹配逻辑：算术右移 (32 - prefix_length) 后是否相等
        uint32_t route_shift = 0, dst_shift = 0;
        if (prefix_length > 0) {
          route_shift = route_prefix >> (32 - prefix_length);
          dst_shift = dst >> (32 - prefix_length);
        }
        if ((route_shift == dst_shift)
         && (!lpm.has_value() || prefix_length > lpm.value().second)) {
            lpm.emplace(i, prefix_length);
        }
      }

      // 如果找到了最长匹配的下一跳地址，将数据报 dgram 发送给下一跳
      if (lpm.has_value()) {
        size_t idx = lpm.value().first; // 路由表的下标
        dgram.header.ttl--;
        dgram.header.compute_checksum();
        const auto& [route_prefix, prefix_length, next_hop, interface_num] = route_table_[idx];
        // 如果 next_hop 为空，说明下一跳就是数据报 dgram 的目的地址 dst
        if (!next_hop.has_value()) {
          // 使用 Address::from_ipv4_numeric() 将 uint32_t 转换成 Address 类型
          Address address = Address::from_ipv4_numeric(dst);
          interfaces_[interface_num].send_datagram(dgram, address);
        } else {
          interfaces_[interface_num].send_datagram(dgram, next_hop.value());
        }
      }
      // 如果没有找到任何的匹配项，丢弃该报文，处理该 NetworkInterface 接收到的下一个数据报
      maybe_recv = networkInterface.maybe_receive();
    }
  }
}
