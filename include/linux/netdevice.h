/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Definitions for the Interfaces handler.
 *
 * Version:	@(#)dev.h	1.0.10	08/12/93
 *
 * Authors:	Ross Biro
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *		Corey Minyard <wf-rch!minyard@relay.EU.net>
 *		Donald J. Becker, <becker@cesdis.gsfc.nasa.gov>
 *		Alan Cox, <alan@lxorguk.ukuu.org.uk>
 *		Bjorn Ekwall. <bj0rn@blox.se>
 *              Pekka Riikonen <priikone@poseidon.pspt.fi>
 *
 *		Moved to /usr/include/linux for NET3
 */
#ifndef _LINUX_NETDEVICE_H
#define _LINUX_NETDEVICE_H

#include <linux/timer.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/prefetch.h>
#include <asm/cache.h>
#include <asm/byteorder.h>
#include <asm/local.h>

#include <linux/percpu.h>
#include <linux/rculist.h>
#include <linux/workqueue.h>
#include <linux/dynamic_queue_limits.h>

#include <net/net_namespace.h>
#ifdef CONFIG_DCB
#include <net/dcbnl.h>
#endif
#include <net/netprio_cgroup.h>
#include <linux/netdev_features.h>
#include <linux/neighbour.h>
#include <linux/netdevice_xmit.h>
#include <uapi/linux/netdevice.h>
#include <uapi/linux/if_bonding.h>
#include <uapi/linux/pkt_cls.h>
#include <uapi/linux/netdev.h>
#include <linux/hashtable.h>
#include <linux/rbtree.h>
#include <net/net_trackers.h>
#include <net/net_debug.h>
#include <net/dropreason-core.h>
#include <net/neighbour_tables.h>

struct netpoll_info;
struct device;
struct ethtool_ops;
struct kernel_hwtstamp_config;
struct phy_device;
struct dsa_port;
struct ip_tunnel_parm_kern;
struct macsec_context;
struct macsec_ops;
struct netdev_config;
struct netdev_name_node;
struct sd_flow_limit;
struct sfp_bus;
/* 802.11 specific */
struct wireless_dev;
/* 802.15.4 specific */
struct wpan_dev;
struct mpls_dev;
/* UDP Tunnel offloads */
struct udp_tunnel_info;
struct udp_tunnel_nic_info;
struct udp_tunnel_nic;
struct bpf_prog;
struct xdp_buff;
struct xdp_frame;
struct xdp_metadata_ops;
struct xdp_md;
struct ethtool_netdev_state;
struct phy_link_topology;
struct hwtstamp_provider;

typedef u32 xdp_features_t;

void synchronize_net(void);
void netdev_set_default_ethtool_ops(struct net_device *dev,
				    const struct ethtool_ops *ops);
void netdev_sw_irq_coalesce_default_on(struct net_device *dev);

/* Backlog congestion levels */
#define NET_RX_SUCCESS		0	/* keep 'em coming, baby */
#define NET_RX_DROP		1	/* packet dropped */

#define MAX_NEST_DEV 8

/*
 * Transmit return codes: transmit return codes originate from three different
 * namespaces:
 *
 * - qdisc return codes
 * - driver transmit return codes
 * - errno values
 *
 * Drivers are allowed to return any one of those in their hard_start_xmit()
 * function. Real network devices commonly used with qdiscs should only return
 * the driver transmit return codes though - when qdiscs are used, the actual
 * transmission happens asynchronously, so the value is not propagated to
 * higher layers. Virtual network devices transmit synchronously; in this case
 * the driver transmit return codes are consumed by dev_queue_xmit(), and all
 * others are propagated to higher layers.
 */

/* qdisc ->enqueue() return codes. */
#define NET_XMIT_SUCCESS	0x00
#define NET_XMIT_DROP		0x01	/* skb dropped			*/
#define NET_XMIT_CN		0x02	/* congestion notification	*/
#define NET_XMIT_MASK		0x0f	/* qdisc flags in net/sch_generic.h */

/* NET_XMIT_CN is special. It does not guarantee that this packet is lost. It
 * indicates that the device will soon be dropping packets, or already drops
 * some packets of the same priority; prompting us to send less aggressively. */
#define net_xmit_eval(e)	((e) == NET_XMIT_CN ? 0 : (e))
#define net_xmit_errno(e)	((e) != NET_XMIT_CN ? -ENOBUFS : 0)

/* Driver transmit return codes */
#define NETDEV_TX_MASK		0xf0

enum netdev_tx {
	__NETDEV_TX_MIN	 = INT_MIN,	/* make sure enum is signed */
	NETDEV_TX_OK	 = 0x00,	/* driver took care of packet */
	NETDEV_TX_BUSY	 = 0x10,	/* driver tx path was busy*/
};
typedef enum netdev_tx netdev_tx_t;

/*
 * Current order: NETDEV_TX_MASK > NET_XMIT_MASK >= 0 is significant;
 * hard_start_xmit() return < NET_XMIT_MASK means skb was consumed.
 */
static inline bool dev_xmit_complete(int rc)
{
	/*
	 * Positive cases with an skb consumed by a driver:
	 * - successful transmission (rc == NETDEV_TX_OK)
	 * - error while transmitting (rc < 0)
	 * - error while queueing to a different device (rc & NET_XMIT_MASK)
	 */
	if (likely(rc < NET_XMIT_MASK))
		return true;

	return false;
}

/*
 *	Compute the worst-case header length according to the protocols
 *	used.
 */

#if defined(CONFIG_HYPERV_NET)
# define LL_MAX_HEADER 128
#elif defined(CONFIG_WLAN) || IS_ENABLED(CONFIG_AX25)
# if defined(CONFIG_MAC80211_MESH)
#  define LL_MAX_HEADER 128
# else
#  define LL_MAX_HEADER 96
# endif
#else
# define LL_MAX_HEADER 32
#endif

#if !IS_ENABLED(CONFIG_NET_IPIP) && !IS_ENABLED(CONFIG_NET_IPGRE) && \
    !IS_ENABLED(CONFIG_IPV6_SIT) && !IS_ENABLED(CONFIG_IPV6_TUNNEL)
#define MAX_HEADER LL_MAX_HEADER
#else
#define MAX_HEADER (LL_MAX_HEADER + 48)
#endif

/*
 *	Old network device statistics. Fields are native words
 *	(unsigned long) so they can be read and written atomically.
 */

#define NET_DEV_STAT(FIELD)			\
	union {					\
		unsigned long FIELD;		\
		atomic_long_t __##FIELD;	\
	}

struct net_device_stats {
	NET_DEV_STAT(rx_packets);
	NET_DEV_STAT(tx_packets);
	NET_DEV_STAT(rx_bytes);
	NET_DEV_STAT(tx_bytes);
	NET_DEV_STAT(rx_errors);
	NET_DEV_STAT(tx_errors);
	NET_DEV_STAT(rx_dropped);
	NET_DEV_STAT(tx_dropped);
	NET_DEV_STAT(multicast);
	NET_DEV_STAT(collisions);
	NET_DEV_STAT(rx_length_errors);
	NET_DEV_STAT(rx_over_errors);
	NET_DEV_STAT(rx_crc_errors);
	NET_DEV_STAT(rx_frame_errors);
	NET_DEV_STAT(rx_fifo_errors);
	NET_DEV_STAT(rx_missed_errors);
	NET_DEV_STAT(tx_aborted_errors);
	NET_DEV_STAT(tx_carrier_errors);
	NET_DEV_STAT(tx_fifo_errors);
	NET_DEV_STAT(tx_heartbeat_errors);
	NET_DEV_STAT(tx_window_errors);
	NET_DEV_STAT(rx_compressed);
	NET_DEV_STAT(tx_compressed);
};
#undef NET_DEV_STAT

/* per-cpu stats, allocated on demand.
 * Try to fit them in a single cache line, for dev_get_stats() sake.
 */
struct net_device_core_stats {
	unsigned long	rx_dropped;
	unsigned long	tx_dropped;
	unsigned long	rx_nohandler;
	unsigned long	rx_otherhost_dropped;
} __aligned(4 * sizeof(unsigned long));

#include <linux/cache.h>
#include <linux/skbuff.h>

struct neighbour;
struct neigh_parms;
struct sk_buff;

struct netdev_hw_addr {
	struct list_head	list;
	struct rb_node		node;
	unsigned char		addr[MAX_ADDR_LEN];
	unsigned char		type;
#define NETDEV_HW_ADDR_T_LAN		1
#define NETDEV_HW_ADDR_T_SAN		2
#define NETDEV_HW_ADDR_T_UNICAST	3
#define NETDEV_HW_ADDR_T_MULTICAST	4
	bool			global_use;
	int			sync_cnt;
	int			refcount;
	int			synced;
	struct rcu_head		rcu_head;
};

struct netdev_hw_addr_list {
	struct list_head	list;
	int			count;

	/* Auxiliary tree for faster lookup on addition and deletion */
	struct rb_root		tree;
};

#define netdev_hw_addr_list_count(l) ((l)->count)
#define netdev_hw_addr_list_empty(l) (netdev_hw_addr_list_count(l) == 0)
#define netdev_hw_addr_list_for_each(ha, l) \
	list_for_each_entry(ha, &(l)->list, list)

#define netdev_uc_count(dev) netdev_hw_addr_list_count(&(dev)->uc)
#define netdev_uc_empty(dev) netdev_hw_addr_list_empty(&(dev)->uc)
#define netdev_for_each_uc_addr(ha, dev) \
	netdev_hw_addr_list_for_each(ha, &(dev)->uc)
#define netdev_for_each_synced_uc_addr(_ha, _dev) \
	netdev_for_each_uc_addr((_ha), (_dev)) \
		if ((_ha)->sync_cnt)

#define netdev_mc_count(dev) netdev_hw_addr_list_count(&(dev)->mc)
#define netdev_mc_empty(dev) netdev_hw_addr_list_empty(&(dev)->mc)
#define netdev_for_each_mc_addr(ha, dev) \
	netdev_hw_addr_list_for_each(ha, &(dev)->mc)
#define netdev_for_each_synced_mc_addr(_ha, _dev) \
	netdev_for_each_mc_addr((_ha), (_dev)) \
		if ((_ha)->sync_cnt)

struct hh_cache {
	unsigned int	hh_len;
	seqlock_t	hh_lock;

	/* cached hardware header; allow for machine alignment needs.        */
#define HH_DATA_MOD	16
#define HH_DATA_OFF(__len) \
	(HH_DATA_MOD - (((__len - 1) & (HH_DATA_MOD - 1)) + 1))
#define HH_DATA_ALIGN(__len) \
	(((__len)+(HH_DATA_MOD-1))&~(HH_DATA_MOD - 1))
	unsigned long	hh_data[HH_DATA_ALIGN(LL_MAX_HEADER) / sizeof(long)];
};

/* Reserve HH_DATA_MOD byte-aligned hard_header_len, but at least that much.
 * Alternative is:
 *   dev->hard_header_len ? (dev->hard_header_len +
 *                           (HH_DATA_MOD - 1)) & ~(HH_DATA_MOD - 1) : 0
 *
 * We could use other alignment values, but we must maintain the
 * relationship HH alignment <= LL alignment.
 */
#define LL_RESERVED_SPACE(dev) \
	((((dev)->hard_header_len + READ_ONCE((dev)->needed_headroom)) \
	  & ~(HH_DATA_MOD - 1)) + HH_DATA_MOD)
#define LL_RESERVED_SPACE_EXTRA(dev,extra) \
	((((dev)->hard_header_len + READ_ONCE((dev)->needed_headroom) + (extra)) \
	  & ~(HH_DATA_MOD - 1)) + HH_DATA_MOD)

struct header_ops {
	int	(*create) (struct sk_buff *skb, struct net_device *dev,
			   unsigned short type, const void *daddr,
			   const void *saddr, unsigned int len);
	int	(*parse)(const struct sk_buff *skb, unsigned char *haddr);
	int	(*cache)(const struct neighbour *neigh, struct hh_cache *hh, __be16 type);
	void	(*cache_update)(struct hh_cache *hh,
				const struct net_device *dev,
				const unsigned char *haddr);
	bool	(*validate)(const char *ll_header, unsigned int len);
	__be16	(*parse_protocol)(const struct sk_buff *skb);
};

/* These flag bits are private to the generic network queueing
 * layer; they may not be explicitly referenced by any other
 * code.
 */

enum netdev_state_t {
	__LINK_STATE_START,
	__LINK_STATE_PRESENT,
	__LINK_STATE_NOCARRIER,
	__LINK_STATE_LINKWATCH_PENDING,
	__LINK_STATE_DORMANT,
	__LINK_STATE_TESTING,
};

struct gro_list {
	struct list_head	list;
	int			count;
};

/*
 * size of gro hash buckets, must be <= the number of bits in
 * gro_node::bitmask
 */
#define GRO_HASH_BUCKETS	8

/**
 * struct gro_node - structure to support Generic Receive Offload
 * @bitmask: bitmask to indicate used buckets in @hash
 * @hash: hashtable of pending aggregated skbs, separated by flows
 * @rx_list: list of pending ``GRO_NORMAL`` skbs
 * @rx_count: cached current length of @rx_list
 * @cached_napi_id: napi_struct::napi_id cached for hotpath, 0 for standalone
 */
struct gro_node {
	unsigned long		bitmask;
	struct gro_list		hash[GRO_HASH_BUCKETS];
	struct list_head	rx_list;
	u32			rx_count;
	u32			cached_napi_id;
};

/*
 * Structure for per-NAPI config
 */
struct napi_config {
	u64 gro_flush_timeout;
	u64 irq_suspend_timeout;
	u32 defer_hard_irqs;
	cpumask_t affinity_mask;
	u8 threaded;
	unsigned int napi_id;
};

/*
 * Structure for NAPI scheduling similar to tasklet but with weighting
 */
struct napi_struct {
	/* The poll_list must only be managed by the entity which
	 * changes the state of the NAPI_STATE_SCHED bit.  This means
	 * whoever atomically sets that bit can add this napi_struct
	 * to the per-CPU poll_list, and whoever clears that bit
	 * can remove from the list right before clearing the bit.
	 */
	struct list_head	poll_list;

	unsigned long		state;
	int			weight;
	u32			defer_hard_irqs_count;
	int			(*poll)(struct napi_struct *, int);
#ifdef CONFIG_NETPOLL
	/* CPU actively polling if netpoll is configured */
	int			poll_owner;
#endif
	/* CPU on which NAPI has been scheduled for processing */
	int			list_owner;
	struct net_device	*dev;
	struct sk_buff		*skb;
	struct gro_node		gro;
	struct hrtimer		timer;
	/* all fields past this point are write-protected by netdev_lock */
	struct task_struct	*thread;
	unsigned long		gro_flush_timeout;
	unsigned long		irq_suspend_timeout;
	u32			defer_hard_irqs;
	/* control-path-only fields follow */
	u32			napi_id;
	struct list_head	dev_list;
	struct hlist_node	napi_hash_node;
	int			irq;
	struct irq_affinity_notify notify;
	int			napi_rmap_idx;
	int			index;
	struct napi_config	*config;
};

enum {
	NAPI_STATE_SCHED,		/* Poll is scheduled */
	NAPI_STATE_MISSED,		/* reschedule a napi */
	NAPI_STATE_DISABLE,		/* Disable pending */
	NAPI_STATE_NPSVC,		/* Netpoll - don't dequeue from poll_list */
	NAPI_STATE_LISTED,		/* NAPI added to system lists */
	NAPI_STATE_NO_BUSY_POLL,	/* Do not add in napi_hash, no busy polling */
	NAPI_STATE_IN_BUSY_POLL,	/* sk_busy_loop() owns this NAPI */
	NAPI_STATE_PREFER_BUSY_POLL,	/* prefer busy-polling over softirq processing*/
	NAPI_STATE_THREADED,		/* The poll is performed inside its own thread*/
	NAPI_STATE_SCHED_THREADED,	/* Napi is currently scheduled in threaded mode */
	NAPI_STATE_HAS_NOTIFIER,	/* Napi has an IRQ notifier */
};

enum {
	NAPIF_STATE_SCHED		= BIT(NAPI_STATE_SCHED),
	NAPIF_STATE_MISSED		= BIT(NAPI_STATE_MISSED),
	NAPIF_STATE_DISABLE		= BIT(NAPI_STATE_DISABLE),
	NAPIF_STATE_NPSVC		= BIT(NAPI_STATE_NPSVC),
	NAPIF_STATE_LISTED		= BIT(NAPI_STATE_LISTED),
	NAPIF_STATE_NO_BUSY_POLL	= BIT(NAPI_STATE_NO_BUSY_POLL),
	NAPIF_STATE_IN_BUSY_POLL	= BIT(NAPI_STATE_IN_BUSY_POLL),
	NAPIF_STATE_PREFER_BUSY_POLL	= BIT(NAPI_STATE_PREFER_BUSY_POLL),
	NAPIF_STATE_THREADED		= BIT(NAPI_STATE_THREADED),
	NAPIF_STATE_SCHED_THREADED	= BIT(NAPI_STATE_SCHED_THREADED),
	NAPIF_STATE_HAS_NOTIFIER	= BIT(NAPI_STATE_HAS_NOTIFIER),
};

enum gro_result {
	GRO_MERGED,
	GRO_MERGED_FREE,
	GRO_HELD,
	GRO_NORMAL,
	GRO_CONSUMED,
};
typedef enum gro_result gro_result_t;

/*
 * enum rx_handler_result - Possible return values for rx_handlers.
 * @RX_HANDLER_CONSUMED: skb was consumed by rx_handler, do not process it
 * further.
 * @RX_HANDLER_ANOTHER: Do another round in receive path. This is indicated in
 * case skb->dev was changed by rx_handler.
 * @RX_HANDLER_EXACT: Force exact delivery, no wildcard.
 * @RX_HANDLER_PASS: Do nothing, pass the skb as if no rx_handler was called.
 *
 * rx_handlers are functions called from inside __netif_receive_skb(), to do
 * special processing of the skb, prior to delivery to protocol handlers.
 *
 * Currently, a net_device can only have a single rx_handler registered. Trying
 * to register a second rx_handler will return -EBUSY.
 *
 * To register a rx_handler on a net_device, use netdev_rx_handler_register().
 * To unregister a rx_handler on a net_device, use
 * netdev_rx_handler_unregister().
 *
 * Upon return, rx_handler is expected to tell __netif_receive_skb() what to
 * do with the skb.
 *
 * If the rx_handler consumed the skb in some way, it should return
 * RX_HANDLER_CONSUMED. This is appropriate when the rx_handler arranged for
 * the skb to be delivered in some other way.
 *
 * If the rx_handler changed skb->dev, to divert the skb to another
 * net_device, it should return RX_HANDLER_ANOTHER. The rx_handler for the
 * new device will be called if it exists.
 *
 * If the rx_handler decides the skb should be ignored, it should return
 * RX_HANDLER_EXACT. The skb will only be delivered to protocol handlers that
 * are registered on exact device (ptype->dev == skb->dev).
 *
 * If the rx_handler didn't change skb->dev, but wants the skb to be normally
 * delivered, it should return RX_HANDLER_PASS.
 *
 * A device without a registered rx_handler will behave as if rx_handler
 * returned RX_HANDLER_PASS.
 */

enum rx_handler_result {
	RX_HANDLER_CONSUMED,
	RX_HANDLER_ANOTHER,
	RX_HANDLER_EXACT,
	RX_HANDLER_PASS,
};
typedef enum rx_handler_result rx_handler_result_t;
typedef rx_handler_result_t rx_handler_func_t(struct sk_buff **pskb);

void __napi_schedule(struct napi_struct *n);
void __napi_schedule_irqoff(struct napi_struct *n);

static inline bool napi_disable_pending(struct napi_struct *n)
{
	return test_bit(NAPI_STATE_DISABLE, &n->state);
}

static inline bool napi_prefer_busy_poll(struct napi_struct *n)
{
	return test_bit(NAPI_STATE_PREFER_BUSY_POLL, &n->state);
}

/**
 * napi_is_scheduled - test if NAPI is scheduled
 * @n: NAPI context
 *
 * This check is "best-effort". With no locking implemented,
 * a NAPI can be scheduled or terminate right after this check
 * and produce not precise results.
 *
 * NAPI_STATE_SCHED is an internal state, napi_is_scheduled
 * should not be used normally and napi_schedule should be
 * used instead.
 *
 * Use only if the driver really needs to check if a NAPI
 * is scheduled for example in the context of delayed timer
 * that can be skipped if a NAPI is already scheduled.
 *
 * Return: True if NAPI is scheduled, False otherwise.
 */
static inline bool napi_is_scheduled(struct napi_struct *n)
{
	return test_bit(NAPI_STATE_SCHED, &n->state);
}

bool napi_schedule_prep(struct napi_struct *n);

/**
 *	napi_schedule - schedule NAPI poll
 *	@n: NAPI context
 *
 * Schedule NAPI poll routine to be called if it is not already
 * running.
 * Return: true if we schedule a NAPI or false if not.
 * Refer to napi_schedule_prep() for additional reason on why
 * a NAPI might not be scheduled.
 */
static inline bool napi_schedule(struct napi_struct *n)
{
	if (napi_schedule_prep(n)) {
		__napi_schedule(n);
		return true;
	}

	return false;
}

/**
 *	napi_schedule_irqoff - schedule NAPI poll
 *	@n: NAPI context
 *
 * Variant of napi_schedule(), assuming hard irqs are masked.
 */
static inline void napi_schedule_irqoff(struct napi_struct *n)
{
	if (napi_schedule_prep(n))
		__napi_schedule_irqoff(n);
}

/**
 * napi_complete_done - NAPI processing complete
 * @n: NAPI context
 * @work_done: number of packets processed
 *
 * Mark NAPI processing as complete. Should only be called if poll budget
 * has not been completely consumed.
 * Prefer over napi_complete().
 * Return: false if device should avoid rearming interrupts.
 */
bool napi_complete_done(struct napi_struct *n, int work_done);

static inline bool napi_complete(struct napi_struct *n)
{
	return napi_complete_done(n, 0);
}

void netif_threaded_enable(struct net_device *dev);
int dev_set_threaded(struct net_device *dev,
		     enum netdev_napi_threaded threaded);

void napi_disable(struct napi_struct *n);
void napi_disable_locked(struct napi_struct *n);

void napi_enable(struct napi_struct *n);
void napi_enable_locked(struct napi_struct *n);

/**
 *	napi_synchronize - wait until NAPI is not running
 *	@n: NAPI context
 *
 * Wait until NAPI is done being scheduled on this context.
 * Waits till any outstanding processing completes but
 * does not disable future activations.
 */
static inline void napi_synchronize(const struct napi_struct *n)
{
	if (IS_ENABLED(CONFIG_SMP))
		while (test_bit(NAPI_STATE_SCHED, &n->state))
			msleep(1);
	else
		barrier();
}

/**
 *	napi_if_scheduled_mark_missed - if napi is running, set the
 *	NAPIF_STATE_MISSED
 *	@n: NAPI context
 *
 * If napi is running, set the NAPIF_STATE_MISSED, and return true if
 * NAPI is scheduled.
 **/
static inline bool napi_if_scheduled_mark_missed(struct napi_struct *n)
{
	unsigned long val, new;

	val = READ_ONCE(n->state);
	do {
		if (val & NAPIF_STATE_DISABLE)
			return true;

		if (!(val & NAPIF_STATE_SCHED))
			return false;

		new = val | NAPIF_STATE_MISSED;
	} while (!try_cmpxchg(&n->state, &val, new));

	return true;
}

enum netdev_queue_state_t {
	__QUEUE_STATE_DRV_XOFF,
	__QUEUE_STATE_STACK_XOFF,
	__QUEUE_STATE_FROZEN,
};

#define QUEUE_STATE_DRV_XOFF	(1 << __QUEUE_STATE_DRV_XOFF)
#define QUEUE_STATE_STACK_XOFF	(1 << __QUEUE_STATE_STACK_XOFF)
#define QUEUE_STATE_FROZEN	(1 << __QUEUE_STATE_FROZEN)

#define QUEUE_STATE_ANY_XOFF	(QUEUE_STATE_DRV_XOFF | QUEUE_STATE_STACK_XOFF)
#define QUEUE_STATE_ANY_XOFF_OR_FROZEN (QUEUE_STATE_ANY_XOFF | \
					QUEUE_STATE_FROZEN)
#define QUEUE_STATE_DRV_XOFF_OR_FROZEN (QUEUE_STATE_DRV_XOFF | \
					QUEUE_STATE_FROZEN)

/*
 * __QUEUE_STATE_DRV_XOFF is used by drivers to stop the transmit queue.  The
 * netif_tx_* functions below are used to manipulate this flag.  The
 * __QUEUE_STATE_STACK_XOFF flag is used by the stack to stop the transmit
 * queue independently.  The netif_xmit_*stopped functions below are called
 * to check if the queue has been stopped by the driver or stack (either
 * of the XOFF bits are set in the state).  Drivers should not need to call
 * netif_xmit*stopped functions, they should only be using netif_tx_*.
 */

struct netdev_queue {
/*
 * read-mostly part
 */
	struct net_device	*dev;
	netdevice_tracker	dev_tracker;

	struct Qdisc __rcu	*qdisc;
	struct Qdisc __rcu	*qdisc_sleeping;
#ifdef CONFIG_SYSFS
	struct kobject		kobj;
	const struct attribute_group	**groups;
#endif
	unsigned long		tx_maxrate;
	/*
	 * Number of TX timeouts for this queue
	 * (/sys/class/net/DEV/Q/trans_timeout)
	 */
	atomic_long_t		trans_timeout;

	/* Subordinate device that the queue has been assigned to */
	struct net_device	*sb_dev;
#ifdef CONFIG_XDP_SOCKETS
	/* "ops protected", see comment about net_device::lock */
	struct xsk_buff_pool    *pool;
#endif

/*
 * write-mostly part
 */
#ifdef CONFIG_BQL
	struct dql		dql;
#endif
	spinlock_t		_xmit_lock ____cacheline_aligned_in_smp;
	int			xmit_lock_owner;
	/*
	 * Time (in jiffies) of last Tx
	 */
	unsigned long		trans_start;

	unsigned long		state;

/*
 * slow- / control-path part
 */
	/* NAPI instance for the queue
	 * "ops protected", see comment about net_device::lock
	 */
	struct napi_struct	*napi;

#if defined(CONFIG_XPS) && defined(CONFIG_NUMA)
	int			numa_node;
#endif
} ____cacheline_aligned_in_smp;

extern int sysctl_fb_tunnels_only_for_init_net;
extern int sysctl_devconf_inherit_init_net;

