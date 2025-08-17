// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2013 Allied Telesis Labs NZ
 * Chris Packham, <judge.packham@gmail.com>
 *
 * Copyright (C) 2022 YADRO
 * Viacheslav Mitrofanov <v.v.mitrofanov@yadro.com>
 */

/* Simple IPv6 network layer implementation */

#include <common.h>
#include <env_internal.h>
#include <malloc.h>
#include <net.h>
#include <net6.h>
#include <net/tcp6.h>
#include <ndisc.h>

/* NULL IPv6 address */
struct in6_addr const net_null_addr_ip6 = ZERO_IPV6_ADDR;
/* Our gateway's IPv6 address */
struct in6_addr net_gateway6 = ZERO_IPV6_ADDR;
/* Our IPv6 addr (0 = unknown) */
struct in6_addr net_ip6 = ZERO_IPV6_ADDR;
/* Our link local IPv6 addr (0 = unknown) */
struct in6_addr net_link_local_ip6 = ZERO_IPV6_ADDR;
/* set server IPv6 addr (0 = unknown) */
struct in6_addr net_server_ip6 = ZERO_IPV6_ADDR;
/* The prefix length of our network */
u32 net_prefix_length;

bool use_ip6;

static int on_ip6addr(const char *name, const char *value, enum env_op op,
		      int flags)
{
	char *mask;
	size_t len;

	if (flags & H_PROGRAMMATIC)
		return 0;

	if (op == env_op_delete) {
		net_prefix_length = 0;
		net_copy_ip6(&net_ip6, &net_null_addr_ip6);
		return 0;
	}

	mask = strchr(value, '/');

	if (mask) {
		net_prefix_length = simple_strtoul(mask + 1, NULL, 10);
		len = mask - value;
	} else {
		len = strlen(value);
	}

	return string_to_ip6(value, len, &net_ip6);
}

U_BOOT_ENV_CALLBACK(ip6addr, on_ip6addr);

static int on_gatewayip6(const char *name, const char *value, enum env_op op,
			 int flags)
{
	if (flags & H_PROGRAMMATIC)
		return 0;

	return string_to_ip6(value, strlen(value), &net_gateway6);
}

U_BOOT_ENV_CALLBACK(gatewayip6, on_gatewayip6);

static int on_serverip6(const char *name, const char *value, enum env_op op,
			int flags)
{
	if (flags & H_PROGRAMMATIC)
		return 0;

	return string_to_ip6(value, strlen(value), &net_server_ip6);
}

U_BOOT_ENV_CALLBACK(serverip6, on_serverip6);

int ip6_is_unspecified_addr(struct in6_addr *addr)
{
	return !(addr->s6_addr32[0] | addr->s6_addr32[1] |
		addr->s6_addr32[2] | addr->s6_addr32[3]);
}

int ip6_is_our_addr(struct in6_addr *addr)
{
	return !memcmp(addr, &net_link_local_ip6, sizeof(struct in6_addr)) ||
	       !memcmp(addr, &net_ip6, sizeof(struct in6_addr));
}

void ip6_make_eui(unsigned char eui[8], unsigned char const enetaddr[6])
{
	memcpy(eui, enetaddr, 3);
	memcpy(&eui[5], &enetaddr[3], 3);
	eui[3] = 0xff;
	eui[4] = 0xfe;
	eui[0] ^= 2;		/* "u" bit set to indicate global scope */
}

void ip6_make_lladdr(struct in6_addr *lladr, unsigned char const enetaddr[6])
{
	unsigned char eui[8];

	memset(lladr, 0, sizeof(struct in6_addr));
	lladr->s6_addr16[0] = htons(IPV6_LINK_LOCAL_PREFIX);
	ip6_make_eui(eui, enetaddr);
	memcpy(&lladr->s6_addr[8], eui, 8);
}

void ip6_make_snma(struct in6_addr *mcast_addr, struct in6_addr *ip6_addr)
{
	memset(mcast_addr, 0, sizeof(struct in6_addr));
	mcast_addr->s6_addr[0] = 0xff;
	mcast_addr->s6_addr[1] = IPV6_ADDRSCOPE_LINK;
	mcast_addr->s6_addr[11] = 0x01;
	mcast_addr->s6_addr[12] = 0xff;
	mcast_addr->s6_addr[13] = ip6_addr->s6_addr[13];
	mcast_addr->s6_addr[14] = ip6_addr->s6_addr[14];
	mcast_addr->s6_addr[15] = ip6_addr->s6_addr[15];
}

