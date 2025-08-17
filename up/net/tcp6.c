// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 The Android Open Source Project
 */

#include <common.h>
#include <net/tcp.h>
#include <net/tcp6.h>
#include <net6.h>

static rxhand_tcp6_f *tcp6_packet_handler;

static void dummy_handler(uchar *pkt, u16 dport,
			  struct in6_addr sip, u16 sport,
			  u32 tcp_seq_num, u32 tcp_ack_num,
			  u8 action, unsigned int len)
{
}

void net_set_tcp_handler6(rxhand_tcp6_f *f)
{
	if (!f)
		tcp6_packet_handler = dummy_handler;
	else
		tcp6_packet_handler = f;
}

int net_set_tcp_header6(uchar *pkt, u16 dport, u16 sport, int payload_len,
			u8 action, u32 tcp_seq_num, u32 tcp_ack_num)
{
	union tcp6_build_pkt *b = (union tcp6_build_pkt *)pkt;
	int tcp_hdr_len;
	int pkt_len;
	u16 csum;

	pkt_len = IP6_HDR_SIZE;
	tcp_hdr_len = net_set_tcp_header_common(&b->ip.tcp_hdr, &b->ip.tcp_o,
						&b->sack.t_opt, &b->sack.sack_v,
						dport, sport, payload_len, action,
						tcp_seq_num, tcp_ack_num);
	pkt_len += tcp_hdr_len;
	pkt_len += payload_len;

	csum = csum_partial((u8 *)&b->ip.tcp_hdr, tcp_hdr_len + payload_len, 0);
	b->ip.tcp_hdr.tcp_xsum = csum_ipv6_magic(&net_ip6, &net_server_ip6,
						 tcp_hdr_len + payload_len,
						 IPPROTO_TCP, csum);

	return tcp_hdr_len;
}

void rxhand_tcp6(union tcp6_build_pkt *b, unsigned int len)
{
	int tcp_len = len - IP6_HDR_SIZE;
	u8  tcp_action = TCP_DATA;
	u32 tcp_seq_num, tcp_ack_num;
	u32 res_tcp_seq_num, res_tcp_ack_num;
	int tcp_hdr_len, payload_len;

	net_copy_ip6(&net_server_ip6, &b->ip.ip_hdr.saddr);

	tcp_hdr_len = GET_TCP_HDR_LEN_IN_BYTES(b->ip.tcp_hdr.tcp_hlen);
	payload_len = tcp_len - tcp_hdr_len;

	if (tcp_hdr_len > TCP_HDR_SIZE)
		tcp_parse_options((uchar *)b + IP6_HDR_SIZE + TCP_HDR_SIZE,
				  tcp_hdr_len - TCP_HDR_SIZE);

	tcp_seq_num = ntohl(b->ip.tcp_hdr.tcp_seq);
	tcp_ack_num = ntohl(b->ip.tcp_hdr.tcp_ack);

	tcp_action = tcp_state_machine(b->ip.tcp_hdr.tcp_flags,
				       tcp_seq_num, &res_tcp_seq_num, &res_tcp_ack_num,
				       payload_len);

	if ((tcp_action & TCP_PUSH) || payload_len > 0) {
		debug_cond(DEBUG_DEV_PKT,
			   "TCP Notify (action=%x, Seq=%u,Ack=%u,Pay%d)\n",
			   tcp_action, tcp_seq_num, tcp_ack_num, payload_len);

		(*tcp6_packet_handler) ((uchar *)b + len - payload_len, b->ip.tcp_hdr.tcp_dst,
					b->ip.ip_hdr.saddr, b->ip.tcp_hdr.tcp_src, tcp_seq_num,
					tcp_ack_num, tcp_action, payload_len);
		tcp_update_last_connection_data_frame_time();

	} else if (tcp_action != TCP_DATA) {
		debug_cond(DEBUG_DEV_PKT,
			   "TCP Action (action=%x,Seq=%u,Ack=%u,Pay=%d)\n",
			   tcp_action, res_tcp_seq_num, res_tcp_ack_num, payload_len);

		net_send_tcp_packet6(0, ntohs(b->ip.tcp_hdr.tcp_src),
				     ntohs(b->ip.tcp_hdr.tcp_dst),
				     (tcp_action & (~TCP_PUSH)),
				     res_tcp_seq_num, res_tcp_ack_num);
	}
}