/*
 * sysctl_fb_tunnels_only_for_init_net == 0 : For all netns
 *                                     == 1 : For initns only
 *                                     == 2 : For none.
 */
static inline bool net_has_fallback_tunnels(const struct net *net)
{
#if IS_ENABLED(CONFIG_SYSCTL)
	int fb_tunnels_only_for_init_net = READ_ONCE(sysctl_fb_tunnels_only_for_init_net);

	return !fb_tunnels_only_for_init_net ||
		(net_eq(net, &init_net) && fb_tunnels_only_for_init_net == 1);
#else
	return true;
#endif
}

static inline int net_inherit_devconf(void)
{
#if IS_ENABLED(CONFIG_SYSCTL)
	return READ_ONCE(sysctl_devconf_inherit_init_net);
#else
	return 0;
#endif
}

static inline int netdev_queue_numa_node_read(const struct netdev_queue *q)
{
#if defined(CONFIG_XPS) && defined(CONFIG_NUMA)
	return q->numa_node;
#else
	return NUMA_NO_NODE;
#endif
}

static inline void netdev_queue_numa_node_write(struct netdev_queue *q, int node)
{
#if defined(CONFIG_XPS) && defined(CONFIG_NUMA)
	q->numa_node = node;
#endif
}

#ifdef CONFIG_RFS_ACCEL
bool rps_may_expire_flow(struct net_device *dev, u16 rxq_index, u32 flow_id,
			 u16 filter_id);
#endif

/* XPS map type and offset of the xps map within net_device->xps_maps[]. */
enum xps_map_type {
	XPS_CPUS = 0,
	XPS_RXQS,
	XPS_MAPS_MAX,
};

#ifdef CONFIG_XPS
/*
 * This structure holds an XPS map which can be of variable length.  The
 * map is an array of queues.
 */
struct xps_map {
	unsigned int len;
	unsigned int alloc_len;
	struct rcu_head rcu;
	u16 queues[];
};
#define XPS_MAP_SIZE(_num) (sizeof(struct xps_map) + ((_num) * sizeof(u16)))
#define XPS_MIN_MAP_ALLOC ((L1_CACHE_ALIGN(offsetof(struct xps_map, queues[1])) \
       - sizeof(struct xps_map)) / sizeof(u16))

/*
 * This structure holds all XPS maps for device.  Maps are indexed by CPU.
 *
 * We keep track of the number of cpus/rxqs used when the struct is allocated,
 * in nr_ids. This will help not accessing out-of-bound memory.
 *
 * We keep track of the number of traffic classes used when the struct is
 * allocated, in num_tc. This will be used to navigate the maps, to ensure we're
 * not crossing its upper bound, as the original dev->num_tc can be updated in
 * the meantime.
 */
struct xps_dev_maps {
	struct rcu_head rcu;
	unsigned int nr_ids;
	s16 num_tc;
	struct xps_map __rcu *attr_map[]; /* Either CPUs map or RXQs map */
};

#define XPS_CPU_DEV_MAPS_SIZE(_tcs) (sizeof(struct xps_dev_maps) +	\
	(nr_cpu_ids * (_tcs) * sizeof(struct xps_map *)))

#define XPS_RXQ_DEV_MAPS_SIZE(_tcs, _rxqs) (sizeof(struct xps_dev_maps) +\
	(_rxqs * (_tcs) * sizeof(struct xps_map *)))

#endif /* CONFIG_XPS */

#define TC_MAX_QUEUE	16
#define TC_BITMASK	15
/* HW offloaded queuing disciplines txq count and offset maps */
struct netdev_tc_txq {
	u16 count;
	u16 offset;
};

#if defined(CONFIG_FCOE) || defined(CONFIG_FCOE_MODULE)
/*
 * This structure is to hold information about the device
 * configured to run FCoE protocol stack.
 */
struct netdev_fcoe_hbainfo {
	char	manufacturer[64];
	char	serial_number[64];
	char	hardware_version[64];
	char	driver_version[64];
	char	optionrom_version[64];
	char	firmware_version[64];
	char	model[256];
	char	model_description[256];
};
#endif

#define MAX_PHYS_ITEM_ID_LEN 32

/* This structure holds a unique identifier to identify some
 * physical item (port for example) used by a netdevice.
 */
struct netdev_phys_item_id {
	unsigned char id[MAX_PHYS_ITEM_ID_LEN];
	unsigned char id_len;
};

static inline bool netdev_phys_item_id_same(struct netdev_phys_item_id *a,
					    struct netdev_phys_item_id *b)
{
	return a->id_len == b->id_len &&
	       memcmp(a->id, b->id, a->id_len) == 0;
}

typedef u16 (*select_queue_fallback_t)(struct net_device *dev,
				       struct sk_buff *skb,
				       struct net_device *sb_dev);

enum net_device_path_type {
	DEV_PATH_ETHERNET = 0,
	DEV_PATH_VLAN,
	DEV_PATH_BRIDGE,
	DEV_PATH_PPPOE,
	DEV_PATH_DSA,
	DEV_PATH_MTK_WDMA,
};

struct net_device_path {
	enum net_device_path_type	type;
	const struct net_device		*dev;
	union {
		struct {
			u16		id;
			__be16		proto;
			u8		h_dest[ETH_ALEN];
		} encap;
		struct {
			enum {
				DEV_PATH_BR_VLAN_KEEP,
				DEV_PATH_BR_VLAN_TAG,
				DEV_PATH_BR_VLAN_UNTAG,
				DEV_PATH_BR_VLAN_UNTAG_HW,
			}		vlan_mode;
			u16		vlan_id;
			__be16		vlan_proto;
		} bridge;
		struct {
			int port;
			u16 proto;
		} dsa;
		struct {
			u8 wdma_idx;
			u8 queue;
			u16 wcid;
			u8 bss;
			u8 amsdu;
		} mtk_wdma;
	};
};

#define NET_DEVICE_PATH_STACK_MAX	5
#define NET_DEVICE_PATH_VLAN_MAX	2

struct net_device_path_stack {
	int			num_paths;
	struct net_device_path	path[NET_DEVICE_PATH_STACK_MAX];
};

struct net_device_path_ctx {
	const struct net_device *dev;
	u8			daddr[ETH_ALEN];

	int			num_vlans;
	struct {
		u16		id;
		__be16		proto;
	} vlan[NET_DEVICE_PATH_VLAN_MAX];
};

enum tc_setup_type {
	TC_QUERY_CAPS,
	TC_SETUP_QDISC_MQPRIO,
	TC_SETUP_CLSU32,
	TC_SETUP_CLSFLOWER,
	TC_SETUP_CLSMATCHALL,
	TC_SETUP_CLSBPF,
	TC_SETUP_BLOCK,
	TC_SETUP_QDISC_CBS,
	TC_SETUP_QDISC_RED,
	TC_SETUP_QDISC_PRIO,
	TC_SETUP_QDISC_MQ,
	TC_SETUP_QDISC_ETF,
	TC_SETUP_ROOT_QDISC,
	TC_SETUP_QDISC_GRED,
	TC_SETUP_QDISC_TAPRIO,
	TC_SETUP_FT,
	TC_SETUP_QDISC_ETS,
	TC_SETUP_QDISC_TBF,
	TC_SETUP_QDISC_FIFO,
	TC_SETUP_QDISC_HTB,
	TC_SETUP_ACT,
};

/* These structures hold the attributes of bpf state that are being passed
 * to the netdevice through the bpf op.
 */
enum bpf_netdev_command {
	/* Set or clear a bpf program used in the earliest stages of packet
	 * rx. The prog will have been loaded as BPF_PROG_TYPE_XDP. The callee
	 * is responsible for calling bpf_prog_put on any old progs that are
	 * stored. In case of error, the callee need not release the new prog
	 * reference, but on success it takes ownership and must bpf_prog_put
	 * when it is no longer used.
	 */
	XDP_SETUP_PROG,
	XDP_SETUP_PROG_HW,
	/* BPF program for offload callbacks, invoked at program load time. */
	BPF_OFFLOAD_MAP_ALLOC,
	BPF_OFFLOAD_MAP_FREE,
	XDP_SETUP_XSK_POOL,
};

struct bpf_prog_offload_ops;
struct netlink_ext_ack;
struct xdp_umem;
struct xdp_dev_bulk_queue;
struct bpf_xdp_link;

enum bpf_xdp_mode {
	XDP_MODE_SKB = 0,
	XDP_MODE_DRV = 1,
	XDP_MODE_HW = 2,
	__MAX_XDP_MODE
};

struct bpf_xdp_entity {
	struct bpf_prog *prog;
	struct bpf_xdp_link *link;
};

struct netdev_bpf {
	enum bpf_netdev_command command;
	union {
		/* XDP_SETUP_PROG */
		struct {
			u32 flags;
			struct bpf_prog *prog;
			struct netlink_ext_ack *extack;
		};
		/* BPF_OFFLOAD_MAP_ALLOC, BPF_OFFLOAD_MAP_FREE */
		struct {
			struct bpf_offloaded_map *offmap;
		};
		/* XDP_SETUP_XSK_POOL */
		struct {
			struct xsk_buff_pool *pool;
			u16 queue_id;
		} xsk;
	};
};

/* Flags for ndo_xsk_wakeup. */
#define XDP_WAKEUP_RX (1 << 0)
#define XDP_WAKEUP_TX (1 << 1)

#ifdef CONFIG_XFRM_OFFLOAD
struct xfrmdev_ops {
	int	(*xdo_dev_state_add)(struct net_device *dev,
				     struct xfrm_state *x,
				     struct netlink_ext_ack *extack);
	void	(*xdo_dev_state_delete)(struct net_device *dev,
					struct xfrm_state *x);
	void	(*xdo_dev_state_free)(struct net_device *dev,
				      struct xfrm_state *x);
	bool	(*xdo_dev_offload_ok) (struct sk_buff *skb,
				       struct xfrm_state *x);
	void	(*xdo_dev_state_advance_esn) (struct xfrm_state *x);
	void	(*xdo_dev_state_update_stats) (struct xfrm_state *x);
	int	(*xdo_dev_policy_add) (struct xfrm_policy *x, struct netlink_ext_ack *extack);
	void	(*xdo_dev_policy_delete) (struct xfrm_policy *x);
	void	(*xdo_dev_policy_free) (struct xfrm_policy *x);
};
#endif

struct dev_ifalias {
	struct rcu_head rcuhead;
	char ifalias[];
};

struct devlink;
struct tlsdev_ops;

struct netdev_net_notifier {
	struct list_head list;
	struct notifier_block *nb;
};

/*
 * This structure defines the management hooks for network devices.
 * The following hooks can be defined; unless noted otherwise, they are
 * optional and can be filled with a null pointer.
 *
 * int (*ndo_init)(struct net_device *dev);
 *     This function is called once when a network device is registered.
 *     The network device can use this for any late stage initialization
 *     or semantic validation. It can fail with an error code which will
 *     be propagated back to register_netdev.
 *
 * void (*ndo_uninit)(struct net_device *dev);
 *     This function is called when device is unregistered or when registration
 *     fails. It is not called if init fails.
 *
 * int (*ndo_open)(struct net_device *dev);
 *     This function is called when a network device transitions to the up
 *     state.
 *
 * int (*ndo_stop)(struct net_device *dev);
 *     This function is called when a network device transitions to the down
 *     state.
 *
 * netdev_tx_t (*ndo_start_xmit)(struct sk_buff *skb,
 *                               struct net_device *dev);
 *	Called when a packet needs to be transmitted.
 *	Returns NETDEV_TX_OK.  Can return NETDEV_TX_BUSY, but you should stop
 *	the queue before that can happen; it's for obsolete devices and weird
 *	corner cases, but the stack really does a non-trivial amount
 *	of useless work if you return NETDEV_TX_BUSY.
 *	Required; cannot be NULL.
 *
 * netdev_features_t (*ndo_features_check)(struct sk_buff *skb,
 *					   struct net_device *dev
 *					   netdev_features_t features);
 *	Called by core transmit path to determine if device is capable of
 *	performing offload operations on a given packet. This is to give
 *	the device an opportunity to implement any restrictions that cannot
 *	be otherwise expressed by feature flags. The check is called with
 *	the set of features that the stack has calculated and it returns
 *	those the driver believes to be appropriate.
 *
 * u16 (*ndo_select_queue)(struct net_device *dev, struct sk_buff *skb,
 *                         struct net_device *sb_dev);
 *	Called to decide which queue to use when device supports multiple
 *	transmit queues.
 *
 * void (*ndo_change_rx_flags)(struct net_device *dev, int flags);
 *	This function is called to allow device receiver to make
 *	changes to configuration when multicast or promiscuous is enabled.
 *
 * void (*ndo_set_rx_mode)(struct net_device *dev);
 *	This function is called device changes address list filtering.
 *	If driver handles unicast address filtering, it should set
 *	IFF_UNICAST_FLT in its priv_flags.
 *
 * int (*ndo_set_mac_address)(struct net_device *dev, void *addr);
 *	This function  is called when the Media Access Control address
 *	needs to be changed. If this interface is not defined, the
 *	MAC address can not be changed.
 *
 * int (*ndo_validate_addr)(struct net_device *dev);
 *	Test if Media Access Control address is valid for the device.
 *
 * int (*ndo_do_ioctl)(struct net_device *dev, struct ifreq *ifr, int cmd);
 *	Old-style ioctl entry point. This is used internally by the
 *	ieee802154 subsystem but is no longer called by the device
 *	ioctl handler.
 *
 * int (*ndo_siocbond)(struct net_device *dev, struct ifreq *ifr, int cmd);
 *	Used by the bonding driver for its device specific ioctls:
 *	SIOCBONDENSLAVE, SIOCBONDRELEASE, SIOCBONDSETHWADDR, SIOCBONDCHANGEACTIVE,
 *	SIOCBONDSLAVEINFOQUERY, and SIOCBONDINFOQUERY
 *
 * * int (*ndo_eth_ioctl)(struct net_device *dev, struct ifreq *ifr, int cmd);
 *	Called for ethernet specific ioctls: SIOCGMIIPHY, SIOCGMIIREG,
 *	SIOCSMIIREG, SIOCSHWTSTAMP and SIOCGHWTSTAMP.
 *
 * int (*ndo_set_config)(struct net_device *dev, struct ifmap *map);
 *	Used to set network devices bus interface parameters. This interface
 *	is retained for legacy reasons; new devices should use the bus
 *	interface (PCI) for low level management.
 *
 * int (*ndo_change_mtu)(struct net_device *dev, int new_mtu);
 *	Called when a user wants to change the Maximum Transfer Unit
 *	of a device.
 *
 * void (*ndo_tx_timeout)(struct net_device *dev, unsigned int txqueue);
 *	Callback used when the transmitter has not made any progress
 *	for dev->watchdog ticks.
 *
 * void (*ndo_get_stats64)(struct net_device *dev,
 *                         struct rtnl_link_stats64 *storage);
 * struct net_device_stats* (*ndo_get_stats)(struct net_device *dev);
 *	Called when a user wants to get the network device usage
 *	statistics. Drivers must do one of the following:
 *	1. Define @ndo_get_stats64 to fill in a zero-initialised
 *	   rtnl_link_stats64 structure passed by the caller.
 *	2. Define @ndo_get_stats to update a net_device_stats structure
 *	   (which should normally be dev->stats) and return a pointer to
 *	   it. The structure may be changed asynchronously only if each
 *	   field is written atomically.
 *	3. Update dev->stats asynchronously and atomically, and define
 *	   neither operation.
 *
 * bool (*ndo_has_offload_stats)(const struct net_device *dev, int attr_id)
 *	Return true if this device supports offload stats of this attr_id.
 *
 * int (*ndo_get_offload_stats)(int attr_id, const struct net_device *dev,
 *	void *attr_data)
 *	Get statistics for offload operations by attr_id. Write it into the
 *	attr_data pointer.
 *
 * int (*ndo_vlan_rx_add_vid)(struct net_device *dev, __be16 proto, u16 vid);
 *	If device supports VLAN filtering this function is called when a
 *	VLAN id is registered.
 *
 * int (*ndo_vlan_rx_kill_vid)(struct net_device *dev, __be16 proto, u16 vid);
 *	If device supports VLAN filtering this function is called when a
 *	VLAN id is unregistered.
 *
 * void (*ndo_poll_controller)(struct net_device *dev);
 *
 *	SR-IOV management functions.
 * int (*ndo_set_vf_mac)(struct net_device *dev, int vf, u8* mac);
 * int (*ndo_set_vf_vlan)(struct net_device *dev, int vf, u16 vlan,
 *			  u8 qos, __be16 proto);
 * int (*ndo_set_vf_rate)(struct net_device *dev, int vf, int min_tx_rate,
 *			  int max_tx_rate);
 * int (*ndo_set_vf_spoofchk)(struct net_device *dev, int vf, bool setting);
 * int (*ndo_set_vf_trust)(struct net_device *dev, int vf, bool setting);
 * int (*ndo_get_vf_config)(struct net_device *dev,
 *			    int vf, struct ifla_vf_info *ivf);
 * int (*ndo_set_vf_link_state)(struct net_device *dev, int vf, int link_state);
 * int (*ndo_set_vf_port)(struct net_device *dev, int vf,
 *			  struct nlattr *port[]);
 *
 *      Enable or disable the VF ability to query its RSS Redirection Table and
 *      Hash Key. This is needed since on some devices VF share this information
 *      with PF and querying it may introduce a theoretical security risk.
 * int (*ndo_set_vf_rss_query_en)(struct net_device *dev, int vf, bool setting);
 * int (*ndo_get_vf_port)(struct net_device *dev, int vf, struct sk_buff *skb);
 * int (*ndo_setup_tc)(struct net_device *dev, enum tc_setup_type type,
 *		       void *type_data);
 *	Called to setup any 'tc' scheduler, classifier or action on @dev.
 *	This is always called from the stack with the rtnl lock held and netif
 *	tx queues stopped. This allows the netdevice to perform queue
 *	management safely.
 *
 *	Fiber Channel over Ethernet (FCoE) offload functions.
 * int (*ndo_fcoe_enable)(struct net_device *dev);
 *	Called when the FCoE protocol stack wants to start using LLD for FCoE
 *	so the underlying device can perform whatever needed configuration or
 *	initialization to support acceleration of FCoE traffic.
 *
 * int (*ndo_fcoe_disable)(struct net_device *dev);
 *	Called when the FCoE protocol stack wants to stop using LLD for FCoE
 *	so the underlying device can perform whatever needed clean-ups to
 *	stop supporting acceleration of FCoE traffic.
 *
 * int (*ndo_fcoe_ddp_setup)(struct net_device *dev, u16 xid,
 *			     struct scatterlist *sgl, unsigned int sgc);
 *	Called when the FCoE Initiator wants to initialize an I/O that
 *	is a possible candidate for Direct Data Placement (DDP). The LLD can
 *	perform necessary setup and returns 1 to indicate the device is set up
 *	successfully to perform DDP on this I/O, otherwise this returns 0.
 *
 * int (*ndo_fcoe_ddp_done)(struct net_device *dev,  u16 xid);
 *	Called when the FCoE Initiator/Target is done with the DDPed I/O as
 *	indicated by the FC exchange id 'xid', so the underlying device can
 *	clean up and reuse resources for later DDP requests.
 *
 * int (*ndo_fcoe_ddp_target)(struct net_device *dev, u16 xid,
 *			      struct scatterlist *sgl, unsigned int sgc);
 *	Called when the FCoE Target wants to initialize an I/O that
 *	is a possible candidate for Direct Data Placement (DDP). The LLD can
 *	perform necessary setup and returns 1 to indicate the device is set up
 *	successfully to perform DDP on this I/O, otherwise this returns 0.
 *
 * int (*ndo_fcoe_get_hbainfo)(struct net_device *dev,
 *			       struct netdev_fcoe_hbainfo *hbainfo);
 *	Called when the FCoE Protocol stack wants information on the underlying
 *	device. This information is utilized by the FCoE protocol stack to
 *	register attributes with Fiber Channel management service as per the
 *	FC-GS Fabric Device Management Information(FDMI) specification.
 *
 * int (*ndo_fcoe_get_wwn)(struct net_device *dev, u64 *wwn, int type);
 *	Called when the underlying device wants to override default World Wide
 *	Name (WWN) generation mechanism in FCoE protocol stack to pass its own
 *	World Wide Port Name (WWPN) or World Wide Node Name (WWNN) to the FCoE
 *	protocol stack to use.
 *
 *	RFS acceleration.
 * int (*ndo_rx_flow_steer)(struct net_device *dev, const struct sk_buff *skb,
 *			    u16 rxq_index, u32 flow_id);
 *	Set hardware filter for RFS.  rxq_index is the target queue index;
 *	flow_id is a flow ID to be passed to rps_may_expire_flow() later.
 *	Return the filter ID on success, or a negative error code.
 *
 *	Slave management functions (for bridge, bonding, etc).
 * int (*ndo_add_slave)(struct net_device *dev, struct net_device *slave_dev);
 *	Called to make another netdev an underling.
 *
 * int (*ndo_del_slave)(struct net_device *dev, struct net_device *slave_dev);
 *	Called to release previously enslaved netdev.
 *
 * struct net_device *(*ndo_get_xmit_slave)(struct net_device *dev,
 *					    struct sk_buff *skb,
 *					    bool all_slaves);
 *	Get the xmit slave of master device. If all_slaves is true, function
 *	assume all the slaves can transmit.
 *
 *      Feature/offload setting functions.
 * netdev_features_t (*ndo_fix_features)(struct net_device *dev,
 *		netdev_features_t features);
 *	Adjusts the requested feature flags according to device-specific
 *	constraints, and returns the resulting flags. Must not modify
 *	the device state.
 *
 * int (*ndo_set_features)(struct net_device *dev, netdev_features_t features);
 *	Called to update device configuration to new features. Passed
 *	feature set might be less than what was returned by ndo_fix_features()).
 *	Must return >0 or -errno if it changed dev->features itself.
 *
 * int (*ndo_fdb_add)(struct ndmsg *ndm, struct nlattr *tb[],
 *		      struct net_device *dev,
 *		      const unsigned char *addr, u16 vid, u16 flags,
 *		      bool *notified, struct netlink_ext_ack *extack);
 *	Adds an FDB entry to dev for addr.
 *	Callee shall set *notified to true if it sent any appropriate
 *	notification(s). Otherwise core will send a generic one.
 * int (*ndo_fdb_del)(struct ndmsg *ndm, struct nlattr *tb[],
 *		      struct net_device *dev,
 *		      const unsigned char *addr, u16 vid
 *		      bool *notified, struct netlink_ext_ack *extack);
 *	Deletes the FDB entry from dev corresponding to addr.
 *	Callee shall set *notified to true if it sent any appropriate
 *	notification(s). Otherwise core will send a generic one.
 * int (*ndo_fdb_del_bulk)(struct nlmsghdr *nlh, struct net_device *dev,
 *			   struct netlink_ext_ack *extack);
 * int (*ndo_fdb_dump)(struct sk_buff *skb, struct netlink_callback *cb,
 *		       struct net_device *dev, struct net_device *filter_dev,
 *		       int *idx)
 *	Used to add FDB entries to dump requests. Implementers should add
 *	entries to skb and update idx with the number of entries.
 *
 * int (*ndo_mdb_add)(struct net_device *dev, struct nlattr *tb[],
 *		      u16 nlmsg_flags, struct netlink_ext_ack *extack);
 *	Adds an MDB entry to dev.
 * int (*ndo_mdb_del)(struct net_device *dev, struct nlattr *tb[],
 *		      struct netlink_ext_ack *extack);
 *	Deletes the MDB entry from dev.
 * int (*ndo_mdb_del_bulk)(struct net_device *dev, struct nlattr *tb[],
 *			   struct netlink_ext_ack *extack);
 *	Bulk deletes MDB entries from dev.
 * int (*ndo_mdb_dump)(struct net_device *dev, struct sk_buff *skb,
 *		       struct netlink_callback *cb);
 *	Dumps MDB entries from dev. The first argument (marker) in the netlink
 *	callback is used by core rtnetlink code.
 *
 * int (*ndo_bridge_setlink)(struct net_device *dev, struct nlmsghdr *nlh,
 *			     u16 flags, struct netlink_ext_ack *extack)
 * int (*ndo_bridge_getlink)(struct sk_buff *skb, u32 pid, u32 seq,
 *			     struct net_device *dev, u32 filter_mask,
 *			     int nlflags)
 * int (*ndo_bridge_dellink)(struct net_device *dev, struct nlmsghdr *nlh,
 *			     u16 flags);
 *
 * int (*ndo_change_carrier)(struct net_device *dev, bool new_carrier);
 *	Called to change device carrier. Soft-devices (like dummy, team, etc)
 *	which do not represent real hardware may define this to allow their
 *	userspace components to manage their virtual carrier state. Devices
 *	that determine carrier state from physical hardware properties (eg
 *	network cables) or protocol-dependent mechanisms (eg
 *	USB_CDC_NOTIFY_NETWORK_CONNECTION) should NOT implement this function.
 *
 * int (*ndo_get_phys_port_id)(struct net_device *dev,
 *			       struct netdev_phys_item_id *ppid);
 *	Called to get ID of physical port of this device. If driver does
 *	not implement this, it is assumed that the hw is not able to have
 *	multiple net devices on single physical port.
 *
 * int (*ndo_get_port_parent_id)(struct net_device *dev,
 *				 struct netdev_phys_item_id *ppid)
 *	Called to get the parent ID of the physical port of this device.
 *
 * void* (*ndo_dfwd_add_station)(struct net_device *pdev,
 *				 struct net_device *dev)
 *	Called by upper layer devices to accelerate switching or other
 *	station functionality into hardware. 'pdev is the lowerdev
 *	to use for the offload and 'dev' is the net device that will
 *	back the offload. Returns a pointer to the private structure
 *	the upper layer will maintain.
 * void (*ndo_dfwd_del_station)(struct net_device *pdev, void *priv)
 *	Called by upper layer device to delete the station created
 *	by 'ndo_dfwd_add_station'. 'pdev' is the net device backing
 *	the station and priv is the structure returned by the add
 *	operation.
 * int (*ndo_set_tx_maxrate)(struct net_device *dev,
 *			     int queue_index, u32 maxrate);
 *	Called when a user wants to set a max-rate limitation of specific
 *	TX queue.
 * int (*ndo_get_iflink)(const struct net_device *dev);
 *	Called to get the iflink value of this device.
 * int (*ndo_fill_metadata_dst)(struct net_device *dev, struct sk_buff *skb);
 *	This function is used to get egress tunnel information for given skb.
 *	This is useful for retrieving outer tunnel header parameters while
 *	sampling packet.
 * void (*ndo_set_rx_headroom)(struct net_device *dev, int needed_headroom);
 *	This function is used to specify the headroom that the skb must
 *	consider when allocation skb during packet reception. Setting
 *	appropriate rx headroom value allows avoiding skb head copy on
 *	forward. Setting a negative value resets the rx headroom to the
 *	default value.
 * int (*ndo_bpf)(struct net_device *dev, struct netdev_bpf *bpf);
 *	This function is used to set or query state related to XDP on the
 *	netdevice and manage BPF offload. See definition of
 *	enum bpf_netdev_command for details.
 * int (*ndo_xdp_xmit)(struct net_device *dev, int n, struct xdp_frame **xdp,
 *			u32 flags);
 *	This function is used to submit @n XDP packets for transmit on a
 *	netdevice. Returns number of frames successfully transmitted, frames
 *	that got dropped are freed/returned via xdp_return_frame().
 *	Returns negative number, means general error invoking ndo, meaning
 *	no frames were xmit'ed and core-caller will free all frames.
 * struct net_device *(*ndo_xdp_get_xmit_slave)(struct net_device *dev,
 *					        struct xdp_buff *xdp);
 *      Get the xmit slave of master device based on the xdp_buff.
 * int (*ndo_xsk_wakeup)(struct net_device *dev, u32 queue_id, u32 flags);
 *      This function is used to wake up the softirq, ksoftirqd or kthread
 *	responsible for sending and/or receiving packets on a specific
 *	queue id bound to an AF_XDP socket. The flags field specifies if
 *	only RX, only Tx, or both should be woken up using the flags
 *	XDP_WAKEUP_RX and XDP_WAKEUP_TX.
 * int (*ndo_tunnel_ctl)(struct net_device *dev, struct ip_tunnel_parm_kern *p,
 *			 int cmd);
 *	Add, change, delete or get information on an IPv4 tunnel.
 * struct net_device *(*ndo_get_peer_dev)(struct net_device *dev);
 *	If a device is paired with a peer device, return the peer instance.
 *	The caller must be under RCU read context.
 * int (*ndo_fill_forward_path)(struct net_device_path_ctx *ctx, struct net_device_path *path);
 *     Get the forwarding path to reach the real device from the HW destination address
 * ktime_t (*ndo_get_tstamp)(struct net_device *dev,
 *			     const struct skb_shared_hwtstamps *hwtstamps,
 *			     bool cycles);
 *	Get hardware timestamp based on normal/adjustable time or free running
 *	cycle counter. This function is required if physical clock supports a
 *	free running cycle counter.
 *
 * int (*ndo_hwtstamp_get)(struct net_device *dev,
 *			   struct kernel_hwtstamp_config *kernel_config);
 *	Get the currently configured hardware timestamping parameters for the
 *	NIC device.
 *
 * int (*ndo_hwtstamp_set)(struct net_device *dev,
 *			   struct kernel_hwtstamp_config *kernel_config,
 *			   struct netlink_ext_ack *extack);
 *	Change the hardware timestamping parameters for NIC device.
 */