void
ip6_make_mult_ethdstaddr(unsigned char enetaddr[6], struct in6_addr *mcast_addr)
{
	enetaddr[0] = 0x33;
	enetaddr[1] = 0x33;
	memcpy(&enetaddr[2], &mcast_addr->s6_addr[12], 4);
}

int
ip6_addr_in_subnet(struct in6_addr *our_addr, struct in6_addr *neigh_addr,
		   u32 plen)
{
	__be32 *addr_dwords;
	__be32 *neigh_dwords;

	addr_dwords = our_addr->s6_addr32;
	neigh_dwords = neigh_addr->s6_addr32;

	while (plen > 32) {
		if (*addr_dwords++ != *neigh_dwords++)
			return 0;

		plen -= 32;
	}

	/* Check any remaining bits */
	if (plen > 0) {
		if ((*addr_dwords >> (32 - plen)) !=
		    (*neigh_dwords >> (32 - plen))) {
			return 0;
		}
	}

	return 1;
}

static inline unsigned int csum_fold(unsigned int sum)
{
	sum = (sum & 0xffff) + (sum >> 16);
	sum = (sum & 0xffff) + (sum >> 16);

	/* Opaque moment. If reverse it to zero it will not be checked on
	 * receiver's side. It leads to bad negibour advertisement.
	 */
	if (sum == 0xffff)
		return sum;

	return ~sum;
}

static inline unsigned short from32to16(unsigned int x)
{
	/* add up 16-bit and 16-bit for 16+c bit */
	x = (x & 0xffff) + (x >> 16);
	/* add up carry.. */
	x = (x & 0xffff) + (x >> 16);
	return x;
}

static u32 csum_do_csum(const u8 *buff, int len)
{
	int odd;
	unsigned int result = 0;

	if (len <= 0)
		goto out;
	odd = 1 & (unsigned long)buff;
	if (odd) {
#ifdef __LITTLE_ENDIAN
		result += (*buff << 8);
#else
		result = *buff;
#endif
		len--;
		buff++;
	}
	if (len >= 2) {
		if (2 & (unsigned long)buff) {
			result += *(unsigned short *)buff;
			len -= 2;
			buff += 2;
		}
		if (len >= 4) {
			const unsigned char *end = buff + ((u32)len & ~3);
			unsigned int carry = 0;

			do {
				unsigned int w = *(unsigned int *)buff;

				buff += 4;
				result += carry;
				result += w;
				carry = (w > result);
			} while (buff < end);
			result += carry;
			result = (result & 0xffff) + (result >> 16);
		}
		if (len & 2) {
			result += *(unsigned short *)buff;
			buff += 2;
		}
	}
	if (len & 1)
#ifdef __LITTLE_ENDIAN
		result += *buff;
#else
		result += (*buff << 8);
#endif
	result = from32to16(result);
	if (odd)
		result = ((result >> 8) & 0xff) | ((result & 0xff) << 8);
out:
	return result;
}

unsigned int csum_partial(const unsigned char *buff, int len, unsigned int sum)
{
	unsigned int result = csum_do_csum(buff, len);

	/* add in old sum, and carry.. */
	result += sum;
	/* 16+c bits -> 16 bits */
	result = (result & 0xffff) + (result >> 16);
	return result;
}

unsigned short int
csum_ipv6_magic(struct in6_addr *saddr, struct in6_addr *daddr, u16 len,
		unsigned short proto, unsigned int csum)
{
	int carry;
	u32 ulen;
	u32 uproto;
	u32 sum = csum;

	sum += saddr->s6_addr32[0];
	carry = (sum < saddr->s6_addr32[0]);
	sum += carry;

	sum += saddr->s6_addr32[1];
	carry = (sum < saddr->s6_addr32[1]);
	sum += carry;

	sum += saddr->s6_addr32[2];
	carry = (sum < saddr->s6_addr32[2]);
	sum += carry;

	sum += saddr->s6_addr32[3];
	carry = (sum < saddr->s6_addr32[3]);
	sum += carry;

	sum += daddr->s6_addr32[0];
	carry = (sum < daddr->s6_addr32[0]);
	sum += carry;

	sum += daddr->s6_addr32[1];
	carry = (sum < daddr->s6_addr32[1]);
	sum += carry;

	sum += daddr->s6_addr32[2];
	carry = (sum < daddr->s6_addr32[2]);
	sum += carry;

	sum += daddr->s6_addr32[3];
	carry = (sum < daddr->s6_addr32[3]);
	sum += carry;

	ulen = htonl((u32)len);
	sum += ulen;
	carry = (sum < ulen);
	sum += carry;

	uproto = htonl(proto);
	sum += uproto;
	carry = (sum < uproto);
	sum += carry;

	return csum_fold(sum);
}

