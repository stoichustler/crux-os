/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2022 The Android Open Source Project
 */

#ifndef __TCP6_H__
#define __TCP6_H__

#include <net6.h>
#include <net/tcp.h>

/**
 * typedef rxhand_tcp6_f() - An incoming TCP IPv6 packet handler.
 * @pkt: pointer to the application packet
 * @dport: destination TCP port
 * @sip: source IP6 address
 * @sport: source TCP port
 * @tcp_seq_num: TCP sequential number
 * @tcp_ack_num: TCP acknowledgment number
 * @action: TCP packet type (SYN, ACK, FIN, etc)
 */
typedef void rxhand_tcp6_f(uchar *pkt, u16 dport,
			   struct in6_addr sip, u16 sport,
			   u32 tcp_seq_num, u32 tcp_ack_num,
			   u8 action, unsigned int len);

/**
 * struct ip6_tcp_hdr_o - IP6 + TCP header + TCP options
 * @ip_hdr: IP6 + TCP header
 * @tcp_hdr: TCP header
 * @tcp_o: TCP options
 * @end: end of IP6/TCP header
 */
struct ip6_tcp_hdr_o {
	struct  ip6_hdr    ip_hdr;
	struct  tcp_hdr    tcp_hdr;
	struct  tcp_hdr_o  tcp_o;
	u8	end;
} __packed;

#define IP6_TCP_O_SIZE (sizeof(struct ip6_tcp_hdr_o))

/**
 * struct ip6_tcp_hdr_s - IP6 + TCP header + TCP options
 * @ip_hdr: IP6 + TCP header
 * @tcp_hdr: TCP header
 * @t_opt: TCP Timestamp Option
 * @sack_v: TCP SACK Option
 * @end: end of options
 */
struct ip6_tcp_hdr_s {
	struct  ip6_hdr    ip_hdr;
	struct  tcp_hdr    tcp_hdr;
	struct  tcp_t_opt  t_opt;
	struct  tcp_sack_v sack_v;
	u8	end;
} __packed;

#define IP6_TCP_SACK_SIZE (sizeof(struct ip6_tcp_hdr_s))

/**
 * union tcp6_build_pkt - union for building TCP/IP6 packet.
 * @ip: IP6 and TCP header plus TCP options
 * @sack: IP6 and TCP header plus SACK options
 * @raw: buffer
 */
union tcp6_build_pkt {
	struct ip6_tcp_hdr_o ip;
	struct ip6_tcp_hdr_s sack;
	uchar  raw[1600];
} __packed;

/**
 * net_set_tcp6_handler6() - set application TCP6 packet handler
 * @param f pointer to callback function
 */
void net_set_tcp_handler6(rxhand_tcp6_f *f);

/**
 * net_set_tcp_header6() - set
 * @pkt: pointer to IP6/TCP headers
 * @dport: destination TCP port
 * @sport: source TCP port
 * @payload_len: payload length
 * @action: TCP packet type (SYN, ACK, FIN, etc)
 * @tcp_seq_num: TCP sequential number
 * @tcp_ack_num: TCP acknowledgment number
 *
 * returns TCP header size
 */
int net_set_tcp_header6(uchar *pkt, u16 dport, u16 sport, int payload_len,
			u8 action, u32 tcp_seq_num, u32 tcp_ack_num);

/**
 * rxhand_tcp6() - handle incoming IP6 TCP packet
 * @param b pointer to IP6/TCP packet builder struct
 * @param len full packet length
 */
void rxhand_tcp6(union tcp6_build_pkt *b, unsigned int len);

#endif // __TCP6_H__