struct net_device_ops {
	int			(*ndo_init)(struct net_device *dev);
	void			(*ndo_uninit)(struct net_device *dev);
	int			(*ndo_open)(struct net_device *dev);
	int			(*ndo_stop)(struct net_device *dev);
	netdev_tx_t		(*ndo_start_xmit)(struct sk_buff *skb,
						  struct net_device *dev);
	netdev_features_t	(*ndo_features_check)(struct sk_buff *skb,
						      struct net_device *dev,
						      netdev_features_t features);
	u16			(*ndo_select_queue)(struct net_device *dev,
						    struct sk_buff *skb,
						    struct net_device *sb_dev);
	void			(*ndo_change_rx_flags)(struct net_device *dev,
						       int flags);
	void			(*ndo_set_rx_mode)(struct net_device *dev);
	int			(*ndo_set_mac_address)(struct net_device *dev,
						       void *addr);
	int			(*ndo_validate_addr)(struct net_device *dev);
	int			(*ndo_do_ioctl)(struct net_device *dev,
					        struct ifreq *ifr, int cmd);
	int			(*ndo_eth_ioctl)(struct net_device *dev,
						 struct ifreq *ifr, int cmd);
	int			(*ndo_siocbond)(struct net_device *dev,
						struct ifreq *ifr, int cmd);
	int			(*ndo_siocwandev)(struct net_device *dev,
						  struct if_settings *ifs);
	int			(*ndo_siocdevprivate)(struct net_device *dev,
						      struct ifreq *ifr,
						      void __user *data, int cmd);
	int			(*ndo_set_config)(struct net_device *dev,
					          struct ifmap *map);
	int			(*ndo_change_mtu)(struct net_device *dev,
						  int new_mtu);
	int			(*ndo_neigh_setup)(struct net_device *dev,
						   struct neigh_parms *);
	void			(*ndo_tx_timeout) (struct net_device *dev,
						   unsigned int txqueue);

	void			(*ndo_get_stats64)(struct net_device *dev,
						   struct rtnl_link_stats64 *storage);
	bool			(*ndo_has_offload_stats)(const struct net_device *dev, int attr_id);
	int			(*ndo_get_offload_stats)(int attr_id,
							 const struct net_device *dev,
							 void *attr_data);
	struct net_device_stats* (*ndo_get_stats)(struct net_device *dev);

	int			(*ndo_vlan_rx_add_vid)(struct net_device *dev,
						       __be16 proto, u16 vid);
	int			(*ndo_vlan_rx_kill_vid)(struct net_device *dev,
						        __be16 proto, u16 vid);
#ifdef CONFIG_NET_POLL_CONTROLLER
	void                    (*ndo_poll_controller)(struct net_device *dev);
	int			(*ndo_netpoll_setup)(struct net_device *dev);
	void			(*ndo_netpoll_cleanup)(struct net_device *dev);
#endif
	int			(*ndo_set_vf_mac)(struct net_device *dev,
						  int queue, u8 *mac);
	int			(*ndo_set_vf_vlan)(struct net_device *dev,
						   int queue, u16 vlan,
						   u8 qos, __be16 proto);
	int			(*ndo_set_vf_rate)(struct net_device *dev,
						   int vf, int min_tx_rate,
						   int max_tx_rate);
	int			(*ndo_set_vf_spoofchk)(struct net_device *dev,
						       int vf, bool setting);
	int			(*ndo_set_vf_trust)(struct net_device *dev,
						    int vf, bool setting);
	int			(*ndo_get_vf_config)(struct net_device *dev,
						     int vf,
						     struct ifla_vf_info *ivf);
	int			(*ndo_set_vf_link_state)(struct net_device *dev,
							 int vf, int link_state);
	int			(*ndo_get_vf_stats)(struct net_device *dev,
						    int vf,
						    struct ifla_vf_stats
						    *vf_stats);
	int			(*ndo_set_vf_port)(struct net_device *dev,
						   int vf,
						   struct nlattr *port[]);
	int			(*ndo_get_vf_port)(struct net_device *dev,
						   int vf, struct sk_buff *skb);
	int			(*ndo_get_vf_guid)(struct net_device *dev,
						   int vf,
						   struct ifla_vf_guid *node_guid,
						   struct ifla_vf_guid *port_guid);
	int			(*ndo_set_vf_guid)(struct net_device *dev,
						   int vf, u64 guid,
						   int guid_type);
	int			(*ndo_set_vf_rss_query_en)(
						   struct net_device *dev,
						   int vf, bool setting);
	int			(*ndo_setup_tc)(struct net_device *dev,
						enum tc_setup_type type,
						void *type_data);
#if IS_ENABLED(CONFIG_FCOE)
	int			(*ndo_fcoe_enable)(struct net_device *dev);
	int			(*ndo_fcoe_disable)(struct net_device *dev);
	int			(*ndo_fcoe_ddp_setup)(struct net_device *dev,
						      u16 xid,
						      struct scatterlist *sgl,
						      unsigned int sgc);
	int			(*ndo_fcoe_ddp_done)(struct net_device *dev,
						     u16 xid);
	int			(*ndo_fcoe_ddp_target)(struct net_device *dev,
						       u16 xid,
						       struct scatterlist *sgl,
						       unsigned int sgc);
	int			(*ndo_fcoe_get_hbainfo)(struct net_device *dev,
							struct netdev_fcoe_hbainfo *hbainfo);
#endif

#if IS_ENABLED(CONFIG_LIBFCOE)
#define NETDEV_FCOE_WWNN 0
#define NETDEV_FCOE_WWPN 1
	int			(*ndo_fcoe_get_wwn)(struct net_device *dev,
						    u64 *wwn, int type);
#endif

#ifdef CONFIG_RFS_ACCEL
	int			(*ndo_rx_flow_steer)(struct net_device *dev,
						     const struct sk_buff *skb,
						     u16 rxq_index,
						     u32 flow_id);
#endif
	int			(*ndo_add_slave)(struct net_device *dev,
						 struct net_device *slave_dev,
						 struct netlink_ext_ack *extack);
	int			(*ndo_del_slave)(struct net_device *dev,
						 struct net_device *slave_dev);
	struct net_device*	(*ndo_get_xmit_slave)(struct net_device *dev,
						      struct sk_buff *skb,
						      bool all_slaves);
	struct net_device*	(*ndo_sk_get_lower_dev)(struct net_device *dev,
							struct sock *sk);
	netdev_features_t	(*ndo_fix_features)(struct net_device *dev,
						    netdev_features_t features);
	int			(*ndo_set_features)(struct net_device *dev,
						    netdev_features_t features);
	int			(*ndo_neigh_construct)(struct net_device *dev,
						       struct neighbour *n);
	void			(*ndo_neigh_destroy)(struct net_device *dev,
						     struct neighbour *n);

	int			(*ndo_fdb_add)(struct ndmsg *ndm,
					       struct nlattr *tb[],
					       struct net_device *dev,
					       const unsigned char *addr,
					       u16 vid,
					       u16 flags,
					       bool *notified,
					       struct netlink_ext_ack *extack);
	int			(*ndo_fdb_del)(struct ndmsg *ndm,
					       struct nlattr *tb[],
					       struct net_device *dev,
					       const unsigned char *addr,
					       u16 vid,
					       bool *notified,
					       struct netlink_ext_ack *extack);
	int			(*ndo_fdb_del_bulk)(struct nlmsghdr *nlh,
						    struct net_device *dev,
						    struct netlink_ext_ack *extack);
	int			(*ndo_fdb_dump)(struct sk_buff *skb,
						struct netlink_callback *cb,
						struct net_device *dev,
						struct net_device *filter_dev,
						int *idx);
	int			(*ndo_fdb_get)(struct sk_buff *skb,
					       struct nlattr *tb[],
					       struct net_device *dev,
					       const unsigned char *addr,
					       u16 vid, u32 portid, u32 seq,
					       struct netlink_ext_ack *extack);
	int			(*ndo_mdb_add)(struct net_device *dev,
					       struct nlattr *tb[],
					       u16 nlmsg_flags,
					       struct netlink_ext_ack *extack);
	int			(*ndo_mdb_del)(struct net_device *dev,
					       struct nlattr *tb[],
					       struct netlink_ext_ack *extack);
	int			(*ndo_mdb_del_bulk)(struct net_device *dev,
						    struct nlattr *tb[],
						    struct netlink_ext_ack *extack);
	int			(*ndo_mdb_dump)(struct net_device *dev,
						struct sk_buff *skb,
						struct netlink_callback *cb);
	int			(*ndo_mdb_get)(struct net_device *dev,
					       struct nlattr *tb[], u32 portid,
					       u32 seq,
					       struct netlink_ext_ack *extack);
	int			(*ndo_bridge_setlink)(struct net_device *dev,
						      struct nlmsghdr *nlh,
						      u16 flags,
						      struct netlink_ext_ack *extack);
	int			(*ndo_bridge_getlink)(struct sk_buff *skb,
						      u32 pid, u32 seq,
						      struct net_device *dev,
						      u32 filter_mask,
						      int nlflags);
	int			(*ndo_bridge_dellink)(struct net_device *dev,
						      struct nlmsghdr *nlh,
						      u16 flags);
	int			(*ndo_change_carrier)(struct net_device *dev,
						      bool new_carrier);
	int			(*ndo_get_phys_port_id)(struct net_device *dev,
							struct netdev_phys_item_id *ppid);
	int			(*ndo_get_port_parent_id)(struct net_device *dev,
							  struct netdev_phys_item_id *ppid);
	int			(*ndo_get_phys_port_name)(struct net_device *dev,
							  char *name, size_t len);
	void*			(*ndo_dfwd_add_station)(struct net_device *pdev,
							struct net_device *dev);
	void			(*ndo_dfwd_del_station)(struct net_device *pdev,
							void *priv);

	int			(*ndo_set_tx_maxrate)(struct net_device *dev,
						      int queue_index,
						      u32 maxrate);
	int			(*ndo_get_iflink)(const struct net_device *dev);
	int			(*ndo_fill_metadata_dst)(struct net_device *dev,
						       struct sk_buff *skb);
	void			(*ndo_set_rx_headroom)(struct net_device *dev,
						       int needed_headroom);
	int			(*ndo_bpf)(struct net_device *dev,
					   struct netdev_bpf *bpf);
	int			(*ndo_xdp_xmit)(struct net_device *dev, int n,
						struct xdp_frame **xdp,
						u32 flags);
	struct net_device *	(*ndo_xdp_get_xmit_slave)(struct net_device *dev,
							  struct xdp_buff *xdp);
	int			(*ndo_xsk_wakeup)(struct net_device *dev,
						  u32 queue_id, u32 flags);
	int			(*ndo_tunnel_ctl)(struct net_device *dev,
						  struct ip_tunnel_parm_kern *p,
						  int cmd);
	struct net_device *	(*ndo_get_peer_dev)(struct net_device *dev);
	int                     (*ndo_fill_forward_path)(struct net_device_path_ctx *ctx,
                                                         struct net_device_path *path);
	ktime_t			(*ndo_get_tstamp)(struct net_device *dev,
						  const struct skb_shared_hwtstamps *hwtstamps,
						  bool cycles);
	int			(*ndo_hwtstamp_get)(struct net_device *dev,
						    struct kernel_hwtstamp_config *kernel_config);
	int			(*ndo_hwtstamp_set)(struct net_device *dev,
						    struct kernel_hwtstamp_config *kernel_config,
						    struct netlink_ext_ack *extack);

#if IS_ENABLED(CONFIG_NET_SHAPER)
	/**
	 * @net_shaper_ops: Device shaping offload operations
	 * see include/net/net_shapers.h
	 */
	const struct net_shaper_ops *net_shaper_ops;
#endif
};

/**
 * enum netdev_priv_flags - &struct net_device priv_flags
 *
 * These are the &struct net_device, they are only set internally
 * by drivers and used in the kernel. These flags are invisible to
 * userspace; this means that the order of these flags can change
 * during any kernel release.
 *
 * You should add bitfield booleans after either net_device::priv_flags
 * (hotpath) or ::threaded (slowpath) instead of extending these flags.
 *
 * @IFF_802_1Q_VLAN: 802.1Q VLAN device
 * @IFF_EBRIDGE: Ethernet bridging device
 * @IFF_BONDING: bonding master or slave
 * @IFF_ISATAP: ISATAP interface (RFC4214)
 * @IFF_WAN_HDLC: WAN HDLC device
 * @IFF_XMIT_DST_RELEASE: dev_hard_start_xmit() is allowed to
 *	release skb->dst
 * @IFF_DONT_BRIDGE: disallow bridging this ether dev
 * @IFF_DISABLE_NETPOLL: disable netpoll at run-time
 * @IFF_MACVLAN_PORT: device used as macvlan port
 * @IFF_BRIDGE_PORT: device used as bridge port
 * @IFF_OVS_DATAPATH: device used as Open vSwitch datapath port
 * @IFF_TX_SKB_SHARING: The interface supports sharing skbs on transmit
 * @IFF_UNICAST_FLT: Supports unicast filtering
 * @IFF_TEAM_PORT: device used as team port
 * @IFF_SUPP_NOFCS: device supports sending custom FCS
 * @IFF_LIVE_ADDR_CHANGE: device supports hardware address
 *	change when it's running
 * @IFF_MACVLAN: Macvlan device
 * @IFF_XMIT_DST_RELEASE_PERM: IFF_XMIT_DST_RELEASE not taking into account
 *	underlying stacked devices
 * @IFF_L3MDEV_MASTER: device is an L3 master device
 * @IFF_NO_QUEUE: device can run without qdisc attached
 * @IFF_OPENVSWITCH: device is a Open vSwitch master
 * @IFF_L3MDEV_SLAVE: device is enslaved to an L3 master device
 * @IFF_TEAM: device is a team device
 * @IFF_RXFH_CONFIGURED: device has had Rx Flow indirection table configured
 * @IFF_PHONY_HEADROOM: the headroom value is controlled by an external
 *	entity (i.e. the master device for bridged veth)
 * @IFF_MACSEC: device is a MACsec device
 * @IFF_NO_RX_HANDLER: device doesn't support the rx_handler hook
 * @IFF_FAILOVER: device is a failover master device
 * @IFF_FAILOVER_SLAVE: device is lower dev of a failover master device
 * @IFF_L3MDEV_RX_HANDLER: only invoke the rx handler of L3 master device
 * @IFF_NO_ADDRCONF: prevent ipv6 addrconf
 * @IFF_TX_SKB_NO_LINEAR: device/driver is capable of xmitting frames with
 *	skb_headlen(skb) == 0 (data starts from frag0)
 */
enum netdev_priv_flags {
	IFF_802_1Q_VLAN			= 1<<0,
	IFF_EBRIDGE			= 1<<1,
	IFF_BONDING			= 1<<2,
	IFF_ISATAP			= 1<<3,
	IFF_WAN_HDLC			= 1<<4,
	IFF_XMIT_DST_RELEASE		= 1<<5,
	IFF_DONT_BRIDGE			= 1<<6,
	IFF_DISABLE_NETPOLL		= 1<<7,
	IFF_MACVLAN_PORT		= 1<<8,
	IFF_BRIDGE_PORT			= 1<<9,
	IFF_OVS_DATAPATH		= 1<<10,
	IFF_TX_SKB_SHARING		= 1<<11,
	IFF_UNICAST_FLT			= 1<<12,
	IFF_TEAM_PORT			= 1<<13,
	IFF_SUPP_NOFCS			= 1<<14,
	IFF_LIVE_ADDR_CHANGE		= 1<<15,
	IFF_MACVLAN			= 1<<16,
	IFF_XMIT_DST_RELEASE_PERM	= 1<<17,
	IFF_L3MDEV_MASTER		= 1<<18,
	IFF_NO_QUEUE			= 1<<19,
	IFF_OPENVSWITCH			= 1<<20,
	IFF_L3MDEV_SLAVE		= 1<<21,
	IFF_TEAM			= 1<<22,
	IFF_RXFH_CONFIGURED		= 1<<23,
	IFF_PHONY_HEADROOM		= 1<<24,
	IFF_MACSEC			= 1<<25,
	IFF_NO_RX_HANDLER		= 1<<26,
	IFF_FAILOVER			= 1<<27,
	IFF_FAILOVER_SLAVE		= 1<<28,
	IFF_L3MDEV_RX_HANDLER		= 1<<29,
	IFF_NO_ADDRCONF			= BIT_ULL(30),
	IFF_TX_SKB_NO_LINEAR		= BIT_ULL(31),
};

/* Specifies the type of the struct net_device::ml_priv pointer */
enum netdev_ml_priv_type {
	ML_PRIV_NONE,
	ML_PRIV_CAN,
};

enum netdev_stat_type {
	NETDEV_PCPU_STAT_NONE,
	NETDEV_PCPU_STAT_LSTATS, /* struct pcpu_lstats */
	NETDEV_PCPU_STAT_TSTATS, /* struct pcpu_sw_netstats */
	NETDEV_PCPU_STAT_DSTATS, /* struct pcpu_dstats */
};

enum netdev_reg_state {
	NETREG_UNINITIALIZED = 0,
	NETREG_REGISTERED,	/* completed register_netdevice */
	NETREG_UNREGISTERING,	/* called unregister_netdevice */
	NETREG_UNREGISTERED,	/* completed unregister todo */
	NETREG_RELEASED,	/* called free_netdev */
	NETREG_DUMMY,		/* dummy device for NAPI poll */
};