int ip6_add_hdr(uchar *xip, struct in6_addr *src, struct in6_addr *dest,
		int nextheader, int hoplimit, int payload_len)
{
	struct ip6_hdr *ip6 = (struct ip6_hdr *)xip;

	ip6->version = 6;
	ip6->priority = 0;
	ip6->flow_lbl[0] = 0;
	ip6->flow_lbl[1] = 0;
	ip6->flow_lbl[2] = 0;
	ip6->payload_len = htons(payload_len);
	ip6->nexthdr = nextheader;
	ip6->hop_limit = hoplimit;
	net_copy_ip6(&ip6->saddr, src);
	net_copy_ip6(&ip6->daddr, dest);

	return sizeof(struct ip6_hdr);
}

int udp6_add_hdr(uchar *xip, struct in6_addr *dest, int dport, int sport,
		 int len)
{
	struct udp_hdr *udp;
	u16 csum_p;

	udp = (struct udp_hdr *)xip;

	udp->udp_dst = htons(dport);
	udp->udp_src = htons(sport);
	udp->udp_len = htons(len + UDP_HDR_SIZE);

	/* checksum */
	udp->udp_xsum = 0;
	csum_p = csum_partial((u8 *)udp, len + UDP_HDR_SIZE, 0);
	udp->udp_xsum = csum_ipv6_magic(&net_ip6, dest, len + UDP_HDR_SIZE,
					IPPROTO_UDP, csum_p);

	return sizeof(struct udp_hdr);
}

int net_send_ip_packet6(uchar *ether, struct in6_addr *dest, int dport, int sport,
			int payload_len, int proto, u8 action, u32 tcp_seq_num,
			u32 tcp_ack_num)
{
	uchar *pkt;
	int eth_hdr_size;
	int ip_hdr_size;
	int udp_hdr_size;
	int tcp_hdr_size;
	int pkt_hdr_size;

	/* make sure the net_tx_packet is initialized (net_init() was called) */
	assert(net_tx_packet != NULL);
	if (net_tx_packet == NULL)
		return -1;

	pkt = (uchar *)net_tx_packet;

	eth_hdr_size = net_set_ether(pkt, ether, PROT_IP6);
	pkt_hdr_size = eth_hdr_size;
	pkt += eth_hdr_size;

	switch (proto) {
#if defined(CONFIG_PROT_UDP)
	case IPPROTO_UDP:
		ip_hdr_size = ip6_add_hdr(pkt, &net_ip6, dest, IPPROTO_UDP, 64,
					  payload_len + UDP_HDR_SIZE);
		pkt_hdr_size += ip_hdr_size;
		pkt += ip_hdr_size;

		udp_hdr_size = udp6_add_hdr(pkt, dest, dport, sport, payload_len);
		pkt_hdr_size += udp_hdr_size;
		pkt += udp_hdr_size;
		break;
#endif
#if defined(CONFIG_PROT_TCP)
	case IPPROTO_TCP:
		tcp_hdr_size = net_set_tcp_header6(pkt, dport, sport,
						   payload_len, action, tcp_seq_num,
						   tcp_ack_num);
		ip_hdr_size = ip6_add_hdr(pkt, &net_ip6, dest, IPPROTO_TCP, 64,
					  tcp_hdr_size + payload_len);

		pkt_hdr_size += ip_hdr_size + tcp_hdr_size;
		pkt += ip_hdr_size + tcp_hdr_size;
		break;
#endif
	default:
		return -EINVAL;
	}

	/* if MAC address was not discovered yet, save the packet and do
	 * neighbour discovery
	 */
	if (memcmp(ether, net_null_ethaddr, 6) == 0) {
		memcpy((uchar *)net_nd_tx_packet,
		       (uchar *)net_tx_packet, pkt_hdr_size + payload_len);
		memset((uchar *)net_tx_packet, 0, pkt_hdr_size + payload_len);

		net_copy_ip6(&net_nd_sol_packet_ip6, dest);
		net_nd_packet_mac_out = ether;
		/* size of the waiting packet */
		net_nd_tx_packet_size = pkt_hdr_size + payload_len;
		net_nd_try = 1;
		net_nd_timer_start = get_timer(0);
		ndisc_request();
		return 1;	/* waiting */
	}

	(void)eth_send(net_tx_packet, pkt_hdr_size + payload_len);

	return 0;	/* transmitted */
}

