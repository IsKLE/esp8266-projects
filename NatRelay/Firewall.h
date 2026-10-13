#pragma once
#include <stdint.h>
#include <stddef.h>

namespace RelayFirewall {
inline uint16_t be16(const uint8_t* p) { return (uint16_t(p[0]) << 8) | p[1]; }
inline uint32_t be32(const uint8_t* p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
// Called on the original Ethernet frame, before lwIP rewrites NAT addresses.
// ARP is link-local. Only DHCP replies may use a broadcast IPv4 destination.
inline bool allowUpstream(const uint8_t* f, size_t captured, size_t total, uint32_t wan) {
  if (captured < 14) return false;
  const uint16_t ether = be16(f + 12);
  if (ether == 0x0806) return total >= 42;
  if (ether != 0x0800 || captured < 34) return false;
  const uint8_t* ip = f + 14;
  const size_t h = (ip[0] & 15) * 4;
  const size_t len = be16(ip + 2);
  if ((ip[0] >> 4) != 4 || h < 20 || h > 60 || len < h || total < 14 + len || captured < 14 + h) return false;
  if (be16(ip + 6) & 0x3fff) return false; // NAPT does not safely handle fragments.
  const uint32_t dest = be32(ip + 16);
  const uint8_t proto = ip[9];
  const uint8_t* l4 = ip + h;
  if (proto == 17 && len >= h + 8 && captured >= 14 + h + 8 &&
      be16(l4) == 67 && be16(l4 + 2) == 68 && (dest == 0xffffffff || dest == wan)) return true;
  if (!wan || dest != wan) return false; // Includes every directly routed child address.
  if (proto == 6) {
    if (len < h + 20 || captured < 14 + h + 20) return false;
    const unsigned tcpHeader = (l4[12] >> 4) * 4;
    if (tcpHeader < 20 || len < h + tcpHeader) return false;
    // New inbound TCP connections cannot be opened, even to an occupied NAT port.
    return !(l4[13] & 2) || (l4[13] & 16);
  }
  if (proto == 17) return len >= h + 8 && captured >= 14 + h + 8 &&
      be16(l4 + 4) >= 8 && be16(l4 + 4) <= len - h;
  // Echo request terminates on the board; echo reply is matched by the NAPT table.
  if (proto == 1) return len >= h + 8 && captured >= 14 + h + 8 && (l4[0] == 0 || l4[0] == 8);
  return false;
}
}