/**
 *	struct net_device - The DEVICE structure.
 *
 *	Actually, this whole structure is a big mistake.  It mixes I/O
 *	data with strictly "high-level" data, and it has to know about
 *	almost every data structure used in the INET module.
 *
 *	@priv_flags:	flags invisible to userspace defined as bits, see
 *			enum netdev_priv_flags for the definitions
 *	@lltx:		device supports lockless Tx. Deprecated for real HW
 *			drivers. Mainly used by logical interfaces, such as
 *			bonding and tunnels
 *	@netmem_tx:	device support netmem_tx.
 *
 *	@name:	This is the first field of the "visible" part of this structure
 *		(i.e. as seen by users in the "Space.c" file).  It is the name
 *		of the interface.
 *
 *	@name_node:	Name hashlist node
 *	@ifalias:	SNMP alias
 *	@mem_end:	Shared memory end
 *	@mem_start:	Shared memory start
 *	@base_addr:	Device I/O address
 *	@irq:		Device IRQ number
 *
 *	@state:		Generic network queuing layer state, see netdev_state_t
 *	@dev_list:	The global list of network devices
 *	@napi_list:	List entry used for polling NAPI devices
 *	@unreg_list:	List entry  when we are unregistering the
 *			device; see the function unregister_netdev
 *	@close_list:	List entry used when we are closing the device
 *	@ptype_all:     Device-specific packet handlers for all protocols
 *	@ptype_specific: Device-specific, protocol-specific packet handlers
 *
 *	@adj_list:	Directly linked devices, like slaves for bonding
 *	@features:	Currently active device features
 *	@hw_features:	User-changeable features
 *
 *	@wanted_features:	User-requested features
 *	@vlan_features:		Mask of features inheritable by VLAN devices
 *
 *	@hw_enc_features:	Mask of features inherited by encapsulating devices
 *				This field indicates what encapsulation
 *				offloads the hardware is capable of doing,
 *				and drivers will need to set them appropriately.
 *
 *	@mpls_features:	Mask of features inheritable by MPLS
 *	@gso_partial_features: value(s) from NETIF_F_GSO\*
 *
 *	@ifindex:	interface index
 *	@group:		The group the device belongs to
 *
 *	@stats:		Statistics struct, which was left as a legacy, use
 *			rtnl_link_stats64 instead
 *
 *	@core_stats:	core networking counters,
 *			do not use this in drivers
 *	@carrier_up_count:	Number of times the carrier has been up
 *	@carrier_down_count:	Number of times the carrier has been down
 *
 *	@wireless_handlers:	List of functions to handle Wireless Extensions,
 *				instead of ioctl,
 *				see <net/iw_handler.h> for details.
 *
 *	@netdev_ops:	Includes several pointers to callbacks,
 *			if one wants to override the ndo_*() functions
 *	@xdp_metadata_ops:	Includes pointers to XDP metadata callbacks.
 *	@xsk_tx_metadata_ops:	Includes pointers to AF_XDP TX metadata callbacks.
 *	@ethtool_ops:	Management operations
 *	@l3mdev_ops:	Layer 3 master device operations
 *	@ndisc_ops:	Includes callbacks for different IPv6 neighbour
 *			discovery handling. Necessary for e.g. 6LoWPAN.
 *	@xfrmdev_ops:	Transformation offload operations
 *	@tlsdev_ops:	Transport Layer Security offload operations
 *	@header_ops:	Includes callbacks for creating,parsing,caching,etc
 *			of Layer 2 headers.
 *
 *	@flags:		Interface flags (a la BSD)
 *	@xdp_features:	XDP capability supported by the device
 *	@gflags:	Global flags ( kept as legacy )
 *	@priv_len:	Size of the ->priv flexible array
 *	@priv:		Flexible array containing private data
 *	@operstate:	RFC2863 operstate
 *	@link_mode:	Mapping policy to operstate
 *	@if_port:	Selectable AUI, TP, ...
 *	@dma:		DMA channel
 *	@mtu:		Interface MTU value
 *	@min_mtu:	Interface Minimum MTU value
 *	@max_mtu:	Interface Maximum MTU value
 *	@type:		Interface hardware type
 *	@hard_header_len: Maximum hardware header length.
 *	@min_header_len:  Minimum hardware header length
 *
 *	@needed_headroom: Extra headroom the hardware may need, but not in all
 *			  cases can this be guaranteed
 *	@needed_tailroom: Extra tailroom the hardware may need, but not in all
 *			  cases can this be guaranteed. Some cases also use
 *			  LL_MAX_HEADER instead to allocate the skb
 *
 *	interface address info:
 *
 * 	@perm_addr:		Permanent hw address
 * 	@addr_assign_type:	Hw address assignment type
 * 	@addr_len:		Hardware address length
 *	@upper_level:		Maximum depth level of upper devices.
 *	@lower_level:		Maximum depth level of lower devices.
 *	@threaded:		napi threaded state.
 *	@neigh_priv_len:	Used in neigh_alloc()
 * 	@dev_id:		Used to differentiate devices that share
 * 				the same link layer address
 * 	@dev_port:		Used to differentiate devices that share
 * 				the same function
 *	@addr_list_lock:	XXX: need comments on this one
 *	@name_assign_type:	network interface name assignment type
 *	@uc_promisc:		Counter that indicates promiscuous mode
 *				has been enabled due to the need to listen to
 *				additional unicast addresses in a device that
 *				does not implement ndo_set_rx_mode()
 *	@uc:			unicast mac addresses
 *	@mc:			multicast mac addresses
 *	@dev_addrs:		list of device hw addresses
 *	@queues_kset:		Group of all Kobjects in the Tx and RX queues
 *	@promiscuity:		Number of times the NIC is told to work in
 *				promiscuous mode; if it becomes 0 the NIC will
 *				exit promiscuous mode
 *	@allmulti:		Counter, enables or disables allmulticast mode
 *
 *	@vlan_info:	VLAN info
 *	@dsa_ptr:	dsa specific data
 *	@tipc_ptr:	TIPC specific data
 *	@atalk_ptr:	AppleTalk link
 *	@ip_ptr:	IPv4 specific data
 *	@ip6_ptr:	IPv6 specific data
 *	@ax25_ptr:	AX.25 specific data
 *	@ieee80211_ptr:	IEEE 802.11 specific data, assign before registering
 *	@ieee802154_ptr: IEEE 802.15.4 low-rate Wireless Personal Area Network
 *			 device struct
 *	@mpls_ptr:	mpls_dev struct pointer
 *	@mctp_ptr:	MCTP specific data
 *
 *	@dev_addr:	Hw address (before bcast,
 *			because most packets are unicast)
 *
 *	@_rx:			Array of RX queues
 *	@num_rx_queues:		Number of RX queues
 *				allocated at register_netdev() time
 *	@real_num_rx_queues: 	Number of RX queues currently active in device
 *	@xdp_prog:		XDP sockets filter program pointer
 *
 *	@rx_handler:		handler for received packets
 *	@rx_handler_data: 	XXX: need comments on this one
 *	@tcx_ingress:		BPF & clsact qdisc specific data for ingress processing
 *	@ingress_queue:		XXX: need comments on this one
 *	@nf_hooks_ingress:	netfilter hooks executed for ingress packets
 *	@broadcast:		hw bcast address
 *
 *	@rx_cpu_rmap:	CPU reverse-mapping for RX completion interrupts,
 *			indexed by RX queue number. Assigned by driver.
 *			This must only be set if the ndo_rx_flow_steer
 *			operation is defined
 *	@index_hlist:		Device index hash chain
 *
 *	@_tx:			Array of TX queues
 *	@num_tx_queues:		Number of TX queues allocated at alloc_netdev_mq() time
 *	@real_num_tx_queues: 	Number of TX queues currently active in device
 *	@qdisc:			Root qdisc from userspace point of view
 *	@tx_queue_len:		Max frames per queue allowed
 *	@tx_global_lock: 	XXX: need comments on this one
 *	@xdp_bulkq:		XDP device bulk queue
 *	@xps_maps:		all CPUs/RXQs maps for XPS device
 *
 *	@xps_maps:	XXX: need comments on this one
 *	@tcx_egress:		BPF & clsact qdisc specific data for egress processing
 *	@nf_hooks_egress:	netfilter hooks executed for egress packets
 *	@qdisc_hash:		qdisc hash table
 *	@watchdog_timeo:	Represents the timeout that is used by
 *				the watchdog (see dev_watchdog())
 *	@watchdog_timer:	List of timers
 *
 *	@proto_down_reason:	reason a netdev interface is held down
 *	@pcpu_refcnt:		Number of references to this device
 *	@dev_refcnt:		Number of references to this device
 *	@refcnt_tracker:	Tracker directory for tracked references to this device
 *	@todo_list:		Delayed register/unregister
 *	@link_watch_list:	XXX: need comments on this one
 *
 *	@reg_state:		Register/unregister state machine
 *	@dismantle:		Device is going to be freed
 *	@needs_free_netdev:	Should unregister perform free_netdev?
 *	@priv_destructor:	Called from unregister
 *	@npinfo:		XXX: need comments on this one
 * 	@nd_net:		Network namespace this network device is inside
 *				protected by @lock
 *
 * 	@ml_priv:	Mid-layer private
 *	@ml_priv_type:  Mid-layer private type
 *
 *	@pcpu_stat_type:	Type of device statistics which the core should
 *				allocate/free: none, lstats, tstats, dstats. none
 *				means the driver is handling statistics allocation/
 *				freeing internally.
 *	@lstats:		Loopback statistics: packets, bytes
 *	@tstats:		Tunnel statistics: RX/TX packets, RX/TX bytes
 *	@dstats:		Dummy statistics: RX/TX/drop packets, RX/TX bytes
 *
 *	@garp_port:	GARP
 *	@mrp_port:	MRP
 *
 *	@dm_private:	Drop monitor private
 *
 *	@dev:		Class/net/name entry
 *	@sysfs_groups:	Space for optional device, statistics and wireless
 *			sysfs groups
 *
 *	@sysfs_rx_queue_group:	Space for optional per-rx queue attributes
 *	@rtnl_link_ops:	Rtnl_link_ops
 *	@stat_ops:	Optional ops for queue-aware statistics
 *	@queue_mgmt_ops:	Optional ops for queue management
 *
 *	@gso_max_size:	Maximum size of generic segmentation offload
 *	@tso_max_size:	Device (as in HW) limit on the max TSO request size
 *	@gso_max_segs:	Maximum number of segments that can be passed to the
 *			NIC for GSO
 *	@tso_max_segs:	Device (as in HW) limit on the max TSO segment count
 * 	@gso_ipv4_max_size:	Maximum size of generic segmentation offload,
 * 				for IPv4.
 *
 *	@dcbnl_ops:	Data Center Bridging netlink ops
 *	@num_tc:	Number of traffic classes in the net device
 *	@tc_to_txq:	XXX: need comments on this one
 *	@prio_tc_map:	XXX: need comments on this one
 *
 *	@fcoe_ddp_xid:	Max exchange id for FCoE LRO by ddp
 *
 *	@priomap:	XXX: need comments on this one
 *	@link_topo:	Physical link topology tracking attached PHYs
 *	@phydev:	Physical device may attach itself
 *			for hardware timestamping
 *	@sfp_bus:	attached &struct sfp_bus structure.
 *
 *	@qdisc_tx_busylock: lockdep class annotating Qdisc->busylock spinlock
 *
 *	@proto_down:	protocol port state information can be sent to the
 *			switch driver and used to set the phys state of the
 *			switch port.
 *
 *	@irq_affinity_auto: driver wants the core to store and re-assign the IRQ
 *			    affinity. Set by netif_enable_irq_affinity(), then
 *			    the driver must create a persistent napi by
 *			    netif_napi_add_config() and finally bind the napi to
 *			    IRQ (via netif_napi_set_irq()).
 *
 *	@rx_cpu_rmap_auto: driver wants the core to manage the ARFS rmap.
 *	                   Set by calling netif_enable_cpu_rmap().
 *
 *	@see_all_hwtstamp_requests: device wants to see calls to
 *			ndo_hwtstamp_set() for all timestamp requests
 *			regardless of source, even if those aren't
 *			HWTSTAMP_SOURCE_NETDEV
 *	@change_proto_down: device supports setting carrier via IFLA_PROTO_DOWN
 *	@netns_immutable: interface can't change network namespaces
 *	@fcoe_mtu:	device supports maximum FCoE MTU, 2158 bytes
 *
 *	@net_notifier_list:	List of per-net netdev notifier block
 *				that follow this device when it is moved
 *				to another network namespace.
 *
 *	@macsec_ops:    MACsec offloading ops
 *
 *	@udp_tunnel_nic_info:	static structure describing the UDP tunnel
 *				offload capabilities of the device
 *	@udp_tunnel_nic:	UDP tunnel offload state
 *	@ethtool:	ethtool related state
 *	@xdp_state:		stores info on attached XDP BPF programs
 *
 *	@nested_level:	Used as a parameter of spin_lock_nested() of
 *			dev->addr_list_lock.
 *	@unlink_list:	As netif_addr_lock() can be called recursively,
 *			keep a list of interfaces to be deleted.
 *	@gro_max_size:	Maximum size of aggregated packet in generic
 *			receive offload (GRO)
 * 	@gro_ipv4_max_size:	Maximum size of aggregated packet in generic
 * 				receive offload (GRO), for IPv4.
 *	@xdp_zc_max_segs:	Maximum number of segments supported by AF_XDP
 *				zero copy driver
 *
 *	@dev_addr_shadow:	Copy of @dev_addr to catch direct writes.
 *	@linkwatch_dev_tracker:	refcount tracker used by linkwatch.
 *	@watchdog_dev_tracker:	refcount tracker used by watchdog.
 *	@dev_registered_tracker:	tracker for reference held while
 *					registered
 *	@offload_xstats_l3:	L3 HW stats for this netdevice.
 *
 *	@devlink_port:	Pointer to related devlink port structure.
 *			Assigned by a driver before netdev registration using
 *			SET_NETDEV_DEVLINK_PORT macro. This pointer is static
 *			during the time netdevice is registered.
 *
 *	@dpll_pin: Pointer to the SyncE source pin of a DPLL subsystem,
 *		   where the clock is recovered.
 *
 *	@max_pacing_offload_horizon: max EDT offload horizon in nsec.
 *	@napi_config: An array of napi_config structures containing per-NAPI
 *		      settings.
 *	@gro_flush_timeout:	timeout for GRO layer in NAPI
 *	@napi_defer_hard_irqs:	If not zero, provides a counter that would
 *				allow to avoid NIC hard IRQ, on busy queues.
 *
 *	@neighbours:	List heads pointing to this device's neighbours'
 *			dev_list, one per address-family.
 *	@hwprov: Tracks which PTP performs hardware packet time stamping.
 *
 *	FIXME: cleanup struct net_device such that network protocol info
 *	moves out.
 */

struct net_device {
	/* Cacheline organization can be found documented in
	 * Documentation/networking/net_cachelines/net_device.rst.
	 * Please update the document when adding new fields.
	 */

	/* TX read-mostly hotpath */
	__cacheline_group_begin(net_device_read_tx);
	struct_group(priv_flags_fast,
		unsigned long		priv_flags:32;
		unsigned long		lltx:1;
		unsigned long		netmem_tx:1;
	);
	const struct net_device_ops *netdev_ops;
	const struct header_ops *header_ops;
	struct netdev_queue	*_tx;
	netdev_features_t	gso_partial_features;
	unsigned int		real_num_tx_queues;
	unsigned int		gso_max_size;
	unsigned int		gso_ipv4_max_size;
	u16			gso_max_segs;
	s16			num_tc;
	/* Note : dev->mtu is often read without holding a lock.
	 * Writers usually hold RTNL.
	 * It is recommended to use READ_ONCE() to annotate the reads,
	 * and to use WRITE_ONCE() to annotate the writes.
	 */
	unsigned int		mtu;
	unsigned short		needed_headroom;
	struct netdev_tc_txq	tc_to_txq[TC_MAX_QUEUE];
#ifdef CONFIG_XPS
	struct xps_dev_maps __rcu *xps_maps[XPS_MAPS_MAX];
#endif
#ifdef CONFIG_NETFILTER_EGRESS
	struct nf_hook_entries __rcu *nf_hooks_egress;
#endif
#ifdef CONFIG_NET_XGRESS
	struct bpf_mprog_entry __rcu *tcx_egress;
#endif
	__cacheline_group_end(net_device_read_tx);

	/* TXRX read-mostly hotpath */
	__cacheline_group_begin(net_device_read_txrx);
	union {
		struct pcpu_lstats __percpu		*lstats;
		struct pcpu_sw_netstats __percpu	*tstats;
		struct pcpu_dstats __percpu		*dstats;
	};
	unsigned long		state;
	unsigned int		flags;
	unsigned short		hard_header_len;
	netdev_features_t	features;
	struct inet6_dev __rcu	*ip6_ptr;
	__cacheline_group_end(net_device_read_txrx);

	/* RX read-mostly hotpath */
	__cacheline_group_begin(net_device_read_rx);
	struct bpf_prog __rcu	*xdp_prog;
	struct list_head	ptype_specific;
	int			ifindex;
	unsigned int		real_num_rx_queues;
	struct netdev_rx_queue	*_rx;
	unsigned int		gro_max_size;
	unsigned int		gro_ipv4_max_size;
	rx_handler_func_t __rcu	*rx_handler;
	void __rcu		*rx_handler_data;
	possible_net_t			nd_net;
#ifdef CONFIG_NETPOLL
	struct netpoll_info __rcu	*npinfo;
#endif
#ifdef CONFIG_NET_XGRESS
	struct bpf_mprog_entry __rcu *tcx_ingress;
#endif
	__cacheline_group_end(net_device_read_rx);

	char			name[IFNAMSIZ];
	struct netdev_name_node	*name_node;
	struct dev_ifalias	__rcu *ifalias;
	/*
	 *	I/O specific fields
	 *	FIXME: Merge these and struct ifmap into one
	 */
	unsigned long		mem_end;
	unsigned long		mem_start;
	unsigned long		base_addr;

	/*
	 *	Some hardware also needs these fields (state,dev_list,
	 *	napi_list,unreg_list,close_list) but they are not
	 *	part of the usual set specified in Space.c.
	 */


	struct list_head	dev_list;
	struct list_head	napi_list;
	struct list_head	unreg_list;
	struct list_head	close_list;
	struct list_head	ptype_all;

	struct {
		struct list_head upper;
		struct list_head lower;
	} adj_list;

	/* Read-mostly cache-line for fast-path access */
	xdp_features_t		xdp_features;
	const struct xdp_metadata_ops *xdp_metadata_ops;
	const struct xsk_tx_metadata_ops *xsk_tx_metadata_ops;
	unsigned short		gflags;

	unsigned short		needed_tailroom;

	netdev_features_t	hw_features;
	netdev_features_t	wanted_features;
	netdev_features_t	vlan_features;
	netdev_features_t	hw_enc_features;
	netdev_features_t	mpls_features;

	unsigned int		min_mtu;
	unsigned int		max_mtu;
	unsigned short		type;
	unsigned char		min_header_len;
	unsigned char		name_assign_type;

	int			group;

	struct net_device_stats	stats; /* not used by modern drivers */

	struct net_device_core_stats __percpu *core_stats;

	/* Stats to monitor link on/off, flapping */
	atomic_t		carrier_up_count;
	atomic_t		carrier_down_count;

#ifdef CONFIG_WIRELESS_EXT
	const struct iw_handler_def *wireless_handlers;
#endif
	const struct ethtool_ops *ethtool_ops;
#ifdef CONFIG_NET_L3_MASTER_DEV
	const struct l3mdev_ops	*l3mdev_ops;
#endif
#if IS_ENABLED(CONFIG_IPV6)
	const struct ndisc_ops *ndisc_ops;
#endif

#ifdef CONFIG_XFRM_OFFLOAD
	const struct xfrmdev_ops *xfrmdev_ops;
#endif

#if IS_ENABLED(CONFIG_TLS_DEVICE)
	const struct tlsdev_ops *tlsdev_ops;
#endif

	unsigned int		operstate;
	unsigned char		link_mode;

	unsigned char		if_port;
	unsigned char		dma;

	/* Interface address info. */
	unsigned char		perm_addr[MAX_ADDR_LEN];
	unsigned char		addr_assign_type;
	unsigned char		addr_len;
	unsigned char		upper_level;
	unsigned char		lower_level;
	u8			threaded;

	unsigned short		neigh_priv_len;
	unsigned short          dev_id;
	unsigned short          dev_port;
	int			irq;
	u32			priv_len;

	spinlock_t		addr_list_lock;

	struct netdev_hw_addr_list	uc;
	struct netdev_hw_addr_list	mc;
	struct netdev_hw_addr_list	dev_addrs;

#ifdef CONFIG_SYSFS
	struct kset		*queues_kset;
#endif
#ifdef CONFIG_LOCKDEP
	struct list_head	unlink_list;
#endif
	unsigned int		promiscuity;
	unsigned int		allmulti;
	bool			uc_promisc;
#ifdef CONFIG_LOCKDEP
	unsigned char		nested_level;
#endif


	/* Protocol-specific pointers */
	struct in_device __rcu	*ip_ptr;
	/** @fib_nh_head: nexthops associated with this netdev */
	struct hlist_head	fib_nh_head;

#if IS_ENABLED(CONFIG_VLAN_8021Q)
	struct vlan_info __rcu	*vlan_info;
#endif
#if IS_ENABLED(CONFIG_NET_DSA)
	struct dsa_port		*dsa_ptr;
#endif
#if IS_ENABLED(CONFIG_TIPC)
	struct tipc_bearer __rcu *tipc_ptr;
#endif
#if IS_ENABLED(CONFIG_ATALK)
	void 			*atalk_ptr;
#endif
#if IS_ENABLED(CONFIG_AX25)
	struct ax25_dev	__rcu	*ax25_ptr;
#endif
#if IS_ENABLED(CONFIG_CFG80211)
	struct wireless_dev	*ieee80211_ptr;
#endif
#if IS_ENABLED(CONFIG_IEEE802154) || IS_ENABLED(CONFIG_6LOWPAN)
	struct wpan_dev		*ieee802154_ptr;
#endif
#if IS_ENABLED(CONFIG_MPLS_ROUTING)
	struct mpls_dev __rcu	*mpls_ptr;
#endif
#if IS_ENABLED(CONFIG_MCTP)
	struct mctp_dev __rcu	*mctp_ptr;
#endif

/*
 * Cache lines mostly used on receive path (including eth_type_trans())
 */
	/* Interface address info used in eth_type_trans() */
	const unsigned char	*dev_addr;

	unsigned int		num_rx_queues;
#define GRO_LEGACY_MAX_SIZE	65536u
/* TCP minimal MSS is 8 (TCP_MIN_GSO_SIZE),
 * and shinfo->gso_segs is a 16bit field.
 */
#define GRO_MAX_SIZE		(8 * 65535u)
	unsigned int		xdp_zc_max_segs;
	struct netdev_queue __rcu *ingress_queue;
#ifdef CONFIG_NETFILTER_INGRESS
	struct nf_hook_entries __rcu *nf_hooks_ingress;
#endif

	unsigned char		broadcast[MAX_ADDR_LEN];
#ifdef CONFIG_RFS_ACCEL
	struct cpu_rmap		*rx_cpu_rmap;
#endif
	struct hlist_node	index_hlist;

/*
 * Cache lines mostly used on transmit path
 */
	unsigned int		num_tx_queues;
	struct Qdisc __rcu	*qdisc;
	unsigned int		tx_queue_len;
	spinlock_t		tx_global_lock;

	struct xdp_dev_bulk_queue __percpu *xdp_bulkq;

#ifdef CONFIG_NET_SCHED
	DECLARE_HASHTABLE	(qdisc_hash, 4);
#endif
	/* These may be needed for future network-power-down code. */
	struct timer_list	watchdog_timer;
	int			watchdog_timeo;

	u32                     proto_down_reason;

	struct list_head	todo_list;

#ifdef CONFIG_PCPU_DEV_REFCNT
	int __percpu		*pcpu_refcnt;
#else
	refcount_t		dev_refcnt;
#endif
	struct ref_tracker_dir	refcnt_tracker;

	struct list_head	link_watch_list;

	u8 reg_state;

	bool dismantle;

	/** @moving_ns: device is changing netns, protected by @lock */
	bool moving_ns;
	/** @rtnl_link_initializing: Device being created, suppress events */
	bool rtnl_link_initializing;

	bool needs_free_netdev;
	void (*priv_destructor)(struct net_device *dev);

	/* mid-layer private */
	void				*ml_priv;
	enum netdev_ml_priv_type	ml_priv_type;

	enum netdev_stat_type		pcpu_stat_type:8;

#if IS_ENABLED(CONFIG_GARP)
	struct garp_port __rcu	*garp_port;
#endif
#if IS_ENABLED(CONFIG_MRP)
	struct mrp_port __rcu	*mrp_port;
#endif
#if IS_ENABLED(CONFIG_NET_DROP_MONITOR)
	struct dm_hw_stat_delta __rcu *dm_private;
#endif
	struct device		dev;
	const struct attribute_group *sysfs_groups[5];
	const struct attribute_group *sysfs_rx_queue_group;

	const struct rtnl_link_ops *rtnl_link_ops;

	const struct netdev_stat_ops *stat_ops;

	const struct netdev_queue_mgmt_ops *queue_mgmt_ops;

	/* for setting kernel sock attribute on TCP connection setup */
#define GSO_MAX_SEGS		65535u
#define GSO_LEGACY_MAX_SIZE	65536u
/* TCP minimal MSS is 8 (TCP_MIN_GSO_SIZE),
 * and shinfo->gso_segs is a 16bit field.
 */
#define GSO_MAX_SIZE		(8 * GSO_MAX_SEGS)

#define TSO_LEGACY_MAX_SIZE	65536
#define TSO_MAX_SIZE		UINT_MAX
	unsigned int		tso_max_size;
#define TSO_MAX_SEGS		U16_MAX
	u16			tso_max_segs;

#ifdef CONFIG_DCB
	const struct dcbnl_rtnl_ops *dcbnl_ops;
#endif
	u8			prio_tc_map[TC_BITMASK + 1];

#if IS_ENABLED(CONFIG_FCOE)
	unsigned int		fcoe_ddp_xid;
#endif
#if IS_ENABLED(CONFIG_CGROUP_NET_PRIO)
	struct netprio_map __rcu *priomap;
#endif
	struct phy_link_topology	*link_topo;
	struct phy_device	*phydev;
	struct sfp_bus		*sfp_bus;
	struct lock_class_key	*qdisc_tx_busylock;
	bool			proto_down;
	bool			irq_affinity_auto;
	bool			rx_cpu_rmap_auto;

	/* priv_flags_slow, ungrouped to save space */
	unsigned long		see_all_hwtstamp_requests:1;
	unsigned long		change_proto_down:1;
	unsigned long		netns_immutable:1;
	unsigned long		fcoe_mtu:1;

	struct list_head	net_notifier_list;

#if IS_ENABLED(CONFIG_MACSEC)
	/* MACsec management functions */
	const struct macsec_ops *macsec_ops;
#endif
	const struct udp_tunnel_nic_info	*udp_tunnel_nic_info;
	struct udp_tunnel_nic	*udp_tunnel_nic;

	/** @cfg: net_device queue-related configuration */
	struct netdev_config	*cfg;
	/**
	 * @cfg_pending: same as @cfg but when device is being actively
	 *	reconfigured includes any changes to the configuration
	 *	requested by the user, but which may or may not be rejected.
	 */
	struct netdev_config	*cfg_pending;
	struct ethtool_netdev_state *ethtool;

	/* protected by rtnl_lock */
	struct bpf_xdp_entity	xdp_state[__MAX_XDP_MODE];

	u8 dev_addr_shadow[MAX_ADDR_LEN];
	netdevice_tracker	linkwatch_dev_tracker;
	netdevice_tracker	watchdog_dev_tracker;
	netdevice_tracker	dev_registered_tracker;
	struct rtnl_hw_stats64	*offload_xstats_l3;

	struct devlink_port	*devlink_port;

#if IS_ENABLED(CONFIG_DPLL)
	struct dpll_pin	__rcu	*dpll_pin;
#endif
#if IS_ENABLED(CONFIG_PAGE_POOL)
	/** @page_pools: page pools created for this netdevice */
	struct hlist_head	page_pools;
#endif

	/** @irq_moder: dim parameters used if IS_ENABLED(CONFIG_DIMLIB). */
	struct dim_irq_moder	*irq_moder;

	u64			max_pacing_offload_horizon;
	struct napi_config	*napi_config;
	unsigned long		gro_flush_timeout;
	u32			napi_defer_hard_irqs;

	/**
	 * @up: copy of @state's IFF_UP, but safe to read with just @lock.
	 *	May report false negatives while the device is being opened
	 *	or closed (@lock does not protect .ndo_open, or .ndo_close).
	 */
	bool			up;

	/**
	 * @request_ops_lock: request the core to run all @netdev_ops and
	 * @ethtool_ops under the @lock.
	 */
	bool			request_ops_lock;

	/**
	 * @lock: netdev-scope lock, protects a small selection of fields.
	 * Should always be taken using netdev_lock() / netdev_unlock() helpers.
	 * Drivers are free to use it for other protection.
	 *
	 * For the drivers that implement shaper or queue API, the scope
	 * of this lock is expanded to cover most ndo/queue/ethtool/sysfs
	 * operations. Drivers may opt-in to this behavior by setting
	 * @request_ops_lock.
	 *
	 * @lock protection mixes with rtnl_lock in multiple ways, fields are
	 * either:
	 *
	 * - simply protected by the instance @lock;
	 *
	 * - double protected - writers hold both locks, readers hold either;
	 *
	 * - ops protected - protected by the lock held around the NDOs
	 *   and other callbacks, that is the instance lock on devices for
	 *   which netdev_need_ops_lock() returns true, otherwise by rtnl_lock;
	 *
	 * - double ops protected - always protected by rtnl_lock but for
	 *   devices for which netdev_need_ops_lock() returns true - also
	 *   the instance lock.
	 *
	 * Simply protects:
	 *	@gro_flush_timeout, @napi_defer_hard_irqs, @napi_list,
	 *	@net_shaper_hierarchy, @reg_state, @threaded
	 *
	 * Double protects:
	 *	@up, @moving_ns, @nd_net, @xdp_features
	 *
	 * Double ops protects:
	 *	@real_num_rx_queues, @real_num_tx_queues
	 *
	 * Also protects some fields in:
	 *	struct napi_struct, struct netdev_queue, struct netdev_rx_queue
	 *
	 * Ordering: take after rtnl_lock.
	 */
	struct mutex		lock;

#if IS_ENABLED(CONFIG_NET_SHAPER)
	/**
	 * @net_shaper_hierarchy: data tracking the current shaper status
	 *  see include/net/net_shapers.h
	 */
	struct net_shaper_hierarchy *net_shaper_hierarchy;
#endif

	struct hlist_head neighbours[NEIGH_NR_TABLES];

	struct hwtstamp_provider __rcu	*hwprov;

	u8			priv[] ____cacheline_aligned
				       __counted_by(priv_len);
} ____cacheline_aligned;
#define to_net_dev(d) container_of(d, struct net_device, dev)

/*
 * Driver should use this to assign devlink port instance to a netdevice
 * before it registers the netdevice. Therefore devlink_port is static
 * during the netdev lifetime after it is registered.
 */
#define SET_NETDEV_DEVLINK_PORT(dev, port)			\
({								\
	WARN_ON((dev)->reg_state != NETREG_UNINITIALIZED);	\
	((dev)->devlink_port = (port));				\
})

static inline bool netif_elide_gro(const struct net_device *dev)
{
	if (!(dev->features & NETIF_F_GRO) || dev->xdp_prog)
		return true;
	return false;
}

#define	NETDEV_ALIGN		32

static inline
int netdev_get_prio_tc_map(const struct net_device *dev, u32 prio)
{
	return dev->prio_tc_map[prio & TC_BITMASK];
}

static inline
int netdev_set_prio_tc_map(struct net_device *dev, u8 prio, u8 tc)
{
	if (tc >= dev->num_tc)
		return -EINVAL;

	dev->prio_tc_map[prio & TC_BITMASK] = tc & TC_BITMASK;
	return 0;
}

int netdev_txq_to_tc(struct net_device *dev, unsigned int txq);
void netdev_reset_tc(struct net_device *dev);
int netdev_set_tc_queue(struct net_device *dev, u8 tc, u16 count, u16 offset);
int netdev_set_num_tc(struct net_device *dev, u8 num_tc);

static inline
int netdev_get_num_tc(struct net_device *dev)
{
	return dev->num_tc;
}

static inline void net_prefetch(void *p)
{
	prefetch(p);
#if L1_CACHE_BYTES < 128
	prefetch((u8 *)p + L1_CACHE_BYTES);
#endif
}

static inline void net_prefetchw(void *p)
{
	prefetchw(p);
#if L1_CACHE_BYTES < 128
	prefetchw((u8 *)p + L1_CACHE_BYTES);
#endif
}