int net_send_udp_packet6(uchar *ether, struct in6_addr *dest, int dport,
			 int sport, int len)
{
	return net_send_ip_packet6(ether, dest, dport, sport, len, IPPROTO_UDP, 0, 0, 0);
}

int net_send_tcp_packet6(int payload_len, int dport, int sport, u8 action,
			 u32 tcp_seq_num, u32 tcp_ack_num)
{
	return net_send_ip_packet6(net_server_ethaddr, &net_server_ip6, dport,
				   sport, payload_len, IPPROTO_TCP, action,
				   tcp_seq_num, tcp_ack_num);
}

int net_ip6_handler(struct ethernet_hdr *et, struct ip6_hdr *ip6, int len)
{
	union tcp6_build_pkt *tcp_headers;
	struct in_addr zero_ip = {.s_addr = 0 };
	struct icmp6hdr *icmp;
	struct udp_hdr *udp;
	struct tcp_hdr *tcp;
	u16 csum;
	u16 csum_p;
	u16 hlen;

	if (len < IP6_HDR_SIZE)
		return -EINVAL;

	if (ip6->version != 6)
		return -EINVAL;

	switch (ip6->nexthdr) {
	case PROT_ICMPV6:
		icmp = (struct icmp6hdr *)(((uchar *)ip6) + IP6_HDR_SIZE);
		csum = icmp->icmp6_cksum;
		hlen = ntohs(ip6->payload_len);
		icmp->icmp6_cksum = 0;
		/* checksum */
		csum_p = csum_partial((u8 *)icmp, hlen, 0);
		icmp->icmp6_cksum = csum_ipv6_magic(&ip6->saddr, &ip6->daddr,
						    hlen, PROT_ICMPV6, csum_p);

		if (icmp->icmp6_cksum != csum)
			return -EINVAL;

		switch (icmp->icmp6_type) {
		case IPV6_ICMP_ECHO_REQUEST:
		case IPV6_ICMP_ECHO_REPLY:
			ping6_receive(et, ip6, len);
			break;
		case IPV6_NDISC_NEIGHBOUR_SOLICITATION:
		case IPV6_NDISC_NEIGHBOUR_ADVERTISEMENT:
		case IPV6_NDISC_ROUTER_ADVERTISEMENT:
			ndisc_receive(et, ip6, len);
			break;
		default:
			break;
		}
		break;
	case IPPROTO_UDP:
		udp = (struct udp_hdr *)(((uchar *)ip6) + IP6_HDR_SIZE);
		csum = udp->udp_xsum;
		hlen = ntohs(ip6->payload_len);
		udp->udp_xsum = 0;
		/* checksum */
		csum_p = csum_partial((u8 *)udp, hlen, 0);
		udp->udp_xsum = csum_ipv6_magic(&ip6->saddr, &ip6->daddr,
						hlen, IPPROTO_UDP, csum_p);

		if (csum != udp->udp_xsum)
			return -EINVAL;

		/* IP header OK. Pass the packet to the current handler. */
		net_get_udp_handler()((uchar *)ip6 + IP6_HDR_SIZE +
					UDP_HDR_SIZE,
				ntohs(udp->udp_dst),
				zero_ip,
				ntohs(udp->udp_src),
				ntohs(udp->udp_len) - 8);
		break;
	case IPPROTO_TCP:
		tcp = (struct tcp_hdr *)(((uchar *)ip6) + IP6_HDR_SIZE);
		csum = tcp->tcp_xsum;
		hlen = ntohs(ip6->payload_len);
		tcp->tcp_xsum = 0;
		/* checksum */
		csum_p = csum_partial((u8 *)tcp, hlen, 0);
		tcp->tcp_xsum = csum_ipv6_magic(&ip6->saddr, &ip6->daddr,
						hlen, IPPROTO_TCP, csum_p);

		if (csum != tcp->tcp_xsum)
			return -EINVAL;

		tcp_headers = (union tcp6_build_pkt *)ip6;
		rxhand_tcp6(tcp_headers, len);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