void netdev_unbind_sb_channel(struct net_device *dev,
			      struct net_device *sb_dev);
int netdev_bind_sb_channel_queue(struct net_device *dev,
				 struct net_device *sb_dev,
				 u8 tc, u16 count, u16 offset);
int netdev_set_sb_channel(struct net_device *dev, u16 channel);
static inline int netdev_get_sb_channel(struct net_device *dev)
{
	return max_t(int, -dev->num_tc, 0);
}

static inline
struct netdev_queue *netdev_get_tx_queue(const struct net_device *dev,
					 unsigned int index)
{
	DEBUG_NET_WARN_ON_ONCE(index >= dev->num_tx_queues);
	return &dev->_tx[index];
}

static inline struct netdev_queue *skb_get_tx_queue(const struct net_device *dev,
						    const struct sk_buff *skb)
{
	return netdev_get_tx_queue(dev, skb_get_queue_mapping(skb));
}

static inline void netdev_for_each_tx_queue(struct net_device *dev,
					    void (*f)(struct net_device *,
						      struct netdev_queue *,
						      void *),
					    void *arg)
{
	unsigned int i;

	for (i = 0; i < dev->num_tx_queues; i++)
		f(dev, &dev->_tx[i], arg);
}

u16 netdev_pick_tx(struct net_device *dev, struct sk_buff *skb,
		     struct net_device *sb_dev);
struct netdev_queue *netdev_core_pick_tx(struct net_device *dev,
					 struct sk_buff *skb,
					 struct net_device *sb_dev);

/* returns the headroom that the master device needs to take in account
 * when forwarding to this dev
 */
static inline unsigned netdev_get_fwd_headroom(struct net_device *dev)
{
	return dev->priv_flags & IFF_PHONY_HEADROOM ? 0 : dev->needed_headroom;
}

static inline void netdev_set_rx_headroom(struct net_device *dev, int new_hr)
{
	if (dev->netdev_ops->ndo_set_rx_headroom)
		dev->netdev_ops->ndo_set_rx_headroom(dev, new_hr);
}

/* set the device rx headroom to the dev's default */
static inline void netdev_reset_rx_headroom(struct net_device *dev)
{
	netdev_set_rx_headroom(dev, -1);
}

static inline void *netdev_get_ml_priv(struct net_device *dev,
				       enum netdev_ml_priv_type type)
{
	if (dev->ml_priv_type != type)
		return NULL;

	return dev->ml_priv;
}

static inline void netdev_set_ml_priv(struct net_device *dev,
				      void *ml_priv,
				      enum netdev_ml_priv_type type)
{
	WARN(dev->ml_priv_type && dev->ml_priv_type != type,
	     "Overwriting already set ml_priv_type (%u) with different ml_priv_type (%u)!\n",
	     dev->ml_priv_type, type);
	WARN(!dev->ml_priv_type && dev->ml_priv,
	     "Overwriting already set ml_priv and ml_priv_type is ML_PRIV_NONE!\n");

	dev->ml_priv = ml_priv;
	dev->ml_priv_type = type;
}

/*
 * Net namespace inlines
 */
static inline
struct net *dev_net(const struct net_device *dev)
{
	return read_pnet(&dev->nd_net);
}

static inline
struct net *dev_net_rcu(const struct net_device *dev)
{
	return read_pnet_rcu(&dev->nd_net);
}

static inline
void dev_net_set(struct net_device *dev, struct net *net)
{
	write_pnet(&dev->nd_net, net);
}

/**
 *	netdev_priv - access network device private data
 *	@dev: network device
 *
 * Get network device private data
 */
static inline void *netdev_priv(const struct net_device *dev)
{
	return (void *)dev->priv;
}

/* Set the sysfs physical device reference for the network logical device
 * if set prior to registration will cause a symlink during initialization.
 */
#define SET_NETDEV_DEV(net, pdev)	((net)->dev.parent = (pdev))

/* Set the sysfs device type for the network logical device to allow
 * fine-grained identification of different network device types. For
 * example Ethernet, Wireless LAN, Bluetooth, WiMAX etc.
 */
#define SET_NETDEV_DEVTYPE(net, devtype)	((net)->dev.type = (devtype))

void netif_queue_set_napi(struct net_device *dev, unsigned int queue_index,
			  enum netdev_queue_type type,
			  struct napi_struct *napi);

static inline void netdev_lock(struct net_device *dev)
{
	mutex_lock(&dev->lock);
}

static inline void netdev_unlock(struct net_device *dev)
{
	mutex_unlock(&dev->lock);
}
/* Additional netdev_lock()-related helpers are in net/netdev_lock.h */

void netif_napi_set_irq_locked(struct napi_struct *napi, int irq);

static inline void netif_napi_set_irq(struct napi_struct *napi, int irq)
{
	netdev_lock(napi->dev);
	netif_napi_set_irq_locked(napi, irq);
	netdev_unlock(napi->dev);
}

/* Default NAPI poll() weight
 * Device drivers are strongly advised to not use bigger value
 */
#define NAPI_POLL_WEIGHT 64

void netif_napi_add_weight_locked(struct net_device *dev,
				  struct napi_struct *napi,
				  int (*poll)(struct napi_struct *, int),
				  int weight);

static inline void
netif_napi_add_weight(struct net_device *dev, struct napi_struct *napi,
		      int (*poll)(struct napi_struct *, int), int weight)
{
	netdev_lock(dev);
	netif_napi_add_weight_locked(dev, napi, poll, weight);
	netdev_unlock(dev);
}

/**
 * netif_napi_add() - initialize a NAPI context
 * @dev:  network device
 * @napi: NAPI context
 * @poll: polling function
 *
 * netif_napi_add() must be used to initialize a NAPI context prior to calling
 * *any* of the other NAPI-related functions.
 */
static inline void
netif_napi_add(struct net_device *dev, struct napi_struct *napi,
	       int (*poll)(struct napi_struct *, int))
{
	netif_napi_add_weight(dev, napi, poll, NAPI_POLL_WEIGHT);
}

static inline void
netif_napi_add_locked(struct net_device *dev, struct napi_struct *napi,
		      int (*poll)(struct napi_struct *, int))
{
	netif_napi_add_weight_locked(dev, napi, poll, NAPI_POLL_WEIGHT);
}

static inline void
netif_napi_add_tx_weight(struct net_device *dev,
			 struct napi_struct *napi,
			 int (*poll)(struct napi_struct *, int),
			 int weight)
{
	set_bit(NAPI_STATE_NO_BUSY_POLL, &napi->state);
	netif_napi_add_weight(dev, napi, poll, weight);
}

static inline void
netif_napi_add_config_locked(struct net_device *dev, struct napi_struct *napi,
			     int (*poll)(struct napi_struct *, int), int index)
{
	napi->index = index;
	napi->config = &dev->napi_config[index];
	netif_napi_add_weight_locked(dev, napi, poll, NAPI_POLL_WEIGHT);
}

/**
 * netif_napi_add_config - initialize a NAPI context with persistent config
 * @dev: network device
 * @napi: NAPI context
 * @poll: polling function
 * @index: the NAPI index
 */
static inline void
netif_napi_add_config(struct net_device *dev, struct napi_struct *napi,
		      int (*poll)(struct napi_struct *, int), int index)
{
	netdev_lock(dev);
	netif_napi_add_config_locked(dev, napi, poll, index);
	netdev_unlock(dev);
}

/**
 * netif_napi_add_tx() - initialize a NAPI context to be used for Tx only
 * @dev:  network device
 * @napi: NAPI context
 * @poll: polling function
 *
 * This variant of netif_napi_add() should be used from drivers using NAPI
 * to exclusively poll a TX queue.
 * This will avoid we add it into napi_hash[], thus polluting this hash table.
 */
static inline void netif_napi_add_tx(struct net_device *dev,
				     struct napi_struct *napi,
				     int (*poll)(struct napi_struct *, int))
{
	netif_napi_add_tx_weight(dev, napi, poll, NAPI_POLL_WEIGHT);
}

void __netif_napi_del_locked(struct napi_struct *napi);

/**
 *  __netif_napi_del - remove a NAPI context
 *  @napi: NAPI context
 *
 * Warning: caller must observe RCU grace period before freeing memory
 * containing @napi. Drivers might want to call this helper to combine
 * all the needed RCU grace periods into a single one.
 */
static inline void __netif_napi_del(struct napi_struct *napi)
{
	netdev_lock(napi->dev);
	__netif_napi_del_locked(napi);
	netdev_unlock(napi->dev);
}

static inline void netif_napi_del_locked(struct napi_struct *napi)
{
	__netif_napi_del_locked(napi);
	synchronize_net();
}

/**
 *  netif_napi_del - remove a NAPI context
 *  @napi: NAPI context
 *
 *  netif_napi_del() removes a NAPI context from the network device NAPI list
 */
static inline void netif_napi_del(struct napi_struct *napi)
{
	__netif_napi_del(napi);
	synchronize_net();
}

int netif_enable_cpu_rmap(struct net_device *dev, unsigned int num_irqs);
void netif_set_affinity_auto(struct net_device *dev);

struct packet_type {
	__be16			type;	/* This is really htons(ether_type). */
	bool			ignore_outgoing;
	struct net_device	*dev;	/* NULL is wildcarded here	     */
	netdevice_tracker	dev_tracker;
	int			(*func) (struct sk_buff *,
					 struct net_device *,
					 struct packet_type *,
					 struct net_device *);
	void			(*list_func) (struct list_head *,
					      struct packet_type *,
					      struct net_device *);
	bool			(*id_match)(struct packet_type *ptype,
					    struct sock *sk);
	struct net		*af_packet_net;
	void			*af_packet_priv;
	struct list_head	list;
};

struct offload_callbacks {
	struct sk_buff		*(*gso_segment)(struct sk_buff *skb,
						netdev_features_t features);
	struct sk_buff		*(*gro_receive)(struct list_head *head,
						struct sk_buff *skb);
	int			(*gro_complete)(struct sk_buff *skb, int nhoff);
};

struct packet_offload {
	__be16			 type;	/* This is really htons(ether_type). */
	u16			 priority;
	struct offload_callbacks callbacks;
	struct list_head	 list;
};

/* often modified stats are per-CPU, other are shared (netdev->stats) */
struct pcpu_sw_netstats {
	u64_stats_t		rx_packets;
	u64_stats_t		rx_bytes;
	u64_stats_t		tx_packets;
	u64_stats_t		tx_bytes;
	struct u64_stats_sync   syncp;
} __aligned(4 * sizeof(u64));

struct pcpu_dstats {
	u64_stats_t		rx_packets;
	u64_stats_t		rx_bytes;
	u64_stats_t		tx_packets;
	u64_stats_t		tx_bytes;
	u64_stats_t		rx_drops;
	u64_stats_t		tx_drops;
	struct u64_stats_sync	syncp;
} __aligned(8 * sizeof(u64));

struct pcpu_lstats {
	u64_stats_t packets;
	u64_stats_t bytes;
	struct u64_stats_sync syncp;
} __aligned(2 * sizeof(u64));

void dev_lstats_read(struct net_device *dev, u64 *packets, u64 *bytes);

static inline void dev_sw_netstats_rx_add(struct net_device *dev, unsigned int len)
{
	struct pcpu_sw_netstats *tstats = this_cpu_ptr(dev->tstats);

	u64_stats_update_begin(&tstats->syncp);
	u64_stats_add(&tstats->rx_bytes, len);
	u64_stats_inc(&tstats->rx_packets);
	u64_stats_update_end(&tstats->syncp);
}

static inline void dev_sw_netstats_tx_add(struct net_device *dev,
					  unsigned int packets,
					  unsigned int len)
{
	struct pcpu_sw_netstats *tstats = this_cpu_ptr(dev->tstats);

	u64_stats_update_begin(&tstats->syncp);
	u64_stats_add(&tstats->tx_bytes, len);
	u64_stats_add(&tstats->tx_packets, packets);
	u64_stats_update_end(&tstats->syncp);
}

static inline void dev_lstats_add(struct net_device *dev, unsigned int len)
{
	struct pcpu_lstats *lstats = this_cpu_ptr(dev->lstats);

	u64_stats_update_begin(&lstats->syncp);
	u64_stats_add(&lstats->bytes, len);
	u64_stats_inc(&lstats->packets);
	u64_stats_update_end(&lstats->syncp);
}

static inline void dev_dstats_rx_add(struct net_device *dev,
				     unsigned int len)
{
	struct pcpu_dstats *dstats = this_cpu_ptr(dev->dstats);

	u64_stats_update_begin(&dstats->syncp);
	u64_stats_inc(&dstats->rx_packets);
	u64_stats_add(&dstats->rx_bytes, len);
	u64_stats_update_end(&dstats->syncp);
}

static inline void dev_dstats_rx_dropped(struct net_device *dev)
{
	struct pcpu_dstats *dstats = this_cpu_ptr(dev->dstats);

	u64_stats_update_begin(&dstats->syncp);
	u64_stats_inc(&dstats->rx_drops);
	u64_stats_update_end(&dstats->syncp);
}

static inline void dev_dstats_rx_dropped_add(struct net_device *dev,
					     unsigned int packets)
{
	struct pcpu_dstats *dstats = this_cpu_ptr(dev->dstats);

	u64_stats_update_begin(&dstats->syncp);
	u64_stats_add(&dstats->rx_drops, packets);
	u64_stats_update_end(&dstats->syncp);
}

static inline void dev_dstats_tx_add(struct net_device *dev,
				     unsigned int len)
{
	struct pcpu_dstats *dstats = this_cpu_ptr(dev->dstats);

	u64_stats_update_begin(&dstats->syncp);
	u64_stats_inc(&dstats->tx_packets);
	u64_stats_add(&dstats->tx_bytes, len);
	u64_stats_update_end(&dstats->syncp);
}

static inline void dev_dstats_tx_dropped(struct net_device *dev)
{
	struct pcpu_dstats *dstats = this_cpu_ptr(dev->dstats);

	u64_stats_update_begin(&dstats->syncp);
	u64_stats_inc(&dstats->tx_drops);
	u64_stats_update_end(&dstats->syncp);
}

#define __netdev_alloc_pcpu_stats(type, gfp)				\
({									\
	typeof(type) __percpu *pcpu_stats = alloc_percpu_gfp(type, gfp);\
	if (pcpu_stats)	{						\
		int __cpu;						\
		for_each_possible_cpu(__cpu) {				\
			typeof(type) *stat;				\
			stat = per_cpu_ptr(pcpu_stats, __cpu);		\
			u64_stats_init(&stat->syncp);			\
		}							\
	}								\
	pcpu_stats;							\
})

#define netdev_alloc_pcpu_stats(type)					\
	__netdev_alloc_pcpu_stats(type, GFP_KERNEL)

#define devm_netdev_alloc_pcpu_stats(dev, type)				\
({									\
	typeof(type) __percpu *pcpu_stats = devm_alloc_percpu(dev, type);\
	if (pcpu_stats) {						\
		int __cpu;						\
		for_each_possible_cpu(__cpu) {				\
			typeof(type) *stat;				\
			stat = per_cpu_ptr(pcpu_stats, __cpu);		\
			u64_stats_init(&stat->syncp);			\
		}							\
	}								\
	pcpu_stats;							\
})

enum netdev_lag_tx_type {
	NETDEV_LAG_TX_TYPE_UNKNOWN,
	NETDEV_LAG_TX_TYPE_RANDOM,
	NETDEV_LAG_TX_TYPE_BROADCAST,
	NETDEV_LAG_TX_TYPE_ROUNDROBIN,
	NETDEV_LAG_TX_TYPE_ACTIVEBACKUP,
	NETDEV_LAG_TX_TYPE_HASH,
};

enum netdev_lag_hash {
	NETDEV_LAG_HASH_NONE,
	NETDEV_LAG_HASH_L2,
	NETDEV_LAG_HASH_L34,
	NETDEV_LAG_HASH_L23,
	NETDEV_LAG_HASH_E23,
	NETDEV_LAG_HASH_E34,
	NETDEV_LAG_HASH_VLAN_SRCMAC,
	NETDEV_LAG_HASH_UNKNOWN,
};

struct netdev_lag_upper_info {
	enum netdev_lag_tx_type tx_type;
	enum netdev_lag_hash hash_type;
};

struct netdev_lag_lower_state_info {
	u8 link_up : 1,
	   tx_enabled : 1;
};

#include <linux/notifier.h>

/* netdevice notifier chain. Please remember to update netdev_cmd_to_name()
 * and the rtnetlink notification exclusion list in rtnetlink_event() when
 * adding new types.
 */
enum netdev_cmd {
	NETDEV_UP	= 1,	/* For now you can't veto a device up/down */
	NETDEV_DOWN,
	NETDEV_REBOOT,		/* Tell a protocol stack a network interface
				   detected a hardware crash and restarted
				   - we can use this eg to kick tcp sessions
				   once done */
	NETDEV_CHANGE,		/* Notify device state change */
	NETDEV_REGISTER,
	NETDEV_UNREGISTER,
	NETDEV_CHANGEMTU,	/* notify after mtu change happened */
	NETDEV_CHANGEADDR,	/* notify after the address change */
	NETDEV_PRE_CHANGEADDR,	/* notify before the address change */
	NETDEV_GOING_DOWN,
	NETDEV_CHANGENAME,
	NETDEV_FEAT_CHANGE,
	NETDEV_BONDING_FAILOVER,
	NETDEV_PRE_UP,
	NETDEV_PRE_TYPE_CHANGE,
	NETDEV_POST_TYPE_CHANGE,
	NETDEV_POST_INIT,
	NETDEV_PRE_UNINIT,
	NETDEV_RELEASE,
	NETDEV_NOTIFY_PEERS,
	NETDEV_JOIN,
	NETDEV_CHANGEUPPER,
	NETDEV_RESEND_IGMP,
	NETDEV_PRECHANGEMTU,	/* notify before mtu change happened */
	NETDEV_CHANGEINFODATA,
	NETDEV_BONDING_INFO,
	NETDEV_PRECHANGEUPPER,
	NETDEV_CHANGELOWERSTATE,
	NETDEV_UDP_TUNNEL_PUSH_INFO,
	NETDEV_UDP_TUNNEL_DROP_INFO,
	NETDEV_CHANGE_TX_QUEUE_LEN,
	NETDEV_CVLAN_FILTER_PUSH_INFO,
	NETDEV_CVLAN_FILTER_DROP_INFO,
	NETDEV_SVLAN_FILTER_PUSH_INFO,
	NETDEV_SVLAN_FILTER_DROP_INFO,
	NETDEV_OFFLOAD_XSTATS_ENABLE,
	NETDEV_OFFLOAD_XSTATS_DISABLE,
	NETDEV_OFFLOAD_XSTATS_REPORT_USED,
	NETDEV_OFFLOAD_XSTATS_REPORT_DELTA,
	NETDEV_XDP_FEAT_CHANGE,
};
const char *netdev_cmd_to_name(enum netdev_cmd cmd);

int register_netdevice_notifier(struct notifier_block *nb);
int unregister_netdevice_notifier(struct notifier_block *nb);
int register_netdevice_notifier_net(struct net *net, struct notifier_block *nb);
int unregister_netdevice_notifier_net(struct net *net,
				      struct notifier_block *nb);
int register_netdevice_notifier_dev_net(struct net_device *dev,
					struct notifier_block *nb,
					struct netdev_net_notifier *nn);
int unregister_netdevice_notifier_dev_net(struct net_device *dev,
					  struct notifier_block *nb,
					  struct netdev_net_notifier *nn);

struct netdev_notifier_info {
	struct net_device	*dev;
	struct netlink_ext_ack	*extack;
};

struct netdev_notifier_info_ext {
	struct netdev_notifier_info info; /* must be first */
	union {
		u32 mtu;
	} ext;
};

struct netdev_notifier_change_info {
	struct netdev_notifier_info info; /* must be first */
	unsigned int flags_changed;
};

struct netdev_notifier_changeupper_info {
	struct netdev_notifier_info info; /* must be first */
	struct net_device *upper_dev; /* new upper dev */
	bool master; /* is upper dev master */
	bool linking; /* is the notification for link or unlink */
	void *upper_info; /* upper dev info */
};

struct netdev_notifier_changelowerstate_info {
	struct netdev_notifier_info info; /* must be first */
	void *lower_state_info; /* is lower dev state */
};

struct netdev_notifier_pre_changeaddr_info {
	struct netdev_notifier_info info; /* must be first */
	const unsigned char *dev_addr;
};

enum netdev_offload_xstats_type {
	NETDEV_OFFLOAD_XSTATS_TYPE_L3 = 1,
};

struct netdev_notifier_offload_xstats_info {
	struct netdev_notifier_info info; /* must be first */
	enum netdev_offload_xstats_type type;

	union {
		/* NETDEV_OFFLOAD_XSTATS_REPORT_DELTA */
		struct netdev_notifier_offload_xstats_rd *report_delta;
		/* NETDEV_OFFLOAD_XSTATS_REPORT_USED */
		struct netdev_notifier_offload_xstats_ru *report_used;
	};
};

int netdev_offload_xstats_enable(struct net_device *dev,
				 enum netdev_offload_xstats_type type,
				 struct netlink_ext_ack *extack);
int netdev_offload_xstats_disable(struct net_device *dev,
				  enum netdev_offload_xstats_type type);
bool netdev_offload_xstats_enabled(const struct net_device *dev,
				   enum netdev_offload_xstats_type type);
int netdev_offload_xstats_get(struct net_device *dev,
			      enum netdev_offload_xstats_type type,
			      struct rtnl_hw_stats64 *stats, bool *used,
			      struct netlink_ext_ack *extack);
void
netdev_offload_xstats_report_delta(struct netdev_notifier_offload_xstats_rd *rd,
				   const struct rtnl_hw_stats64 *stats);
void
netdev_offload_xstats_report_used(struct netdev_notifier_offload_xstats_ru *ru);
void netdev_offload_xstats_push_delta(struct net_device *dev,
				      enum netdev_offload_xstats_type type,
				      const struct rtnl_hw_stats64 *stats);

static inline void netdev_notifier_info_init(struct netdev_notifier_info *info,
					     struct net_device *dev)
{
	info->dev = dev;
	info->extack = NULL;
}

static inline struct net_device *
netdev_notifier_info_to_dev(const struct netdev_notifier_info *info)
{
	return info->dev;
}

static inline struct netlink_ext_ack *
netdev_notifier_info_to_extack(const struct netdev_notifier_info *info)
{
	return info->extack;
}

int call_netdevice_notifiers(unsigned long val, struct net_device *dev);
int call_netdevice_notifiers_info(unsigned long val,
				  struct netdev_notifier_info *info);

#define for_each_netdev(net, d)		\
		list_for_each_entry(d, &(net)->dev_base_head, dev_list)
#define for_each_netdev_reverse(net, d)	\
		list_for_each_entry_reverse(d, &(net)->dev_base_head, dev_list)
#define for_each_netdev_rcu(net, d)		\
		list_for_each_entry_rcu(d, &(net)->dev_base_head, dev_list)
#define for_each_netdev_safe(net, d, n)	\
		list_for_each_entry_safe(d, n, &(net)->dev_base_head, dev_list)
#define for_each_netdev_continue(net, d)		\
		list_for_each_entry_continue(d, &(net)->dev_base_head, dev_list)
#define for_each_netdev_continue_reverse(net, d)		\
		list_for_each_entry_continue_reverse(d, &(net)->dev_base_head, \
						     dev_list)
#define for_each_netdev_continue_rcu(net, d)		\
	list_for_each_entry_continue_rcu(d, &(net)->dev_base_head, dev_list)
#define for_each_netdev_in_bond_rcu(bond, slave)	\
		for_each_netdev_rcu(dev_net_rcu(bond), slave)	\
			if (netdev_master_upper_dev_get_rcu(slave) == (bond))
#define net_device_entry(lh)	list_entry(lh, struct net_device, dev_list)

#define for_each_netdev_dump(net, d, ifindex)				\
	for (; (d = xa_find(&(net)->dev_by_index, &ifindex,		\
			    ULONG_MAX, XA_PRESENT)); ifindex++)

static inline struct net_device *next_net_device(struct net_device *dev)
{
	struct list_head *lh;
	struct net *net;

	net = dev_net(dev);
	lh = dev->dev_list.next;
	return lh == &net->dev_base_head ? NULL : net_device_entry(lh);
}

static inline struct net_device *next_net_device_rcu(struct net_device *dev)
{
	struct list_head *lh;
	struct net *net;

	net = dev_net(dev);
	lh = rcu_dereference(list_next_rcu(&dev->dev_list));
	return lh == &net->dev_base_head ? NULL : net_device_entry(lh);
}

static inline struct net_device *first_net_device(struct net *net)
{
	return list_empty(&net->dev_base_head) ? NULL :
		net_device_entry(net->dev_base_head.next);
}

int netdev_boot_setup_check(struct net_device *dev);
struct net_device *dev_getbyhwaddr(struct net *net, unsigned short type,
				   const char *hwaddr);
struct net_device *dev_getbyhwaddr_rcu(struct net *net, unsigned short type,
				       const char *hwaddr);
struct net_device *dev_getfirstbyhwtype(struct net *net, unsigned short type);
void dev_add_pack(struct packet_type *pt);
void dev_remove_pack(struct packet_type *pt);
void __dev_remove_pack(struct packet_type *pt);
void dev_add_offload(struct packet_offload *po);
void dev_remove_offload(struct packet_offload *po);

int dev_get_iflink(const struct net_device *dev);
int dev_fill_metadata_dst(struct net_device *dev, struct sk_buff *skb);
int dev_fill_forward_path(const struct net_device *dev, const u8 *daddr,
			  struct net_device_path_stack *stack);
struct net_device *dev_get_by_name(struct net *net, const char *name);
struct net_device *dev_get_by_name_rcu(struct net *net, const char *name);
struct net_device *__dev_get_by_name(struct net *net, const char *name);
bool netdev_name_in_use(struct net *net, const char *name);
int dev_alloc_name(struct net_device *dev, const char *name);
int netif_open(struct net_device *dev, struct netlink_ext_ack *extack);
int dev_open(struct net_device *dev, struct netlink_ext_ack *extack);
void netif_close(struct net_device *dev);
void dev_close(struct net_device *dev);
void netif_close_many(struct list_head *head, bool unlink);
void netif_disable_lro(struct net_device *dev);
void dev_disable_lro(struct net_device *dev);
int dev_loopback_xmit(struct net *net, struct sock *sk, struct sk_buff *newskb);
u16 dev_pick_tx_zero(struct net_device *dev, struct sk_buff *skb,
		     struct net_device *sb_dev);

int __dev_queue_xmit(struct sk_buff *skb, struct net_device *sb_dev);
int __dev_direct_xmit(struct sk_buff *skb, u16 queue_id);

static inline int dev_queue_xmit(struct sk_buff *skb)
{
	return __dev_queue_xmit(skb, NULL);
}

static inline int dev_queue_xmit_accel(struct sk_buff *skb,
				       struct net_device *sb_dev)
{
	return __dev_queue_xmit(skb, sb_dev);
}

static inline int dev_direct_xmit(struct sk_buff *skb, u16 queue_id)
{
	int ret;

	ret = __dev_direct_xmit(skb, queue_id);
	if (!dev_xmit_complete(ret))
		kfree_skb(skb);
	return ret;
}

int register_netdevice(struct net_device *dev);
void unregister_netdevice_queue(struct net_device *dev, struct list_head *head);
void unregister_netdevice_many(struct list_head *head);
static inline void unregister_netdevice(struct net_device *dev)
{
	unregister_netdevice_queue(dev, NULL);
}

int netdev_refcnt_read(const struct net_device *dev);
void free_netdev(struct net_device *dev);

struct net_device *netdev_get_xmit_slave(struct net_device *dev,
					 struct sk_buff *skb,
					 bool all_slaves);
struct net_device *netdev_sk_get_lowest_dev(struct net_device *dev,
					    struct sock *sk);
struct net_device *dev_get_by_index(struct net *net, int ifindex);
struct net_device *__dev_get_by_index(struct net *net, int ifindex);
struct net_device *netdev_get_by_index(struct net *net, int ifindex,
				       netdevice_tracker *tracker, gfp_t gfp);
struct net_device *netdev_get_by_name(struct net *net, const char *name,
				      netdevice_tracker *tracker, gfp_t gfp);
struct net_device *netdev_get_by_flags_rcu(struct net *net, netdevice_tracker *tracker,
					   unsigned short flags, unsigned short mask);
struct net_device *dev_get_by_index_rcu(struct net *net, int ifindex);
void netdev_copy_name(struct net_device *dev, char *name);

static inline int dev_hard_header(struct sk_buff *skb, struct net_device *dev,
				  unsigned short type,
				  const void *daddr, const void *saddr,
				  unsigned int len)
{
	if (!dev->header_ops || !dev->header_ops->create)
		return 0;

	return dev->header_ops->create(skb, dev, type, daddr, saddr, len);
}

static inline int dev_parse_header(const struct sk_buff *skb,
				   unsigned char *haddr)
{
	const struct net_device *dev = skb->dev;

	if (!dev->header_ops || !dev->header_ops->parse)
		return 0;
	return dev->header_ops->parse(skb, haddr);
}

static inline __be16 dev_parse_header_protocol(const struct sk_buff *skb)
{
	const struct net_device *dev = skb->dev;

	if (!dev->header_ops || !dev->header_ops->parse_protocol)
		return 0;
	return dev->header_ops->parse_protocol(skb);
}

/* ll_header must have at least hard_header_len allocated */
static inline bool dev_validate_header(const struct net_device *dev,
				       char *ll_header, int len)
{
	if (likely(len >= dev->hard_header_len))
		return true;
	if (len < dev->min_header_len)
		return false;

	if (capable(CAP_SYS_RAWIO)) {
		memset(ll_header + len, 0, dev->hard_header_len - len);
		return true;
	}

	if (dev->header_ops && dev->header_ops->validate)
		return dev->header_ops->validate(ll_header, len);

	return false;
}

static inline bool dev_has_header(const struct net_device *dev)
{
	return dev->header_ops && dev->header_ops->create;
}

/*
 * Incoming packets are placed on per-CPU queues
 */
struct softnet_data {
	struct list_head	poll_list;
	struct sk_buff_head	process_queue;
	local_lock_t		process_queue_bh_lock;

	/* stats */
	unsigned int		processed;
	unsigned int		time_squeeze;
#ifdef CONFIG_RPS
	struct softnet_data	*rps_ipi_list;
#endif

	unsigned int		received_rps;
	bool			in_net_rx_action;
	bool			in_napi_threaded_poll;

#ifdef CONFIG_NET_FLOW_LIMIT
	struct sd_flow_limit __rcu *flow_limit;
#endif
	struct Qdisc		*output_queue;
	struct Qdisc		**output_queue_tailp;
	struct sk_buff		*completion_queue;
#ifdef CONFIG_XFRM_OFFLOAD
	struct sk_buff_head	xfrm_backlog;
#endif
	/* written and read only by owning cpu: */
	struct netdev_xmit xmit;
#ifdef CONFIG_RPS
	/* input_queue_head should be written by cpu owning this struct,
	 * and only read by other cpus. Worth using a cache line.
	 */
	unsigned int		input_queue_head ____cacheline_aligned_in_smp;

	/* Elements below can be accessed between CPUs for RPS/RFS */
	call_single_data_t	csd ____cacheline_aligned_in_smp;
	struct softnet_data	*rps_ipi_next;
	unsigned int		cpu;
	unsigned int		input_queue_tail;
#endif
	struct sk_buff_head	input_pkt_queue;
	struct napi_struct	backlog;

	atomic_t		dropped ____cacheline_aligned_in_smp;

	/* Another possibly contended cache line */
	spinlock_t		defer_lock ____cacheline_aligned_in_smp;
	int			defer_count;
	int			defer_ipi_scheduled;
	struct sk_buff		*defer_list;
	call_single_data_t	defer_csd;
};

DECLARE_PER_CPU_ALIGNED(struct softnet_data, softnet_data);

struct page_pool_bh {
	struct page_pool *pool;
	local_lock_t bh_lock;
};
DECLARE_PER_CPU(struct page_pool_bh, system_page_pool);

#ifndef CONFIG_PREEMPT_RT
static inline int dev_recursion_level(void)
{
	return this_cpu_read(softnet_data.xmit.recursion);
}
#else
static inline int dev_recursion_level(void)
{
	return current->net_xmit.recursion;
}

#endif

void __netif_schedule(struct Qdisc *q);
void netif_schedule_queue(struct netdev_queue *txq);

static inline void netif_tx_schedule_all(struct net_device *dev)
{
	unsigned int i;

	for (i = 0; i < dev->num_tx_queues; i++)
		netif_schedule_queue(netdev_get_tx_queue(dev, i));
}

static __always_inline void netif_tx_start_queue(struct netdev_queue *dev_queue)
{
	clear_bit(__QUEUE_STATE_DRV_XOFF, &dev_queue->state);
}

/**
 *	netif_start_queue - allow transmit
 *	@dev: network device
 *
 *	Allow upper layers to call the device hard_start_xmit routine.
 */
static inline void netif_start_queue(struct net_device *dev)
{
	netif_tx_start_queue(netdev_get_tx_queue(dev, 0));
}

static inline void netif_tx_start_all_queues(struct net_device *dev)
{
	unsigned int i;

	for (i = 0; i < dev->num_tx_queues; i++) {
		struct netdev_queue *txq = netdev_get_tx_queue(dev, i);
		netif_tx_start_queue(txq);
	}
}

void netif_tx_wake_queue(struct netdev_queue *dev_queue);

/**
 *	netif_wake_queue - restart transmit
 *	@dev: network device
 *
 *	Allow upper layers to call the device hard_start_xmit routine.
 *	Used for flow control when transmit resources are available.
 */
static inline void netif_wake_queue(struct net_device *dev)
{
	netif_tx_wake_queue(netdev_get_tx_queue(dev, 0));
}

static inline void netif_tx_wake_all_queues(struct net_device *dev)
{
	unsigned int i;

	for (i = 0; i < dev->num_tx_queues; i++) {
		struct netdev_queue *txq = netdev_get_tx_queue(dev, i);
		netif_tx_wake_queue(txq);
	}
}

static __always_inline void netif_tx_stop_queue(struct netdev_queue *dev_queue)
{
	/* Paired with READ_ONCE() from dev_watchdog() */
	WRITE_ONCE(dev_queue->trans_start, jiffies);

	/* This barrier is paired with smp_mb() from dev_watchdog() */
	smp_mb__before_atomic();

	/* Must be an atomic op see netif_txq_try_stop() */
	set_bit(__QUEUE_STATE_DRV_XOFF, &dev_queue->state);
}

/**
 *	netif_stop_queue - stop transmitted packets
 *	@dev: network device
 *
 *	Stop upper layers calling the device hard_start_xmit routine.
 *	Used for flow control when transmit resources are unavailable.
 */
static inline void netif_stop_queue(struct net_device *dev)
{
	netif_tx_stop_queue(netdev_get_tx_queue(dev, 0));
}

void netif_tx_stop_all_queues(struct net_device *dev);

static inline bool netif_tx_queue_stopped(const struct netdev_queue *dev_queue)
{
	return test_bit(__QUEUE_STATE_DRV_XOFF, &dev_queue->state);
}

/**
 *	netif_queue_stopped - test if transmit queue is flowblocked
 *	@dev: network device
 *
 *	Test if transmit queue on device is currently unable to send.
 */
static inline bool netif_queue_stopped(const struct net_device *dev)
{
	return netif_tx_queue_stopped(netdev_get_tx_queue(dev, 0));
}

static inline bool netif_xmit_stopped(const struct netdev_queue *dev_queue)
{
	return dev_queue->state & QUEUE_STATE_ANY_XOFF;
}

static inline bool
netif_xmit_frozen_or_stopped(const struct netdev_queue *dev_queue)
{
	return dev_queue->state & QUEUE_STATE_ANY_XOFF_OR_FROZEN;
}

static inline bool
netif_xmit_frozen_or_drv_stopped(const struct netdev_queue *dev_queue)
{
	return dev_queue->state & QUEUE_STATE_DRV_XOFF_OR_FROZEN;
}

/**
 *	netdev_queue_set_dql_min_limit - set dql minimum limit
 *	@dev_queue: pointer to transmit queue
 *	@min_limit: dql minimum limit
 *
 * Forces xmit_more() to return true until the minimum threshold
 * defined by @min_limit is reached (or until the tx queue is
 * empty). Warning: to be use with care, misuse will impact the
 * latency.
 */
static inline void netdev_queue_set_dql_min_limit(struct netdev_queue *dev_queue,
						  unsigned int min_limit)
{
#ifdef CONFIG_BQL
	dev_queue->dql.min_limit = min_limit;
#endif
}

static inline int netdev_queue_dql_avail(const struct netdev_queue *txq)
{
#ifdef CONFIG_BQL
	/* Non-BQL migrated drivers will return 0, too. */
	return dql_avail(&txq->dql);
#else
	return 0;
#endif
}

/**
 *	netdev_txq_bql_enqueue_prefetchw - prefetch bql data for write
 *	@dev_queue: pointer to transmit queue
 *
 * BQL enabled drivers might use this helper in their ndo_start_xmit(),
 * to give appropriate hint to the CPU.
 */
static inline void netdev_txq_bql_enqueue_prefetchw(struct netdev_queue *dev_queue)
{
#ifdef CONFIG_BQL
	prefetchw(&dev_queue->dql.num_queued);
#endif
}

/**
 *	netdev_txq_bql_complete_prefetchw - prefetch bql data for write
 *	@dev_queue: pointer to transmit queue
 *
 * BQL enabled drivers might use this helper in their TX completion path,
 * to give appropriate hint to the CPU.
 */
static inline void netdev_txq_bql_complete_prefetchw(struct netdev_queue *dev_queue)
{
#ifdef CONFIG_BQL
	prefetchw(&dev_queue->dql.limit);
#endif
}

/**
 *	netdev_tx_sent_queue - report the number of bytes queued to a given tx queue
 *	@dev_queue: network device queue
 *	@bytes: number of bytes queued to the device queue
 *
 *	Report the number of bytes queued for sending/completion to the network
 *	device hardware queue. @bytes should be a good approximation and should
 *	exactly match netdev_completed_queue() @bytes.
 *	This is typically called once per packet, from ndo_start_xmit().
 */
static inline void netdev_tx_sent_queue(struct netdev_queue *dev_queue,
					unsigned int bytes)
{
#ifdef CONFIG_BQL
	dql_queued(&dev_queue->dql, bytes);

	if (likely(dql_avail(&dev_queue->dql) >= 0))
		return;

	/* Paired with READ_ONCE() from dev_watchdog() */
	WRITE_ONCE(dev_queue->trans_start, jiffies);

	/* This barrier is paired with smp_mb() from dev_watchdog() */
	smp_mb__before_atomic();

	set_bit(__QUEUE_STATE_STACK_XOFF, &dev_queue->state);

	/*
	 * The XOFF flag must be set before checking the dql_avail below,
	 * because in netdev_tx_completed_queue we update the dql_completed
	 * before checking the XOFF flag.
	 */
	smp_mb__after_atomic();

	/* check again in case another CPU has just made room avail */
	if (unlikely(dql_avail(&dev_queue->dql) >= 0))
		clear_bit(__QUEUE_STATE_STACK_XOFF, &dev_queue->state);
#endif
}

/* Variant of netdev_tx_sent_queue() for drivers that are aware
 * that they should not test BQL status themselves.
 * We do want to change __QUEUE_STATE_STACK_XOFF only for the last
 * skb of a batch.
 * Returns true if the doorbell must be used to kick the NIC.
 */
static inline bool __netdev_tx_sent_queue(struct netdev_queue *dev_queue,
					  unsigned int bytes,
					  bool xmit_more)
{
	if (xmit_more) {
#ifdef CONFIG_BQL
		dql_queued(&dev_queue->dql, bytes);
#endif
		return netif_tx_queue_stopped(dev_queue);
	}
	netdev_tx_sent_queue(dev_queue, bytes);
	return true;
}

/**
 *	netdev_sent_queue - report the number of bytes queued to hardware
 *	@dev: network device
 *	@bytes: number of bytes queued to the hardware device queue
 *
 *	Report the number of bytes queued for sending/completion to the network
 *	device hardware queue#0. @bytes should be a good approximation and should
 *	exactly match netdev_completed_queue() @bytes.
 *	This is typically called once per packet, from ndo_start_xmit().
 */
static inline void netdev_sent_queue(struct net_device *dev, unsigned int bytes)
{
	netdev_tx_sent_queue(netdev_get_tx_queue(dev, 0), bytes);
}

static inline bool __netdev_sent_queue(struct net_device *dev,
				       unsigned int bytes,
				       bool xmit_more)
{
	return __netdev_tx_sent_queue(netdev_get_tx_queue(dev, 0), bytes,
				      xmit_more);
}

/**
 *	netdev_tx_completed_queue - report number of packets/bytes at TX completion.
 *	@dev_queue: network device queue
 *	@pkts: number of packets (currently ignored)
 *	@bytes: number of bytes dequeued from the device queue
 *
 *	Must be called at most once per TX completion round (and not per
 *	individual packet), so that BQL can adjust its limits appropriately.
 */
static inline void netdev_tx_completed_queue(struct netdev_queue *dev_queue,
					     unsigned int pkts, unsigned int bytes)
{
#ifdef CONFIG_BQL
	if (unlikely(!bytes))
		return;

	dql_completed(&dev_queue->dql, bytes);

	/*
	 * Without the memory barrier there is a small possibility that
	 * netdev_tx_sent_queue will miss the update and cause the queue to
	 * be stopped forever
	 */
	smp_mb(); /* NOTE: netdev_txq_completed_mb() assumes this exists */

	if (unlikely(dql_avail(&dev_queue->dql) < 0))
		return;

	if (test_and_clear_bit(__QUEUE_STATE_STACK_XOFF, &dev_queue->state))
		netif_schedule_queue(dev_queue);
#endif
}

/**
 * 	netdev_completed_queue - report bytes and packets completed by device
 * 	@dev: network device
 * 	@pkts: actual number of packets sent over the medium
 * 	@bytes: actual number of bytes sent over the medium
 *
 * 	Report the number of bytes and packets transmitted by the network device
 * 	hardware queue over the physical medium, @bytes must exactly match the
 * 	@bytes amount passed to netdev_sent_queue()
 */
static inline void netdev_completed_queue(struct net_device *dev,
					  unsigned int pkts, unsigned int bytes)
{
	netdev_tx_completed_queue(netdev_get_tx_queue(dev, 0), pkts, bytes);
}

static inline void netdev_tx_reset_queue(struct netdev_queue *q)
{
#ifdef CONFIG_BQL
	clear_bit(__QUEUE_STATE_STACK_XOFF, &q->state);
	dql_reset(&q->dql);
#endif
}

/**
 * netdev_tx_reset_subqueue - reset the BQL stats and state of a netdev queue
 * @dev: network device
 * @qid: stack index of the queue to reset
 */
static inline void netdev_tx_reset_subqueue(const struct net_device *dev,
					    u32 qid)
{
	netdev_tx_reset_queue(netdev_get_tx_queue(dev, qid));
}

/**
 * 	netdev_reset_queue - reset the packets and bytes count of a network device
 * 	@dev_queue: network device
 *
 * 	Reset the bytes and packet count of a network device and clear the
 * 	software flow control OFF bit for this network device
 */
static inline void netdev_reset_queue(struct net_device *dev_queue)
{
	netdev_tx_reset_subqueue(dev_queue, 0);
}

/**
 * 	netdev_cap_txqueue - check if selected tx queue exceeds device queues
 * 	@dev: network device
 * 	@queue_index: given tx queue index
 *
 * 	Returns 0 if given tx queue index >= number of device tx queues,
 * 	otherwise returns the originally passed tx queue index.
 */
static inline u16 netdev_cap_txqueue(struct net_device *dev, u16 queue_index)
{
	if (unlikely(queue_index >= dev->real_num_tx_queues)) {
		net_warn_ratelimited("%s selects TX queue %d, but real number of TX queues is %d\n",
				     dev->name, queue_index,
				     dev->real_num_tx_queues);
		return 0;
	}

	return queue_index;
}

/**
 *	netif_running - test if up
 *	@dev: network device
 *
 *	Test if the device has been brought up.
 */
static inline bool netif_running(const struct net_device *dev)
{
	return test_bit(__LINK_STATE_START, &dev->state);
}

/*
 * Routines to manage the subqueues on a device.  We only need start,
 * stop, and a check if it's stopped.  All other device management is
 * done at the overall netdevice level.
 * Also test the device if we're multiqueue.
 */

/**
 *	netif_start_subqueue - allow sending packets on subqueue
 *	@dev: network device
 *	@queue_index: sub queue index
 *
 * Start individual transmit queue of a device with multiple transmit queues.
 */
static inline void netif_start_subqueue(struct net_device *dev, u16 queue_index)
{
	struct netdev_queue *txq = netdev_get_tx_queue(dev, queue_index);

	netif_tx_start_queue(txq);
}

/**
 *	netif_stop_subqueue - stop sending packets on subqueue
 *	@dev: network device
 *	@queue_index: sub queue index
 *
 * Stop individual transmit queue of a device with multiple transmit queues.
 */
static inline void netif_stop_subqueue(struct net_device *dev, u16 queue_index)
{
	struct netdev_queue *txq = netdev_get_tx_queue(dev, queue_index);
	netif_tx_stop_queue(txq);
}

/**
 *	__netif_subqueue_stopped - test status of subqueue
 *	@dev: network device
 *	@queue_index: sub queue index
 *
 * Check individual transmit queue of a device with multiple transmit queues.
 */
static inline bool __netif_subqueue_stopped(const struct net_device *dev,
					    u16 queue_index)
{
	struct netdev_queue *txq = netdev_get_tx_queue(dev, queue_index);

	return netif_tx_queue_stopped(txq);
}

/**
 *	netif_subqueue_stopped - test status of subqueue
 *	@dev: network device
 *	@skb: sub queue buffer pointer
 *
 * Check individual transmit queue of a device with multiple transmit queues.
 */
static inline bool netif_subqueue_stopped(const struct net_device *dev,
					  struct sk_buff *skb)
{
	return __netif_subqueue_stopped(dev, skb_get_queue_mapping(skb));
}

/**
 *	netif_wake_subqueue - allow sending packets on subqueue
 *	@dev: network device
 *	@queue_index: sub queue index
 *
 * Resume individual transmit queue of a device with multiple transmit queues.
 */
static inline void netif_wake_subqueue(struct net_device *dev, u16 queue_index)
{
	struct netdev_queue *txq = netdev_get_tx_queue(dev, queue_index);

	netif_tx_wake_queue(txq);
}

#ifdef CONFIG_XPS
int netif_set_xps_queue(struct net_device *dev, const struct cpumask *mask,
			u16 index);
int __netif_set_xps_queue(struct net_device *dev, const unsigned long *mask,
			  u16 index, enum xps_map_type type);

/**
 *	netif_attr_test_mask - Test a CPU or Rx queue set in a mask
 *	@j: CPU/Rx queue index
 *	@mask: bitmask of all cpus/rx queues
 *	@nr_bits: number of bits in the bitmask
 *
 * Test if a CPU or Rx queue index is set in a mask of all CPU/Rx queues.
 */
static inline bool netif_attr_test_mask(unsigned long j,
					const unsigned long *mask,
					unsigned int nr_bits)
{
	cpu_max_bits_warn(j, nr_bits);
	return test_bit(j, mask);
}

/**
 *	netif_attr_test_online - Test for online CPU/Rx queue
 *	@j: CPU/Rx queue index
 *	@online_mask: bitmask for CPUs/Rx queues that are online
 *	@nr_bits: number of bits in the bitmask
 *
 * Returns: true if a CPU/Rx queue is online.
 */
static inline bool netif_attr_test_online(unsigned long j,
					  const unsigned long *online_mask,
					  unsigned int nr_bits)
{
	cpu_max_bits_warn(j, nr_bits);

	if (online_mask)
		return test_bit(j, online_mask);

	return (j < nr_bits);
}

/**
 *	netif_attrmask_next - get the next CPU/Rx queue in a cpu/Rx queues mask
 *	@n: CPU/Rx queue index
 *	@srcp: the cpumask/Rx queue mask pointer
 *	@nr_bits: number of bits in the bitmask
 *
 * Returns: next (after n) CPU/Rx queue index in the mask;
 * >= nr_bits if no further CPUs/Rx queues set.
 */
static inline unsigned int netif_attrmask_next(int n, const unsigned long *srcp,
					       unsigned int nr_bits)
{
	/* -1 is a legal arg here. */
	if (n != -1)
		cpu_max_bits_warn(n, nr_bits);

	if (srcp)
		return find_next_bit(srcp, nr_bits, n + 1);

	return n + 1;
}

/**
 *	netif_attrmask_next_and - get the next CPU/Rx queue in \*src1p & \*src2p
 *	@n: CPU/Rx queue index
 *	@src1p: the first CPUs/Rx queues mask pointer
 *	@src2p: the second CPUs/Rx queues mask pointer
 *	@nr_bits: number of bits in the bitmask
 *
 * Returns: next (after n) CPU/Rx queue index set in both masks;
 * >= nr_bits if no further CPUs/Rx queues set in both.
 */
static inline int netif_attrmask_next_and(int n, const unsigned long *src1p,
					  const unsigned long *src2p,
					  unsigned int nr_bits)
{
	/* -1 is a legal arg here. */
	if (n != -1)
		cpu_max_bits_warn(n, nr_bits);

	if (src1p && src2p)
		return find_next_and_bit(src1p, src2p, nr_bits, n + 1);
	else if (src1p)
		return find_next_bit(src1p, nr_bits, n + 1);
	else if (src2p)
		return find_next_bit(src2p, nr_bits, n + 1);

	return n + 1;
}
#else
static inline int netif_set_xps_queue(struct net_device *dev,
				      const struct cpumask *mask,
				      u16 index)
{
	return 0;
}

static inline int __netif_set_xps_queue(struct net_device *dev,
					const unsigned long *mask,
					u16 index, enum xps_map_type type)
{
	return 0;
}
#endif

/**
 *	netif_is_multiqueue - test if device has multiple transmit queues
 *	@dev: network device
 *
 * Check if device has multiple transmit queues
 */
static inline bool netif_is_multiqueue(const struct net_device *dev)
{
	return dev->num_tx_queues > 1;
}

int netif_set_real_num_tx_queues(struct net_device *dev, unsigned int txq);
int netif_set_real_num_rx_queues(struct net_device *dev, unsigned int rxq);
int netif_set_real_num_queues(struct net_device *dev,
			      unsigned int txq, unsigned int rxq);

int netif_get_num_default_rss_queues(void);

void dev_kfree_skb_irq_reason(struct sk_buff *skb, enum skb_drop_reason reason);
void dev_kfree_skb_any_reason(struct sk_buff *skb, enum skb_drop_reason reason);

/*
 * It is not allowed to call kfree_skb() or consume_skb() from hardware
 * interrupt context or with hardware interrupts being disabled.
 * (in_hardirq() || irqs_disabled())
 *
 * We provide four helpers that can be used in following contexts :
 *
 * dev_kfree_skb_irq(skb) when caller drops a packet from irq context,
 *  replacing kfree_skb(skb)
 *
 * dev_consume_skb_irq(skb) when caller consumes a packet from irq context.
 *  Typically used in place of consume_skb(skb) in TX completion path
 *
 * dev_kfree_skb_any(skb) when caller doesn't know its current irq context,
 *  replacing kfree_skb(skb)
 *
 * dev_consume_skb_any(skb) when caller doesn't know its current irq context,
 *  and consumed a packet. Used in place of consume_skb(skb)
 */
static inline void dev_kfree_skb_irq(struct sk_buff *skb)
{
	dev_kfree_skb_irq_reason(skb, SKB_DROP_REASON_NOT_SPECIFIED);
}

static inline void dev_consume_skb_irq(struct sk_buff *skb)
{
	dev_kfree_skb_irq_reason(skb, SKB_CONSUMED);
}

static inline void dev_kfree_skb_any(struct sk_buff *skb)
{
	dev_kfree_skb_any_reason(skb, SKB_DROP_REASON_NOT_SPECIFIED);
}

static inline void dev_consume_skb_any(struct sk_buff *skb)
{
	dev_kfree_skb_any_reason(skb, SKB_CONSUMED);
}

u32 bpf_prog_run_generic_xdp(struct sk_buff *skb, struct xdp_buff *xdp,
			     const struct bpf_prog *xdp_prog);
void generic_xdp_tx(struct sk_buff *skb, const struct bpf_prog *xdp_prog);
int do_xdp_generic(const struct bpf_prog *xdp_prog, struct sk_buff **pskb);
int netif_rx(struct sk_buff *skb);
int __netif_rx(struct sk_buff *skb);

int netif_receive_skb(struct sk_buff *skb);
int netif_receive_skb_core(struct sk_buff *skb);
void netif_receive_skb_list_internal(struct list_head *head);
void netif_receive_skb_list(struct list_head *head);
gro_result_t gro_receive_skb(struct gro_node *gro, struct sk_buff *skb);

static inline gro_result_t napi_gro_receive(struct napi_struct *napi,
					    struct sk_buff *skb)
{
	return gro_receive_skb(&napi->gro, skb);
}

struct sk_buff *napi_get_frags(struct napi_struct *napi);
gro_result_t napi_gro_frags(struct napi_struct *napi);

static inline void napi_free_frags(struct napi_struct *napi)
{
	kfree_skb(napi->skb);
	napi->skb = NULL;
}

bool netdev_is_rx_handler_busy(struct net_device *dev);
int netdev_rx_handler_register(struct net_device *dev,
			       rx_handler_func_t *rx_handler,
			       void *rx_handler_data);
void netdev_rx_handler_unregister(struct net_device *dev);

bool dev_valid_name(const char *name);
static inline bool is_socket_ioctl_cmd(unsigned int cmd)
{
	return _IOC_TYPE(cmd) == SOCK_IOC_TYPE;
}
int get_user_ifreq(struct ifreq *ifr, void __user **ifrdata, void __user *arg);
int put_user_ifreq(struct ifreq *ifr, void __user *arg);
int dev_ioctl(struct net *net, unsigned int cmd, struct ifreq *ifr,
		void __user *data, bool *need_copyout);
int dev_ifconf(struct net *net, struct ifconf __user *ifc);
int dev_eth_ioctl(struct net_device *dev,
		  struct ifreq *ifr, unsigned int cmd);
int generic_hwtstamp_get_lower(struct net_device *dev,
			       struct kernel_hwtstamp_config *kernel_cfg);
int generic_hwtstamp_set_lower(struct net_device *dev,
			       struct kernel_hwtstamp_config *kernel_cfg,
			       struct netlink_ext_ack *extack);
int dev_ethtool(struct net *net, struct ifreq *ifr, void __user *userdata);
unsigned int netif_get_flags(const struct net_device *dev);
int __dev_change_flags(struct net_device *dev, unsigned int flags,
		       struct netlink_ext_ack *extack);
int netif_change_flags(struct net_device *dev, unsigned int flags,
		       struct netlink_ext_ack *extack);
int dev_change_flags(struct net_device *dev, unsigned int flags,
		     struct netlink_ext_ack *extack);
int netif_set_alias(struct net_device *dev, const char *alias, size_t len);
int dev_set_alias(struct net_device *, const char *, size_t);
int dev_get_alias(const struct net_device *, char *, size_t);
int __dev_change_net_namespace(struct net_device *dev, struct net *net,
			       const char *pat, int new_ifindex,
			       struct netlink_ext_ack *extack);
int dev_change_net_namespace(struct net_device *dev, struct net *net,
			     const char *pat);
int __netif_set_mtu(struct net_device *dev, int new_mtu);
int netif_set_mtu(struct net_device *dev, int new_mtu);
int dev_set_mtu(struct net_device *, int);
int netif_pre_changeaddr_notify(struct net_device *dev, const char *addr,
				struct netlink_ext_ack *extack);
int netif_set_mac_address(struct net_device *dev, struct sockaddr_storage *ss,
			  struct netlink_ext_ack *extack);
int dev_set_mac_address(struct net_device *dev, struct sockaddr_storage *ss,
			struct netlink_ext_ack *extack);
int dev_set_mac_address_user(struct net_device *dev, struct sockaddr_storage *ss,
			     struct netlink_ext_ack *extack);
int netif_get_mac_address(struct sockaddr *sa, struct net *net, char *dev_name);
int netif_get_port_parent_id(struct net_device *dev,
			     struct netdev_phys_item_id *ppid, bool recurse);
bool netdev_port_same_parent_id(struct net_device *a, struct net_device *b);

struct sk_buff *validate_xmit_skb_list(struct sk_buff *skb, struct net_device *dev, bool *again);
struct sk_buff *dev_hard_start_xmit(struct sk_buff *skb, struct net_device *dev,
				    struct netdev_queue *txq, int *ret);

int bpf_xdp_link_attach(const union bpf_attr *attr, struct bpf_prog *prog);
u8 dev_xdp_prog_count(struct net_device *dev);
int netif_xdp_propagate(struct net_device *dev, struct netdev_bpf *bpf);
int dev_xdp_propagate(struct net_device *dev, struct netdev_bpf *bpf);
u8 dev_xdp_sb_prog_count(struct net_device *dev);
u32 dev_xdp_prog_id(struct net_device *dev, enum bpf_xdp_mode mode);

u32 dev_get_min_mp_channel_count(const struct net_device *dev);

int __dev_forward_skb(struct net_device *dev, struct sk_buff *skb);
int dev_forward_skb(struct net_device *dev, struct sk_buff *skb);
int dev_forward_skb_nomtu(struct net_device *dev, struct sk_buff *skb);
bool is_skb_forwardable(const struct net_device *dev,
			const struct sk_buff *skb);

static __always_inline bool __is_skb_forwardable(const struct net_device *dev,
						 const struct sk_buff *skb,
						 const bool check_mtu)
{
	const u32 vlan_hdr_len = 4; /* VLAN_HLEN */
	unsigned int len;

	if (!(dev->flags & IFF_UP))
		return false;

	if (!check_mtu)
		return true;

	len = dev->mtu + dev->hard_header_len + vlan_hdr_len;
	if (skb->len <= len)
		return true;

	/* if TSO is enabled, we don't care about the length as the packet
	 * could be forwarded without being segmented before
	 */
	if (skb_is_gso(skb))
		return true;

	return false;
}

void netdev_core_stats_inc(struct net_device *dev, u32 offset);

#define DEV_CORE_STATS_INC(FIELD)						\
static inline void dev_core_stats_##FIELD##_inc(struct net_device *dev)		\
{										\
	netdev_core_stats_inc(dev,						\
			offsetof(struct net_device_core_stats, FIELD));		\
}
DEV_CORE_STATS_INC(rx_dropped)
DEV_CORE_STATS_INC(tx_dropped)
DEV_CORE_STATS_INC(rx_nohandler)
DEV_CORE_STATS_INC(rx_otherhost_dropped)
#undef DEV_CORE_STATS_INC

static __always_inline int ____dev_forward_skb(struct net_device *dev,
					       struct sk_buff *skb,
					       const bool check_mtu)
{
	if (skb_orphan_frags(skb, GFP_ATOMIC) ||
	    unlikely(!__is_skb_forwardable(dev, skb, check_mtu))) {
		dev_core_stats_rx_dropped_inc(dev);
		kfree_skb(skb);
		return NET_RX_DROP;
	}

	skb_scrub_packet(skb, !net_eq(dev_net(dev), dev_net(skb->dev)));
	skb->priority = 0;
	return 0;
}

bool dev_nit_active_rcu(const struct net_device *dev);
static inline bool dev_nit_active(const struct net_device *dev)
{
	bool ret;

	rcu_read_lock();
	ret = dev_nit_active_rcu(dev);
	rcu_read_unlock();
	return ret;
}

void dev_queue_xmit_nit(struct sk_buff *skb, struct net_device *dev);

static inline void __dev_put(struct net_device *dev)
{
	if (dev) {
#ifdef CONFIG_PCPU_DEV_REFCNT
		this_cpu_dec(*dev->pcpu_refcnt);
#else
		refcount_dec(&dev->dev_refcnt);
#endif
	}
}

static inline void __dev_hold(struct net_device *dev)
{
	if (dev) {
#ifdef CONFIG_PCPU_DEV_REFCNT
		this_cpu_inc(*dev->pcpu_refcnt);
#else
		refcount_inc(&dev->dev_refcnt);
#endif
	}
}

static inline void __netdev_tracker_alloc(struct net_device *dev,
					  netdevice_tracker *tracker,
					  gfp_t gfp)
{
#ifdef CONFIG_NET_DEV_REFCNT_TRACKER
	ref_tracker_alloc(&dev->refcnt_tracker, tracker, gfp);
#endif
}

/* netdev_tracker_alloc() can upgrade a prior untracked reference
 * taken by dev_get_by_name()/dev_get_by_index() to a tracked one.
 */
static inline void netdev_tracker_alloc(struct net_device *dev,
					netdevice_tracker *tracker, gfp_t gfp)
{
#ifdef CONFIG_NET_DEV_REFCNT_TRACKER
	refcount_dec(&dev->refcnt_tracker.no_tracker);
	__netdev_tracker_alloc(dev, tracker, gfp);
#endif
}

static inline void netdev_tracker_free(struct net_device *dev,
				       netdevice_tracker *tracker)
{
#ifdef CONFIG_NET_DEV_REFCNT_TRACKER
	ref_tracker_free(&dev->refcnt_tracker, tracker);
#endif
}

static inline void netdev_hold(struct net_device *dev,
			       netdevice_tracker *tracker, gfp_t gfp)
{
	if (dev) {
		__dev_hold(dev);
		__netdev_tracker_alloc(dev, tracker, gfp);
	}
}

static inline void netdev_put(struct net_device *dev,
			      netdevice_tracker *tracker)
{
	if (dev) {
		netdev_tracker_free(dev, tracker);
		__dev_put(dev);
	}
}

/**
 *	dev_hold - get reference to device
 *	@dev: network device
 *
 * Hold reference to device to keep it from being freed.
 * Try using netdev_hold() instead.
 */
static inline void dev_hold(struct net_device *dev)
{
	netdev_hold(dev, NULL, GFP_ATOMIC);
}

/**
 *	dev_put - release reference to device
 *	@dev: network device
 *
 * Release reference to device to allow it to be freed.
 * Try using netdev_put() instead.
 */
static inline void dev_put(struct net_device *dev)
{
	netdev_put(dev, NULL);
}

DEFINE_FREE(dev_put, struct net_device *, if (_T) dev_put(_T))

static inline void netdev_ref_replace(struct net_device *odev,
				      struct net_device *ndev,
				      netdevice_tracker *tracker,
				      gfp_t gfp)
{
	if (odev)
		netdev_tracker_free(odev, tracker);

	__dev_hold(ndev);
	__dev_put(odev);

	if (ndev)
		__netdev_tracker_alloc(ndev, tracker, gfp);
}

/* Carrier loss detection, dial on demand. The functions netif_carrier_on
 * and _off may be called from IRQ context, but it is caller
 * who is responsible for serialization of these calls.
 *
 * The name carrier is inappropriate, these functions should really be
 * called netif_lowerlayer_*() because they represent the state of any
 * kind of lower layer not just hardware media.
 */
void linkwatch_fire_event(struct net_device *dev);

/**
 * linkwatch_sync_dev - sync linkwatch for the given device
 * @dev: network device to sync linkwatch for
 *
 * Sync linkwatch for the given device, removing it from the
 * pending work list (if queued).
 */
void linkwatch_sync_dev(struct net_device *dev);
void __linkwatch_sync_dev(struct net_device *dev);

/**
 *	netif_carrier_ok - test if carrier present
 *	@dev: network device
 *
 * Check if carrier is present on device
 */
static inline bool netif_carrier_ok(const struct net_device *dev)
{
	return !test_bit(__LINK_STATE_NOCARRIER, &dev->state);
}

unsigned long dev_trans_start(struct net_device *dev);

void netdev_watchdog_up(struct net_device *dev);

void netif_carrier_on(struct net_device *dev);
void netif_carrier_off(struct net_device *dev);
void netif_carrier_event(struct net_device *dev);

/**
 *	netif_dormant_on - mark device as dormant.
 *	@dev: network device
 *
 * Mark device as dormant (as per RFC2863).
 *
 * The dormant state indicates that the relevant interface is not
 * actually in a condition to pass packets (i.e., it is not 'up') but is
 * in a "pending" state, waiting for some external event.  For "on-
 * demand" interfaces, this new state identifies the situation where the
 * interface is waiting for events to place it in the up state.
 */
static inline void netif_dormant_on(struct net_device *dev)
{
	if (!test_and_set_bit(__LINK_STATE_DORMANT, &dev->state))
		linkwatch_fire_event(dev);
}

/**
 *	netif_dormant_off - set device as not dormant.
 *	@dev: network device
 *
 * Device is not in dormant state.
 */
static inline void netif_dormant_off(struct net_device *dev)
{
	if (test_and_clear_bit(__LINK_STATE_DORMANT, &dev->state))
		linkwatch_fire_event(dev);
}

/**
 *	netif_dormant - test if device is dormant
 *	@dev: network device
 *
 * Check if device is dormant.
 */
static inline bool netif_dormant(const struct net_device *dev)
{
	return test_bit(__LINK_STATE_DORMANT, &dev->state);
}


/**
 *	netif_testing_on - mark device as under test.
 *	@dev: network device
 *
 * Mark device as under test (as per RFC2863).
 *
 * The testing state indicates that some test(s) must be performed on
 * the interface. After completion, of the test, the interface state
 * will change to up, dormant, or down, as appropriate.
 */
static inline void netif_testing_on(struct net_device *dev)
{
	if (!test_and_set_bit(__LINK_STATE_TESTING, &dev->state))
		linkwatch_fire_event(dev);
}

/**
 *	netif_testing_off - set device as not under test.
 *	@dev: network device
 *
 * Device is not in testing state.
 */
static inline void netif_testing_off(struct net_device *dev)
{
	if (test_and_clear_bit(__LINK_STATE_TESTING, &dev->state))
		linkwatch_fire_event(dev);
}

/**
 *	netif_testing - test if device is under test
 *	@dev: network device
 *
 * Check if device is under test
 */
static inline bool netif_testing(const struct net_device *dev)
{
	return test_bit(__LINK_STATE_TESTING, &dev->state);
}


/**
 *	netif_oper_up - test if device is operational
 *	@dev: network device
 *
 * Check if carrier is operational
 */
static inline bool netif_oper_up(const struct net_device *dev)
{
	unsigned int operstate = READ_ONCE(dev->operstate);

	return	operstate == IF_OPER_UP ||
		operstate == IF_OPER_UNKNOWN /* backward compat */;
}

/**
 *	netif_device_present - is device available or removed
 *	@dev: network device
 *
 * Check if device has not been removed from system.
 */
static inline bool netif_device_present(const struct net_device *dev)
{
	return test_bit(__LINK_STATE_PRESENT, &dev->state);
}

void netif_device_detach(struct net_device *dev);

void netif_device_attach(struct net_device *dev);

/*
 * Network interface message level settings
 */

enum {
	NETIF_MSG_DRV_BIT,
	NETIF_MSG_PROBE_BIT,
	NETIF_MSG_LINK_BIT,
	NETIF_MSG_TIMER_BIT,
	NETIF_MSG_IFDOWN_BIT,
	NETIF_MSG_IFUP_BIT,
	NETIF_MSG_RX_ERR_BIT,
	NETIF_MSG_TX_ERR_BIT,
	NETIF_MSG_TX_QUEUED_BIT,
	NETIF_MSG_INTR_BIT,
	NETIF_MSG_TX_DONE_BIT,
	NETIF_MSG_RX_STATUS_BIT,
	NETIF_MSG_PKTDATA_BIT,
	NETIF_MSG_HW_BIT,
	NETIF_MSG_WOL_BIT,

	/* When you add a new bit above, update netif_msg_class_names array
	 * in net/ethtool/common.c
	 */
	NETIF_MSG_CLASS_COUNT,
};
/* Both ethtool_ops interface and internal driver implementation use u32 */
static_assert(NETIF_MSG_CLASS_COUNT <= 32);

#define __NETIF_MSG_BIT(bit)	((u32)1 << (bit))
#define __NETIF_MSG(name)	__NETIF_MSG_BIT(NETIF_MSG_ ## name ## _BIT)

#define NETIF_MSG_DRV		__NETIF_MSG(DRV)
#define NETIF_MSG_PROBE		__NETIF_MSG(PROBE)
#define NETIF_MSG_LINK		__NETIF_MSG(LINK)
#define NETIF_MSG_TIMER		__NETIF_MSG(TIMER)
#define NETIF_MSG_IFDOWN	__NETIF_MSG(IFDOWN)
#define NETIF_MSG_IFUP		__NETIF_MSG(IFUP)
#define NETIF_MSG_RX_ERR	__NETIF_MSG(RX_ERR)
#define NETIF_MSG_TX_ERR	__NETIF_MSG(TX_ERR)
#define NETIF_MSG_TX_QUEUED	__NETIF_MSG(TX_QUEUED)
#define NETIF_MSG_INTR		__NETIF_MSG(INTR)
#define NETIF_MSG_TX_DONE	__NETIF_MSG(TX_DONE)
#define NETIF_MSG_RX_STATUS	__NETIF_MSG(RX_STATUS)
#define NETIF_MSG_PKTDATA	__NETIF_MSG(PKTDATA)
#define NETIF_MSG_HW		__NETIF_MSG(HW)
#define NETIF_MSG_WOL		__NETIF_MSG(WOL)

#define netif_msg_drv(p)	((p)->msg_enable & NETIF_MSG_DRV)
#define netif_msg_probe(p)	((p)->msg_enable & NETIF_MSG_PROBE)
#define netif_msg_link(p)	((p)->msg_enable & NETIF_MSG_LINK)
#define netif_msg_timer(p)	((p)->msg_enable & NETIF_MSG_TIMER)
#define netif_msg_ifdown(p)	((p)->msg_enable & NETIF_MSG_IFDOWN)
#define netif_msg_ifup(p)	((p)->msg_enable & NETIF_MSG_IFUP)
#define netif_msg_rx_err(p)	((p)->msg_enable & NETIF_MSG_RX_ERR)
#define netif_msg_tx_err(p)	((p)->msg_enable & NETIF_MSG_TX_ERR)
#define netif_msg_tx_queued(p)	((p)->msg_enable & NETIF_MSG_TX_QUEUED)
#define netif_msg_intr(p)	((p)->msg_enable & NETIF_MSG_INTR)
#define netif_msg_tx_done(p)	((p)->msg_enable & NETIF_MSG_TX_DONE)
#define netif_msg_rx_status(p)	((p)->msg_enable & NETIF_MSG_RX_STATUS)
#define netif_msg_pktdata(p)	((p)->msg_enable & NETIF_MSG_PKTDATA)
#define netif_msg_hw(p)		((p)->msg_enable & NETIF_MSG_HW)
#define netif_msg_wol(p)	((p)->msg_enable & NETIF_MSG_WOL)

static inline u32 netif_msg_init(int debug_value, int default_msg_enable_bits)
{
	/* use default */
	if (debug_value < 0 || debug_value >= (sizeof(u32) * 8))
		return default_msg_enable_bits;
	if (debug_value == 0)	/* no output */
		return 0;
	/* set low N bits */
	return (1U << debug_value) - 1;
}

static inline void __netif_tx_lock(struct netdev_queue *txq, int cpu)
{
	spin_lock(&txq->_xmit_lock);
	/* Pairs with READ_ONCE() in __dev_queue_xmit() */
	WRITE_ONCE(txq->xmit_lock_owner, cpu);
}

static inline bool __netif_tx_acquire(struct netdev_queue *txq)
{
	__acquire(&txq->_xmit_lock);
	return true;
}

static inline void __netif_tx_release(struct netdev_queue *txq)
{
	__release(&txq->_xmit_lock);
}

static inline void __netif_tx_lock_bh(struct netdev_queue *txq)
{
	spin_lock_bh(&txq->_xmit_lock);
	/* Pairs with READ_ONCE() in __dev_queue_xmit() */
	WRITE_ONCE(txq->xmit_lock_owner, smp_processor_id());
}

static inline bool __netif_tx_trylock(struct netdev_queue *txq)
{
	bool ok = spin_trylock(&txq->_xmit_lock);

	if (likely(ok)) {
		/* Pairs with READ_ONCE() in __dev_queue_xmit() */
		WRITE_ONCE(txq->xmit_lock_owner, smp_processor_id());
	}
	return ok;
}

static inline void __netif_tx_unlock(struct netdev_queue *txq)
{
	/* Pairs with READ_ONCE() in __dev_queue_xmit() */
	WRITE_ONCE(txq->xmit_lock_owner, -1);
	spin_unlock(&txq->_xmit_lock);
}

static inline void __netif_tx_unlock_bh(struct netdev_queue *txq)
{
	/* Pairs with READ_ONCE() in __dev_queue_xmit() */
	WRITE_ONCE(txq->xmit_lock_owner, -1);
	spin_unlock_bh(&txq->_xmit_lock);
}

/*
 * txq->trans_start can be read locklessly from dev_watchdog()
 */
static inline void txq_trans_update(const struct net_device *dev,
				    struct netdev_queue *txq)
{
	if (!dev->lltx)
		WRITE_ONCE(txq->trans_start, jiffies);
}

static inline void txq_trans_cond_update(struct netdev_queue *txq)
{
	unsigned long now = jiffies;

	if (READ_ONCE(txq->trans_start) != now)
		WRITE_ONCE(txq->trans_start, now);
}

/* legacy drivers only, netdev_start_xmit() sets txq->trans_start */
static inline void netif_trans_update(struct net_device *dev)
{
	struct netdev_queue *txq = netdev_get_tx_queue(dev, 0);

	txq_trans_cond_update(txq);
}

/**
 *	netif_tx_lock - grab network device transmit lock
 *	@dev: network device
 *
 * Get network device transmit lock
 */
void netif_tx_lock(struct net_device *dev);

static inline void netif_tx_lock_bh(struct net_device *dev)
{
	local_bh_disable();
	netif_tx_lock(dev);
}

void netif_tx_unlock(struct net_device *dev);

static inline void netif_tx_unlock_bh(struct net_device *dev)
{
	netif_tx_unlock(dev);
	local_bh_enable();
}

#define HARD_TX_LOCK(dev, txq, cpu) {			\
	if (!(dev)->lltx) {				\
		__netif_tx_lock(txq, cpu);		\
	} else {					\
		__netif_tx_acquire(txq);		\
	}						\
}

#define HARD_TX_TRYLOCK(dev, txq)			\
	(!(dev)->lltx ?					\
		__netif_tx_trylock(txq) :		\
		__netif_tx_acquire(txq))

#define HARD_TX_UNLOCK(dev, txq) {			\
	if (!(dev)->lltx) {				\
		__netif_tx_unlock(txq);			\
	} else {					\
		__netif_tx_release(txq);		\
	}						\
}

static inline void netif_tx_disable(struct net_device *dev)
{
	unsigned int i;
	int cpu;

	local_bh_disable();
	cpu = smp_processor_id();
	spin_lock(&dev->tx_global_lock);
	for (i = 0; i < dev->num_tx_queues; i++) {
		struct netdev_queue *txq = netdev_get_tx_queue(dev, i);

		__netif_tx_lock(txq, cpu);
		netif_tx_stop_queue(txq);
		__netif_tx_unlock(txq);
	}
	spin_unlock(&dev->tx_global_lock);
	local_bh_enable();
}

static inline void netif_addr_lock(struct net_device *dev)
{
	unsigned char nest_level = 0;

#ifdef CONFIG_LOCKDEP
	nest_level = dev->nested_level;
#endif
	spin_lock_nested(&dev->addr_list_lock, nest_level);
}

static inline void netif_addr_lock_bh(struct net_device *dev)
{
	unsigned char nest_level = 0;

#ifdef CONFIG_LOCKDEP
	nest_level = dev->nested_level;
#endif
	local_bh_disable();
	spin_lock_nested(&dev->addr_list_lock, nest_level);
}

static inline void netif_addr_unlock(struct net_device *dev)
{
	spin_unlock(&dev->addr_list_lock);
}

static inline void netif_addr_unlock_bh(struct net_device *dev)
{
	spin_unlock_bh(&dev->addr_list_lock);
}

/*
 * dev_addrs walker. Should be used only for read access. Call with
 * rcu_read_lock held.
 */
#define for_each_dev_addr(dev, ha) \
		list_for_each_entry_rcu(ha, &dev->dev_addrs.list, list)

/* These functions live elsewhere (drivers/net/net_init.c, but related) */

void ether_setup(struct net_device *dev);

/* Allocate dummy net_device */
struct net_device *alloc_netdev_dummy(int sizeof_priv);

/* Support for loadable net-drivers */
struct net_device *alloc_netdev_mqs(int sizeof_priv, const char *name,
				    unsigned char name_assign_type,
				    void (*setup)(struct net_device *),
				    unsigned int txqs, unsigned int rxqs);
#define alloc_netdev(sizeof_priv, name, name_assign_type, setup) \
	alloc_netdev_mqs(sizeof_priv, name, name_assign_type, setup, 1, 1)

#define alloc_netdev_mq(sizeof_priv, name, name_assign_type, setup, count) \
	alloc_netdev_mqs(sizeof_priv, name, name_assign_type, setup, count, \
			 count)

int register_netdev(struct net_device *dev);
void unregister_netdev(struct net_device *dev);

int devm_register_netdev(struct device *dev, struct net_device *ndev);

/* General hardware address lists handling functions */
int __hw_addr_sync(struct netdev_hw_addr_list *to_list,
		   struct netdev_hw_addr_list *from_list, int addr_len);
int __hw_addr_sync_multiple(struct netdev_hw_addr_list *to_list,
			    struct netdev_hw_addr_list *from_list,
			    int addr_len);
void __hw_addr_unsync(struct netdev_hw_addr_list *to_list,
		      struct netdev_hw_addr_list *from_list, int addr_len);
int __hw_addr_sync_dev(struct netdev_hw_addr_list *list,
		       struct net_device *dev,
		       int (*sync)(struct net_device *, const unsigned char *),
		       int (*unsync)(struct net_device *,
				     const unsigned char *));
int __hw_addr_ref_sync_dev(struct netdev_hw_addr_list *list,
			   struct net_device *dev,
			   int (*sync)(struct net_device *,
				       const unsigned char *, int),
			   int (*unsync)(struct net_device *,
					 const unsigned char *, int));
void __hw_addr_ref_unsync_dev(struct netdev_hw_addr_list *list,
			      struct net_device *dev,
			      int (*unsync)(struct net_device *,
					    const unsigned char *, int));
void __hw_addr_unsync_dev(struct netdev_hw_addr_list *list,
			  struct net_device *dev,
			  int (*unsync)(struct net_device *,
					const unsigned char *));
void __hw_addr_init(struct netdev_hw_addr_list *list);

/* Functions used for device addresses handling */
void dev_addr_mod(struct net_device *dev, unsigned int offset,
		  const void *addr, size_t len);

static inline void
__dev_addr_set(struct net_device *dev, const void *addr, size_t len)
{
	dev_addr_mod(dev, 0, addr, len);
}

static inline void dev_addr_set(struct net_device *dev, const u8 *addr)
{
	__dev_addr_set(dev, addr, dev->addr_len);
}

int dev_addr_add(struct net_device *dev, const unsigned char *addr,
		 unsigned char addr_type);
int dev_addr_del(struct net_device *dev, const unsigned char *addr,
		 unsigned char addr_type);

/* Functions used for unicast addresses handling */
int dev_uc_add(struct net_device *dev, const unsigned char *addr);
int dev_uc_add_excl(struct net_device *dev, const unsigned char *addr);
int dev_uc_del(struct net_device *dev, const unsigned char *addr);
int dev_uc_sync(struct net_device *to, struct net_device *from);
int dev_uc_sync_multiple(struct net_device *to, struct net_device *from);
void dev_uc_unsync(struct net_device *to, struct net_device *from);
void dev_uc_flush(struct net_device *dev);
void dev_uc_init(struct net_device *dev);

/**
 *  __dev_uc_sync - Synchronize device's unicast list
 *  @dev:  device to sync
 *  @sync: function to call if address should be added
 *  @unsync: function to call if address should be removed
 *
 *  Add newly added addresses to the interface, and release
 *  addresses that have been deleted.
 */
static inline int __dev_uc_sync(struct net_device *dev,
				int (*sync)(struct net_device *,
					    const unsigned char *),
				int (*unsync)(struct net_device *,
					      const unsigned char *))
{
	return __hw_addr_sync_dev(&dev->uc, dev, sync, unsync);
}

/**
 *  __dev_uc_unsync - Remove synchronized addresses from device
 *  @dev:  device to sync
 *  @unsync: function to call if address should be removed
 *
 *  Remove all addresses that were added to the device by dev_uc_sync().
 */
static inline void __dev_uc_unsync(struct net_device *dev,
				   int (*unsync)(struct net_device *,
						 const unsigned char *))
{
	__hw_addr_unsync_dev(&dev->uc, dev, unsync);
}

/* Functions used for multicast addresses handling */
int dev_mc_add(struct net_device *dev, const unsigned char *addr);
int dev_mc_add_global(struct net_device *dev, const unsigned char *addr);
int dev_mc_add_excl(struct net_device *dev, const unsigned char *addr);
int dev_mc_del(struct net_device *dev, const unsigned char *addr);
int dev_mc_del_global(struct net_device *dev, const unsigned char *addr);
int dev_mc_sync(struct net_device *to, struct net_device *from);
int dev_mc_sync_multiple(struct net_device *to, struct net_device *from);
void dev_mc_unsync(struct net_device *to, struct net_device *from);
void dev_mc_flush(struct net_device *dev);
void dev_mc_init(struct net_device *dev);

/**
 *  __dev_mc_sync - Synchronize device's multicast list
 *  @dev:  device to sync
 *  @sync: function to call if address should be added
 *  @unsync: function to call if address should be removed
 *
 *  Add newly added addresses to the interface, and release
 *  addresses that have been deleted.
 */
static inline int __dev_mc_sync(struct net_device *dev,
				int (*sync)(struct net_device *,
					    const unsigned char *),
				int (*unsync)(struct net_device *,
					      const unsigned char *))
{
	return __hw_addr_sync_dev(&dev->mc, dev, sync, unsync);
}

/**
 *  __dev_mc_unsync - Remove synchronized addresses from device
 *  @dev:  device to sync
 *  @unsync: function to call if address should be removed
 *
 *  Remove all addresses that were added to the device by dev_mc_sync().
 */
static inline void __dev_mc_unsync(struct net_device *dev,
				   int (*unsync)(struct net_device *,
						 const unsigned char *))
{
	__hw_addr_unsync_dev(&dev->mc, dev, unsync);
}

/* Functions used for secondary unicast and multicast support */
void dev_set_rx_mode(struct net_device *dev);
int netif_set_promiscuity(struct net_device *dev, int inc);
int dev_set_promiscuity(struct net_device *dev, int inc);
int netif_set_allmulti(struct net_device *dev, int inc, bool notify);
int dev_set_allmulti(struct net_device *dev, int inc);
void netif_state_change(struct net_device *dev);
void netdev_state_change(struct net_device *dev);
void __netdev_notify_peers(struct net_device *dev);
void netdev_notify_peers(struct net_device *dev);
void netdev_features_change(struct net_device *dev);
/* Load a device via the kmod */
void dev_load(struct net *net, const char *name);
struct rtnl_link_stats64 *dev_get_stats(struct net_device *dev,
					struct rtnl_link_stats64 *storage);
void netdev_stats_to_stats64(struct rtnl_link_stats64 *stats64,
			     const struct net_device_stats *netdev_stats);
void dev_fetch_sw_netstats(struct rtnl_link_stats64 *s,
			   const struct pcpu_sw_netstats __percpu *netstats);
void dev_get_tstats64(struct net_device *dev, struct rtnl_link_stats64 *s);

enum {
	NESTED_SYNC_IMM_BIT,
	NESTED_SYNC_TODO_BIT,
};

#define __NESTED_SYNC_BIT(bit)	((u32)1 << (bit))
#define __NESTED_SYNC(name)	__NESTED_SYNC_BIT(NESTED_SYNC_ ## name ## _BIT)

#define NESTED_SYNC_IMM		__NESTED_SYNC(IMM)
#define NESTED_SYNC_TODO	__NESTED_SYNC(TODO)

struct netdev_nested_priv {
	unsigned char flags;
	void *data;
};

bool netdev_has_upper_dev(struct net_device *dev, struct net_device *upper_dev);
struct net_device *netdev_upper_get_next_dev_rcu(struct net_device *dev,
						     struct list_head **iter);

/* iterate through upper list, must be called under RCU read lock */
#define netdev_for_each_upper_dev_rcu(dev, updev, iter) \
	for (iter = &(dev)->adj_list.upper, \
	     updev = netdev_upper_get_next_dev_rcu(dev, &(iter)); \
	     updev; \
	     updev = netdev_upper_get_next_dev_rcu(dev, &(iter)))

int netdev_walk_all_upper_dev_rcu(struct net_device *dev,
				  int (*fn)(struct net_device *upper_dev,
					    struct netdev_nested_priv *priv),
				  struct netdev_nested_priv *priv);

bool netdev_has_upper_dev_all_rcu(struct net_device *dev,
				  struct net_device *upper_dev);

bool netdev_has_any_upper_dev(struct net_device *dev);

void *netdev_lower_get_next_private(struct net_device *dev,
				    struct list_head **iter);
void *netdev_lower_get_next_private_rcu(struct net_device *dev,
					struct list_head **iter);

#define netdev_for_each_lower_private(dev, priv, iter) \
	for (iter = (dev)->adj_list.lower.next, \
	     priv = netdev_lower_get_next_private(dev, &(iter)); \
	     priv; \
	     priv = netdev_lower_get_next_private(dev, &(iter)))

#define netdev_for_each_lower_private_rcu(dev, priv, iter) \
	for (iter = &(dev)->adj_list.lower, \
	     priv = netdev_lower_get_next_private_rcu(dev, &(iter)); \
	     priv; \
	     priv = netdev_lower_get_next_private_rcu(dev, &(iter)))

void *netdev_lower_get_next(struct net_device *dev,
				struct list_head **iter);

#define netdev_for_each_lower_dev(dev, ldev, iter) \
	for (iter = (dev)->adj_list.lower.next, \
	     ldev = netdev_lower_get_next(dev, &(iter)); \
	     ldev; \
	     ldev = netdev_lower_get_next(dev, &(iter)))

struct net_device *netdev_next_lower_dev_rcu(struct net_device *dev,
					     struct list_head **iter);
int netdev_walk_all_lower_dev(struct net_device *dev,
			      int (*fn)(struct net_device *lower_dev,
					struct netdev_nested_priv *priv),
			      struct netdev_nested_priv *priv);
int netdev_walk_all_lower_dev_rcu(struct net_device *dev,
				  int (*fn)(struct net_device *lower_dev,
					    struct netdev_nested_priv *priv),
				  struct netdev_nested_priv *priv);

void *netdev_adjacent_get_private(struct list_head *adj_list);
void *netdev_lower_get_first_private_rcu(struct net_device *dev);
struct net_device *netdev_master_upper_dev_get(struct net_device *dev);
struct net_device *netdev_master_upper_dev_get_rcu(struct net_device *dev);
int netdev_upper_dev_link(struct net_device *dev, struct net_device *upper_dev,
			  struct netlink_ext_ack *extack);
int netdev_master_upper_dev_link(struct net_device *dev,
				 struct net_device *upper_dev,
				 void *upper_priv, void *upper_info,
				 struct netlink_ext_ack *extack);
void netdev_upper_dev_unlink(struct net_device *dev,
			     struct net_device *upper_dev);
int netdev_adjacent_change_prepare(struct net_device *old_dev,
				   struct net_device *new_dev,
				   struct net_device *dev,
				   struct netlink_ext_ack *extack);
void netdev_adjacent_change_commit(struct net_device *old_dev,
				   struct net_device *new_dev,
				   struct net_device *dev);
void netdev_adjacent_change_abort(struct net_device *old_dev,
				  struct net_device *new_dev,
				  struct net_device *dev);
void netdev_adjacent_rename_links(struct net_device *dev, char *oldname);
void *netdev_lower_dev_get_private(struct net_device *dev,
				   struct net_device *lower_dev);
void netdev_lower_state_changed(struct net_device *lower_dev,
				void *lower_state_info);

/* RSS keys are 40 or 52 bytes long */
#define NETDEV_RSS_KEY_LEN 52
extern u8 netdev_rss_key[NETDEV_RSS_KEY_LEN] __read_mostly;
void netdev_rss_key_fill(void *buffer, size_t len);

int skb_checksum_help(struct sk_buff *skb);
int skb_crc32c_csum_help(struct sk_buff *skb);
int skb_csum_hwoffload_help(struct sk_buff *skb,
			    const netdev_features_t features);

struct netdev_bonding_info {
	ifslave	slave;
	ifbond	master;
};

struct netdev_notifier_bonding_info {
	struct netdev_notifier_info info; /* must be first */
	struct netdev_bonding_info  bonding_info;
};

void netdev_bonding_info_change(struct net_device *dev,
				struct netdev_bonding_info *bonding_info);

#if IS_ENABLED(CONFIG_ETHTOOL_NETLINK)
void ethtool_notify(struct net_device *dev, unsigned int cmd);
#else
static inline void ethtool_notify(struct net_device *dev, unsigned int cmd)
{
}
#endif

__be16 skb_network_protocol(struct sk_buff *skb, int *depth);

static inline bool can_checksum_protocol(netdev_features_t features,
					 __be16 protocol)
{
	if (protocol == htons(ETH_P_FCOE))
		return !!(features & NETIF_F_FCOE_CRC);

	/* Assume this is an IP checksum (not SCTP CRC) */

	if (features & NETIF_F_HW_CSUM) {
		/* Can checksum everything */
		return true;
	}

	switch (protocol) {
	case htons(ETH_P_IP):
		return !!(features & NETIF_F_IP_CSUM);
	case htons(ETH_P_IPV6):
		return !!(features & NETIF_F_IPV6_CSUM);
	default:
		return false;
	}
}

#ifdef CONFIG_BUG
void netdev_rx_csum_fault(struct net_device *dev, struct sk_buff *skb);
#else
static inline void netdev_rx_csum_fault(struct net_device *dev,
					struct sk_buff *skb)
{
}
#endif
/* rx skb timestamps */
void net_enable_timestamp(void);
void net_disable_timestamp(void);

static inline ktime_t netdev_get_tstamp(struct net_device *dev,
					const struct skb_shared_hwtstamps *hwtstamps,
					bool cycles)
{
	const struct net_device_ops *ops = dev->netdev_ops;

	if (ops->ndo_get_tstamp)
		return ops->ndo_get_tstamp(dev, hwtstamps, cycles);

	return hwtstamps->hwtstamp;
}

#ifndef CONFIG_PREEMPT_RT
static inline void netdev_xmit_set_more(bool more)
{
	__this_cpu_write(softnet_data.xmit.more, more);
}

static inline bool netdev_xmit_more(void)
{
	return __this_cpu_read(softnet_data.xmit.more);
}
#else
static inline void netdev_xmit_set_more(bool more)
{
	current->net_xmit.more = more;
}

static inline bool netdev_xmit_more(void)
{
	return current->net_xmit.more;
}
#endif

static inline netdev_tx_t __netdev_start_xmit(const struct net_device_ops *ops,
					      struct sk_buff *skb, struct net_device *dev,
					      bool more)
{
	netdev_xmit_set_more(more);
	return ops->ndo_start_xmit(skb, dev);
}

static inline netdev_tx_t netdev_start_xmit(struct sk_buff *skb, struct net_device *dev,
					    struct netdev_queue *txq, bool more)
{
	const struct net_device_ops *ops = dev->netdev_ops;
	netdev_tx_t rc;

	rc = __netdev_start_xmit(ops, skb, dev, more);
	if (rc == NETDEV_TX_OK)
		txq_trans_update(dev, txq);

	return rc;
}

int netdev_class_create_file_ns(const struct class_attribute *class_attr,
				const void *ns);
void netdev_class_remove_file_ns(const struct class_attribute *class_attr,
				 const void *ns);

extern const struct kobj_ns_type_operations net_ns_type_operations;

const char *netdev_drivername(const struct net_device *dev);

static inline netdev_features_t netdev_intersect_features(netdev_features_t f1,
							  netdev_features_t f2)
{
	if ((f1 ^ f2) & NETIF_F_HW_CSUM) {
		if (f1 & NETIF_F_HW_CSUM)
			f1 |= (NETIF_F_IP_CSUM|NETIF_F_IPV6_CSUM);
		else
			f2 |= (NETIF_F_IP_CSUM|NETIF_F_IPV6_CSUM);
	}

	return f1 & f2;
}

static inline netdev_features_t netdev_get_wanted_features(
	struct net_device *dev)
{
	return (dev->features & ~dev->hw_features) | dev->wanted_features;
}
netdev_features_t netdev_increment_features(netdev_features_t all,
	netdev_features_t one, netdev_features_t mask);

/* Allow TSO being used on stacked device :
 * Performing the GSO segmentation before last device
 * is a performance improvement.
 */
static inline netdev_features_t netdev_add_tso_features(netdev_features_t features,
							netdev_features_t mask)
{
	return netdev_increment_features(features, NETIF_F_ALL_TSO, mask);
}

int __netdev_update_features(struct net_device *dev);
void netdev_update_features(struct net_device *dev);
void netdev_change_features(struct net_device *dev);

void netif_stacked_transfer_operstate(const struct net_device *rootdev,
					struct net_device *dev);

netdev_features_t passthru_features_check(struct sk_buff *skb,
					  struct net_device *dev,
					  netdev_features_t features);
netdev_features_t netif_skb_features(struct sk_buff *skb);
void skb_warn_bad_offload(const struct sk_buff *skb);

static inline bool net_gso_ok(netdev_features_t features, int gso_type)
{
	netdev_features_t feature = (netdev_features_t)gso_type << NETIF_F_GSO_SHIFT;

	/* check flags correspondence */
	BUILD_BUG_ON(SKB_GSO_TCPV4   != (NETIF_F_TSO >> NETIF_F_GSO_SHIFT));
	BUILD_BUG_ON(SKB_GSO_DODGY   != (NETIF_F_GSO_ROBUST >> NETIF_F_GSO_SHIFT));
	BUILD_BUG_ON(SKB_GSO_TCP_ECN != (NETIF_F_TSO_ECN >> NETIF_F_GSO_SHIFT));
	BUILD_BUG_ON(SKB_GSO_TCP_FIXEDID != (NETIF_F_TSO_MANGLEID >> NETIF_F_GSO_SHIFT));
	BUILD_BUG_ON(SKB_GSO_TCPV6   != (NETIF_F_TSO6 >> NETIF_F_GSO_SHIFT));
	BUILD_BUG_ON(SKB_GSO_FCOE    != (NETIF_F_FSO >> NETIF_F_GSO_SHIFT));
	BUILD_BUG_ON(SKB_GSO_GRE     != (NETIF_F_GSO_GRE >> NETIF_F_GSO_SHIFT));
	BUILD_BUG_ON(SKB_GSO_GRE_CSUM != (NETIF_F_GSO_GRE_CSUM >> NETIF_F_GSO_SHIFT));
	BUILD_BUG_ON(SKB_GSO_IPXIP4  != (NETIF_F_GSO_IPXIP4 >> NETIF_F_GSO_SHIFT));
	BUILD_BUG_ON(SKB_GSO_IPXIP6  != (NETIF_F_GSO_IPXIP6 >> NETIF_F_GSO_SHIFT));
	BUILD_BUG_ON(SKB_GSO_UDP_TUNNEL != (NETIF_F_GSO_UDP_TUNNEL >> NETIF_F_GSO_SHIFT));
	BUILD_BUG_ON(SKB_GSO_UDP_TUNNEL_CSUM != (NETIF_F_GSO_UDP_TUNNEL_CSUM >> NETIF_F_GSO_SHIFT));
	BUILD_BUG_ON(SKB_GSO_PARTIAL != (NETIF_F_GSO_PARTIAL >> NETIF_F_GSO_SHIFT));
	BUILD_BUG_ON(SKB_GSO_TUNNEL_REMCSUM != (NETIF_F_GSO_TUNNEL_REMCSUM >> NETIF_F_GSO_SHIFT));
	BUILD_BUG_ON(SKB_GSO_SCTP    != (NETIF_F_GSO_SCTP >> NETIF_F_GSO_SHIFT));
	BUILD_BUG_ON(SKB_GSO_ESP != (NETIF_F_GSO_ESP >> NETIF_F_GSO_SHIFT));
	BUILD_BUG_ON(SKB_GSO_UDP != (NETIF_F_GSO_UDP >> NETIF_F_GSO_SHIFT));
	BUILD_BUG_ON(SKB_GSO_UDP_L4 != (NETIF_F_GSO_UDP_L4 >> NETIF_F_GSO_SHIFT));
	BUILD_BUG_ON(SKB_GSO_FRAGLIST != (NETIF_F_GSO_FRAGLIST >> NETIF_F_GSO_SHIFT));
	BUILD_BUG_ON(SKB_GSO_TCP_ACCECN !=
		     (NETIF_F_GSO_ACCECN >> NETIF_F_GSO_SHIFT));

	return (features & feature) == feature;
}

static inline bool skb_gso_ok(struct sk_buff *skb, netdev_features_t features)
{
	return net_gso_ok(features, skb_shinfo(skb)->gso_type) &&
	       (!skb_has_frag_list(skb) || (features & NETIF_F_FRAGLIST));
}

static inline bool netif_needs_gso(struct sk_buff *skb,
				   netdev_features_t features)
{
	return skb_is_gso(skb) && (!skb_gso_ok(skb, features) ||
		unlikely((skb->ip_summed != CHECKSUM_PARTIAL) &&
			 (skb->ip_summed != CHECKSUM_UNNECESSARY)));
}

void netif_set_tso_max_size(struct net_device *dev, unsigned int size);
void netif_set_tso_max_segs(struct net_device *dev, unsigned int segs);
void netif_inherit_tso_max(struct net_device *to,
			   const struct net_device *from);

static inline unsigned int
netif_get_gro_max_size(const struct net_device *dev, const struct sk_buff *skb)
{
	/* pairs with WRITE_ONCE() in netif_set_gro(_ipv4)_max_size() */
	return skb->protocol == htons(ETH_P_IPV6) ?
	       READ_ONCE(dev->gro_max_size) :
	       READ_ONCE(dev->gro_ipv4_max_size);
}

static inline unsigned int
netif_get_gso_max_size(const struct net_device *dev, const struct sk_buff *skb)
{
	/* pairs with WRITE_ONCE() in netif_set_gso(_ipv4)_max_size() */
	return skb->protocol == htons(ETH_P_IPV6) ?
	       READ_ONCE(dev->gso_max_size) :
	       READ_ONCE(dev->gso_ipv4_max_size);
}

static inline bool netif_is_macsec(const struct net_device *dev)
{
	return dev->priv_flags & IFF_MACSEC;
}

static inline bool netif_is_macvlan(const struct net_device *dev)
{
	return dev->priv_flags & IFF_MACVLAN;
}

static inline bool netif_is_macvlan_port(const struct net_device *dev)
{
	return dev->priv_flags & IFF_MACVLAN_PORT;
}

static inline bool netif_is_bond_master(const struct net_device *dev)
{
	return dev->flags & IFF_MASTER && dev->priv_flags & IFF_BONDING;
}

static inline bool netif_is_bond_slave(const struct net_device *dev)
{
	return dev->flags & IFF_SLAVE && dev->priv_flags & IFF_BONDING;
}

static inline bool netif_supports_nofcs(struct net_device *dev)
{
	return dev->priv_flags & IFF_SUPP_NOFCS;
}

static inline bool netif_has_l3_rx_handler(const struct net_device *dev)
{
	return dev->priv_flags & IFF_L3MDEV_RX_HANDLER;
}

static inline bool netif_is_l3_master(const struct net_device *dev)
{
	return dev->priv_flags & IFF_L3MDEV_MASTER;
}

static inline bool netif_is_l3_slave(const struct net_device *dev)
{
	return dev->priv_flags & IFF_L3MDEV_SLAVE;
}

static inline int dev_sdif(const struct net_device *dev)
{
#ifdef CONFIG_NET_L3_MASTER_DEV
	if (netif_is_l3_slave(dev))
		return dev->ifindex;
#endif
	return 0;
}

static inline bool netif_is_bridge_master(const struct net_device *dev)
{
	return dev->priv_flags & IFF_EBRIDGE;
}

static inline bool netif_is_bridge_port(const struct net_device *dev)
{
	return dev->priv_flags & IFF_BRIDGE_PORT;
}

static inline bool netif_is_ovs_master(const struct net_device *dev)
{
	return dev->priv_flags & IFF_OPENVSWITCH;
}

static inline bool netif_is_ovs_port(const struct net_device *dev)
{
	return dev->priv_flags & IFF_OVS_DATAPATH;
}

static inline bool netif_is_any_bridge_master(const struct net_device *dev)
{
	return netif_is_bridge_master(dev) || netif_is_ovs_master(dev);
}

static inline bool netif_is_any_bridge_port(const struct net_device *dev)
{
	return netif_is_bridge_port(dev) || netif_is_ovs_port(dev);
}

static inline bool netif_is_team_master(const struct net_device *dev)
{
	return dev->priv_flags & IFF_TEAM;
}

static inline bool netif_is_team_port(const struct net_device *dev)
{
	return dev->priv_flags & IFF_TEAM_PORT;
}

static inline bool netif_is_lag_master(const struct net_device *dev)
{
	return netif_is_bond_master(dev) || netif_is_team_master(dev);
}

static inline bool netif_is_lag_port(const struct net_device *dev)
{
	return netif_is_bond_slave(dev) || netif_is_team_port(dev);
}

static inline bool netif_is_rxfh_configured(const struct net_device *dev)
{
	return dev->priv_flags & IFF_RXFH_CONFIGURED;
}

static inline bool netif_is_failover(const struct net_device *dev)
{
	return dev->priv_flags & IFF_FAILOVER;
}

static inline bool netif_is_failover_slave(const struct net_device *dev)
{
	return dev->priv_flags & IFF_FAILOVER_SLAVE;
}

/* This device needs to keep skb dst for qdisc enqueue or ndo_start_xmit() */
static inline void netif_keep_dst(struct net_device *dev)
{
	dev->priv_flags &= ~(IFF_XMIT_DST_RELEASE | IFF_XMIT_DST_RELEASE_PERM);
}

/* return true if dev can't cope with mtu frames that need vlan tag insertion */
static inline bool netif_reduces_vlan_mtu(struct net_device *dev)
{
	/* TODO: reserve and use an additional IFF bit, if we get more users */
	return netif_is_macsec(dev);
}

extern struct pernet_operations __net_initdata loopback_net_ops;

/* Logging, debugging and troubleshooting/diagnostic helpers. */

/* netdev_printk helpers, similar to dev_printk */

static inline const char *netdev_name(const struct net_device *dev)
{
	if (!dev->name[0] || strchr(dev->name, '%'))
		return "(unnamed net_device)";
	return dev->name;
}

static inline const char *netdev_reg_state(const struct net_device *dev)
{
	u8 reg_state = READ_ONCE(dev->reg_state);

	switch (reg_state) {
	case NETREG_UNINITIALIZED: return " (uninitialized)";
	case NETREG_REGISTERED: return "";
	case NETREG_UNREGISTERING: return " (unregistering)";
	case NETREG_UNREGISTERED: return " (unregistered)";
	case NETREG_RELEASED: return " (released)";
	case NETREG_DUMMY: return " (dummy)";
	}

	WARN_ONCE(1, "%s: unknown reg_state %d\n", dev->name, reg_state);
	return " (unknown)";
}

#define MODULE_ALIAS_NETDEV(device) \
	MODULE_ALIAS("netdev-" device)

/*
 * netdev_WARN() acts like dev_printk(), but with the key difference
 * of using a WARN/WARN_ON to get the message out, including the
 * file/line information and a backtrace.
 */
#define netdev_WARN(dev, format, args...)			\
	WARN(1, "netdevice: %s%s: " format, netdev_name(dev),	\
	     netdev_reg_state(dev), ##args)

#define netdev_WARN_ONCE(dev, format, args...)				\
	WARN_ONCE(1, "netdevice: %s%s: " format, netdev_name(dev),	\
		  netdev_reg_state(dev), ##args)

/*
 *	The list of packet types we will receive (as opposed to discard)
 *	and the routines to invoke.
 *
 *	Why 16. Because with 16 the only overlap we get on a hash of the
 *	low nibble of the protocol value is RARP/SNAP/X.25.
 *
 *		0800	IP
 *		0001	802.3
 *		0002	AX.25
 *		0004	802.2
 *		8035	RARP
 *		0005	SNAP
 *		0805	X.25
 *		0806	ARP
 *		8137	IPX
 *		0009	Localtalk
 *		86DD	IPv6
 */
#define PTYPE_HASH_SIZE	(16)
#define PTYPE_HASH_MASK	(PTYPE_HASH_SIZE - 1)

extern struct list_head ptype_base[PTYPE_HASH_SIZE] __read_mostly;

extern struct net_device *blackhole_netdev;

/* Note: Avoid these macros in fast path, prefer per-cpu or per-queue counters. */
#define DEV_STATS_INC(DEV, FIELD) atomic_long_inc(&(DEV)->stats.__##FIELD)
#define DEV_STATS_ADD(DEV, FIELD, VAL) 	\
		atomic_long_add((VAL), &(DEV)->stats.__##FIELD)
#define DEV_STATS_READ(DEV, FIELD) atomic_long_read(&(DEV)->stats.__##FIELD)

#endif	/* _LINUX_NETDEVICE_H */
