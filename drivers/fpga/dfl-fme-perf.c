// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for FPGA Management Engine (FME) Global Performance Reporting
 *
 * Copyright 2019 Intel Corporation, Inc.
 *
 * Authors:
 *   Kang Luwei <luwei.kang@intel.com>
 *   Xiao Guangrong <guangrong.xiao@linux.intel.com>
 *   Wu Hao <hao.wu@intel.com>
 *   Xu Yilun <yilun.xu@intel.com>
 *   Joseph Grecco <joe.grecco@intel.com>
 *   Enno Luebbers <enno.luebbers@intel.com>
 *   Tim Whisonant <tim.whisonant@intel.com>
 *   Ananda Ravuri <ananda.ravuri@intel.com>
 *   Mitchel, Henry <henry.mitchel@intel.com>
 */

#include <linux/perf_event.h>
#include "dfl.h"
#include "dfl-fme.h"

/*
 * Performance Counter Registers for Cache.
 *
 * Cache Events are listed below as CACHE_EVNT_*.
 */
#define CACHE_CTRL			0x8
#define CACHE_RESET_CNTR		BIT_ULL(0)
#define CACHE_FREEZE_CNTR		BIT_ULL(8)
#define CACHE_CTRL_EVNT			GENMASK_ULL(19, 16)
#define CACHE_EVNT_RD_HIT		0x0
#define CACHE_EVNT_WR_HIT		0x1
#define CACHE_EVNT_RD_MISS		0x2
#define CACHE_EVNT_WR_MISS		0x3
#define CACHE_EVNT_RSVD			0x4
#define CACHE_EVNT_HOLD_REQ		0x5
#define CACHE_EVNT_DATA_WR_PORT_CONTEN	0x6
#define CACHE_EVNT_TAG_WR_PORT_CONTEN	0x7
#define CACHE_EVNT_TX_REQ_STALL		0x8
#define CACHE_EVNT_RX_REQ_STALL		0x9
#define CACHE_EVNT_EVICTIONS		0xa
#define CACHE_CHANNEL_SEL		BIT_ULL(20)
#define CACHE_CHANNEL_RD		0
#define CACHE_CHANNEL_WR		1
#define CACHE_CNTR0			0x10
#define CACHE_CNTR1			0x18
#define CACHE_CNTR_EVNT_CNTR		GENMASK_ULL(47, 0)
#define CACHE_CNTR_EVNT			GENMASK_ULL(63, 60)

/*
 * Performance Counter Registers for Fabric.
 *
 * Fabric Events are listed below as FAB_EVNT_*
 */
#define FAB_CTRL			0x20
#define FAB_RESET_CNTR			BIT_ULL(0)
#define FAB_FREEZE_CNTR			BIT_ULL(8)
#define FAB_CTRL_EVNT			GENMASK_ULL(19, 16)
#define FAB_EVNT_PCIE0_RD		0x0
#define FAB_EVNT_PCIE0_WR		0x1
#define FAB_EVNT_PCIE1_RD		0x2
#define FAB_EVNT_PCIE1_WR		0x3
#define FAB_EVNT_UPI_RD			0x4
#define FAB_EVNT_UPI_WR			0x5
#define FAB_EVNT_MMIO_RD		0x6
#define FAB_EVNT_MMIO_WR		0x7
#define FAB_PORT_ID			GENMASK_ULL(21, 20)
#define FAB_PORT_FILTER			BIT_ULL(23)
#define FAB_PORT_FILTER_DISABLE		0
#define FAB_PORT_FILTER_ENABLE		1
#define FAB_CNTR			0x28
#define FAB_CNTR_EVNT_CNTR		GENMASK_ULL(59, 0)
#define FAB_CNTR_EVNT			GENMASK_ULL(63, 60)

/*
 * Performance Counter Registers for Clock.
 *
 * Clock Counter can't be reset or frozen by SW.
 */
#define CLK_CNTR			0x30
#define BASIC_EVNT_CLK			0x0

/*
 * Performance Counter Registers for IOMMU / VT-D.
 *
 * VT-D Events are listed below as VTD_EVNT_* and VTD_SIP_EVNT_*
 */
#define VTD_CTRL			0x38
#define VTD_RESET_CNTR			BIT_ULL(0)
#define VTD_FREEZE_CNTR			BIT_ULL(8)
#define VTD_CTRL_EVNT			GENMASK_ULL(19, 16)
#define VTD_EVNT_AFU_MEM_RD_TRANS	0x0
#define VTD_EVNT_AFU_MEM_WR_TRANS	0x1
#define VTD_EVNT_AFU_DEVTLB_RD_HIT	0x2
#define VTD_EVNT_AFU_DEVTLB_WR_HIT	0x3
#define VTD_EVNT_DEVTLB_4K_FILL		0x4
#define VTD_EVNT_DEVTLB_2M_FILL		0x5
#define VTD_EVNT_DEVTLB_1G_FILL		0x6
#define VTD_CNTR			0x40
#define VTD_CNTR_EVNT_CNTR		GENMASK_ULL(47, 0)
#define VTD_CNTR_EVNT			GENMASK_ULL(63, 60)

#define VTD_SIP_CTRL			0x48
#define VTD_SIP_RESET_CNTR		BIT_ULL(0)
#define VTD_SIP_FREEZE_CNTR		BIT_ULL(8)
#define VTD_SIP_CTRL_EVNT		GENMASK_ULL(19, 16)
#define VTD_SIP_EVNT_IOTLB_4K_HIT	0x0
#define VTD_SIP_EVNT_IOTLB_2M_HIT	0x1
#define VTD_SIP_EVNT_IOTLB_1G_HIT	0x2
#define VTD_SIP_EVNT_SLPWC_L3_HIT	0x3
#define VTD_SIP_EVNT_SLPWC_L4_HIT	0x4
#define VTD_SIP_EVNT_RCC_HIT		0x5
#define VTD_SIP_EVNT_IOTLB_4K_MISS	0x6
#define VTD_SIP_EVNT_IOTLB_2M_MISS	0x7
#define VTD_SIP_EVNT_IOTLB_1G_MISS	0x8
#define VTD_SIP_EVNT_SLPWC_L3_MISS	0x9
#define VTD_SIP_EVNT_SLPWC_L4_MISS	0xa
#define VTD_SIP_EVNT_RCC_MISS		0xb
#define VTD_SIP_CNTR			0X50
#define VTD_SIP_CNTR_EVNT_CNTR		GENMASK_ULL(47, 0)
#define VTD_SIP_CNTR_EVNT		GENMASK_ULL(63, 60)

#define PERF_TIMEOUT			30

#define PERF_MAX_PORT_NUM		1

/**
 * struct fme_perf_priv - priv data structure for fme perf driver
 *
 * @dev: parent device.
 * @ioaddr: mapped base address of mmio region.
 * @pmu: pmu data structure for fme perf counters.
 * @id: id of this fme performance report private feature.
 * @fab_users: current user number on fabric counters.
 * @fab_port_id: used to indicate current working mode of fabric counters.
 * @fab_lock: lock to protect fabric counters working mode.
 * @events_group: events attribute group for fme perf pmu.
 * @attr_groups: attribute groups for fme perf pmu.
 */
struct fme_perf_priv {
	struct device *dev;
	void __iomem *ioaddr;
	struct pmu pmu;
	u64 id;

	u32 fab_users;
	u32 fab_port_id;
	spinlock_t fab_lock;

	struct attribute_group events_group;
	const struct attribute_group *attr_groups[4];
};

/**
 * struct fme_perf_event_attr - fme perf event attribute
 *
 * @attr: device attribute of fme perf event.
 * @event_id: id of fme perf event.
 * @event_type: type of fme perf event.
 * @is_port_event: indicate if this is a port based event.
 * @data: private data for fme perf event.
 */
struct fme_perf_event_attr {
	struct device_attribute attr;
	u32 event_id;
	u32 event_type;
	bool is_port_event;
	u64 data;
};

/**
 * struct fme_perf_event_ops - callbacks for fme perf events
 *
 * @event_init: callback invoked during event init.
 * @event_destroy: callback invoked during event destroy.
 * @read_counter: callback to read hardware counters.
 */
struct fme_perf_event_ops {
	int (*event_init)(struct fme_perf_priv *priv, u32 event,
			  u32 port_id, u64 data);
	void (*event_destroy)(struct fme_perf_priv *priv, u32 event,
			      u32 port_id, u64 data);
	u64 (*read_counter)(struct fme_perf_priv *priv, u32 event,
			    u32 port_id, u64 data);
};

/**
 * struct fme_perf_event_group - fme perf groups
 *
 * @ev_attrs: fme perf event attributes.
 * @num: events number in this group.
 * @ops: same callbacks shared by all fme perf events in this group.
 */
struct fme_perf_event_group {
	struct fme_perf_event_attr *ev_attrs;
	unsigned int num;
	struct fme_perf_event_ops *ops;
};

#define to_fme_perf_priv(_pmu)	container_of(_pmu, struct fme_perf_priv, pmu)

static cpumask_t fme_perf_cpumask = CPU_MASK_CPU0;

static ssize_t cpumask_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	return cpumap_print_to_pagebuf(true, buf, &fme_perf_cpumask);
}
static DEVICE_ATTR_RO(cpumask);

static struct attribute *fme_perf_cpumask_attrs[] = {
	&dev_attr_cpumask.attr,
	NULL,
};

static struct attribute_group fme_perf_cpumask_group = {
	.attrs = fme_perf_cpumask_attrs,
};

#define FME_EVENT_MASK		GENMASK_ULL(11, 0)
#define FME_EVTYPE_MASK		GENMASK_ULL(15, 12)
#define FME_EVTYPE_BASIC	0
#define FME_EVTYPE_CACHE	1
#define FME_EVTYPE_FABRIC	2
#define FME_EVTYPE_VTD		3
#define FME_EVTYPE_VTD_SIP	4
#define FME_EVTYPE_MAX		FME_EVTYPE_VTD_SIP
#define FME_PORTID_MASK		GENMASK_ULL(23, 16)
#define FME_PORTID_ROOT		(0xffU)

PMU_FORMAT_ATTR(event,		"config:0-11");
PMU_FORMAT_ATTR(evtype,		"config:12-15");
PMU_FORMAT_ATTR(portid,		"config:16-23");

static struct attribute *fme_perf_format_attrs[] = {
	&format_attr_event.attr,
	&format_attr_evtype.attr,
	&format_attr_portid.attr,
	NULL,
};

static struct attribute_group fme_perf_format_group = {
	.name = "format",
	.attrs = fme_perf_format_attrs,
};

static ssize_t fme_perf_event_sysfs_show(struct device *dev,
					 struct device_attribute *attr,
					 char *page)
{
	struct fme_perf_event_attr *ev_attr =
		container_of(attr, struct fme_perf_event_attr, attr);
	char *buf = page;

	buf += sprintf(buf, "event=0x%02x", ev_attr->event_id);
	buf += sprintf(buf, ",evtype=0x%02x", ev_attr->event_type);

	if (ev_attr->is_port_event)
		buf += sprintf(buf, ",portid=?\n");
	else
		buf += sprintf(buf, ",portid=0x%02x\n", FME_PORTID_ROOT);

	return (ssize_t)(buf - page);
}

#define FME_EVENT_ATTR(_name) \
	__ATTR(_name, 0444, fme_perf_event_sysfs_show, NULL)

#define FME_EVENT_BASIC(_name, _event) {		\
	.attr = FME_EVENT_ATTR(_name),			\
	.event_id = _event,				\
	.event_type = FME_EVTYPE_BASIC,			\
	.is_port_event = false,				\
}

/* data is used to save hardware channel information for cache events */
#define FME_EVENT_CACHE(_name, _event, _data) {		\
	.attr = FME_EVENT_ATTR(cache_##_name),		\
	.event_id = _event,				\
	.event_type = FME_EVTYPE_CACHE,			\
	.is_port_event = false,				\
	.data = _data,					\
}

#define FME_EVENT_FABRIC(_name, _event) {		\
	.attr = FME_EVENT_ATTR(fab_##_name),		\
	.event_id = _event,				\
	.event_type = FME_EVTYPE_FABRIC,		\
	.is_port_event = false,				\
}

#define FME_EVENT_FABRIC_PORT(_name, _event) {		\
	.attr = FME_EVENT_ATTR(fab_port_##_name),	\
	.event_id = _event,				\
	.event_type = FME_EVTYPE_FABRIC,		\
	.is_port_event = true,				\
}

#define FME_EVENT_VTD_PORT(_name, _event) {		\
	.attr = FME_EVENT_ATTR(vtd_port_##_name),	\
	.event_id = _event,				\
	.event_type = FME_EVTYPE_VTD,			\
	.is_port_event = true,				\
}

#define FME_EVENT_VTD_SIP(_name, _event) {		\
	.attr = FME_EVENT_ATTR(vtd_sip_##_name),	\
	.event_id = _event,				\
	.event_type = FME_EVTYPE_VTD_SIP,		\
	.is_port_event = false,				\
}

static struct fme_perf_event_attr fme_perf_basic_events[] = {
	FME_EVENT_BASIC(clock, BASIC_EVNT_CLK),
};

static struct fme_perf_event_attr fme_perf_cache_events[] = {
	FME_EVENT_CACHE(read_hit,     CACHE_EVNT_RD_HIT,    CACHE_CHANNEL_RD),
	FME_EVENT_CACHE(read_miss,    CACHE_EVNT_RD_MISS,   CACHE_CHANNEL_RD),
	FME_EVENT_CACHE(write_hit,    CACHE_EVNT_WR_HIT,    CACHE_CHANNEL_WR),
	FME_EVENT_CACHE(write_miss,   CACHE_EVNT_WR_MISS,   CACHE_CHANNEL_WR),
	FME_EVENT_CACHE(hold_request, CACHE_EVNT_HOLD_REQ,  CACHE_CHANNEL_RD),
	FME_EVENT_CACHE(data_write_port_contention,
			CACHE_EVNT_DATA_WR_PORT_CONTEN, CACHE_CHANNEL_WR),
	FME_EVENT_CACHE(tag_write_port_contention,
			CACHE_EVNT_TAG_WR_PORT_CONTEN,  CACHE_CHANNEL_WR),
	FME_EVENT_CACHE(tx_req_stall, CACHE_EVNT_TX_REQ_STALL,
			CACHE_CHANNEL_RD),
	FME_EVENT_CACHE(rx_req_stall, CACHE_EVNT_RX_REQ_STALL,
			CACHE_CHANNEL_RD),
	FME_EVENT_CACHE(eviction,  CACHE_EVNT_EVICTIONS, CACHE_CHANNEL_RD),
};

static struct fme_perf_event_attr fme_perf_fab_events[] = {
	FME_EVENT_FABRIC(pcie0_read,  FAB_EVNT_PCIE0_RD),
	FME_EVENT_FABRIC(pcie0_write, FAB_EVNT_PCIE0_WR),
	FME_EVENT_FABRIC(pcie1_read,  FAB_EVNT_PCIE1_RD),
	FME_EVENT_FABRIC(pcie1_write, FAB_EVNT_PCIE1_WR),
	FME_EVENT_FABRIC(upi_read,    FAB_EVNT_UPI_RD),
	FME_EVENT_FABRIC(upi_write,   FAB_EVNT_UPI_WR),
	FME_EVENT_FABRIC(mmio_read,   FAB_EVNT_MMIO_RD),
	FME_EVENT_FABRIC(mmio_write,  FAB_EVNT_MMIO_WR),

	FME_EVENT_FABRIC_PORT(pcie0_read,  FAB_EVNT_PCIE0_RD),
	FME_EVENT_FABRIC_PORT(pcie0_write, FAB_EVNT_PCIE0_WR),
	FME_EVENT_FABRIC_PORT(pcie1_read,  FAB_EVNT_PCIE1_RD),
	FME_EVENT_FABRIC_PORT(pcie1_write, FAB_EVNT_PCIE1_WR),
	FME_EVENT_FABRIC_PORT(upi_read,    FAB_EVNT_UPI_RD),
	FME_EVENT_FABRIC_PORT(upi_write,   FAB_EVNT_UPI_WR),
	FME_EVENT_FABRIC_PORT(mmio_read,   FAB_EVNT_MMIO_RD),
	FME_EVENT_FABRIC_PORT(mmio_write,  FAB_EVNT_MMIO_WR),
};

static struct fme_perf_event_attr fme_perf_vtd_events[] = {
	FME_EVENT_VTD_PORT(read_transaction,  VTD_EVNT_AFU_MEM_RD_TRANS),
	FME_EVENT_VTD_PORT(write_transaction, VTD_EVNT_AFU_MEM_WR_TRANS),
	FME_EVENT_VTD_PORT(devtlb_read_hit,   VTD_EVNT_AFU_DEVTLB_RD_HIT),
	FME_EVENT_VTD_PORT(devtlb_write_hit,  VTD_EVNT_AFU_DEVTLB_WR_HIT),
	FME_EVENT_VTD_PORT(devtlb_4k_fill,    VTD_EVNT_DEVTLB_4K_FILL),
	FME_EVENT_VTD_PORT(devtlb_2m_fill,    VTD_EVNT_DEVTLB_2M_FILL),
	FME_EVENT_VTD_PORT(devtlb_1g_fill,    VTD_EVNT_DEVTLB_1G_FILL),
};

static struct fme_perf_event_attr fme_perf_vtd_sip_events[] = {
	FME_EVENT_VTD_SIP(iotlb_4k_hit,  VTD_SIP_EVNT_IOTLB_4K_HIT),
	FME_EVENT_VTD_SIP(iotlb_2m_hit,  VTD_SIP_EVNT_IOTLB_2M_HIT),
	FME_EVENT_VTD_SIP(iotlb_1g_hit,  VTD_SIP_EVNT_IOTLB_1G_HIT),
	FME_EVENT_VTD_SIP(slpwc_l3_hit,  VTD_SIP_EVNT_SLPWC_L3_HIT),
	FME_EVENT_VTD_SIP(slpwc_l4_hit,  VTD_SIP_EVNT_SLPWC_L4_HIT),
	FME_EVENT_VTD_SIP(rcc_hit,       VTD_SIP_EVNT_RCC_HIT),
	FME_EVENT_VTD_SIP(iotlb_4k_miss, VTD_SIP_EVNT_IOTLB_4K_MISS),
	FME_EVENT_VTD_SIP(iotlb_2m_miss, VTD_SIP_EVNT_IOTLB_2M_MISS),
	FME_EVENT_VTD_SIP(iotlb_1g_miss, VTD_SIP_EVNT_IOTLB_1G_MISS),
	FME_EVENT_VTD_SIP(slpwc_l3_miss, VTD_SIP_EVNT_SLPWC_L3_MISS),
	FME_EVENT_VTD_SIP(slpwc_l4_miss, VTD_SIP_EVNT_SLPWC_L4_MISS),
	FME_EVENT_VTD_SIP(rcc_miss,      VTD_SIP_EVNT_RCC_MISS),
};

static u64 basic_read_event_counter(struct fme_perf_priv *priv,
				    u32 event, u32 port_id, u64 data)
{
	void __iomem *base = priv->ioaddr;

	if (event == BASIC_EVNT_CLK)
		return readq(base + CLK_CNTR);

	return 0;
}

static struct fme_perf_event_ops fme_perf_basic_ops = {
	.read_counter = basic_read_event_counter,
};

static u64 cache_read_event_counter(struct fme_perf_priv *priv,
				    u32 event, u32 port_id, u64 data)
{
	void __iomem *base = priv->ioaddr;
	u8 channel = (u8)data;
	u64 v, count;

	/* set channel access type and cache event code. */
	v = readq(base + CACHE_CTRL);
	v &= ~(CACHE_CHANNEL_SEL | CACHE_CTRL_EVNT);
	v |= FIELD_PREP(CACHE_CHANNEL_SEL, channel);
	v |= FIELD_PREP(CACHE_CTRL_EVNT, event);
	writeq(v, base + CACHE_CTRL);

	if (readq_poll_timeout_atomic(base + CACHE_CNTR0, v,
				      FIELD_GET(CACHE_CNTR_EVNT, v) == event,
				      1, PERF_TIMEOUT)) {
		dev_err(priv->dev, "timeout, unmatched cache event code in counter register.\n");
		return 0;
	}

	v = readq(base + CACHE_CNTR0);
	count = FIELD_GET(CACHE_CNTR_EVNT_CNTR, v);
	v = readq(base + CACHE_CNTR1);
	count += FIELD_GET(CACHE_CNTR_EVNT_CNTR, v);

	return count;
}

static struct fme_perf_event_ops fme_perf_cache_ops = {
	.read_counter = cache_read_event_counter,
};

static int fabric_event_init(struct fme_perf_priv *priv,
			     u32 event, u32 port_id, u64 data)
{
	void __iomem *base = priv->ioaddr;
	int ret = 0;
	u64 v;

	/*
	 * as fabric counter set only can be in either overall or port mode.
	 * In overall mode, it counts overall data for FPGA, and in port mode,
	 * it is configured to monitor on one individual port.
	 *
	 * so every time, a new event is initialized, driver checks
	 * current working mode and if someone is using this counter set.
	 */
	spin_lock(&priv->fab_lock);
	if (priv->fab_users && priv->fab_port_id != port_id) {
		dev_dbg(priv->dev, "conflict fabric event monitoring mode.\n");
		ret = -EOPNOTSUPP;
		goto exit;
	}

	priv->fab_users++;

	/*
	 * skip if current working mode matches, otherwise change the working
	 * mode per input port_id, to monitor overall data or another port.
	 */
	if (priv->fab_port_id == port_id)
		goto exit;

	priv->fab_port_id = port_id;

	v = readq(base + FAB_CTRL);
	v &= ~(FAB_PORT_FILTER | FAB_PORT_ID);

	if (port_id == FME_PORTID_ROOT) {
		v |= FIELD_PREP(FAB_PORT_FILTER, FAB_PORT_FILTER_DISABLE);
	} else {
		v |= FIELD_PREP(FAB_PORT_FILTER, FAB_PORT_FILTER_ENABLE);
		v |= FIELD_PREP(FAB_PORT_ID, port_id);
	}
	writeq(v, base + FAB_CTRL);

exit:
	spin_unlock(&priv->fab_lock);
	return ret;
}

static void fabric_event_destroy(struct fme_perf_priv *priv,
				 u32 event, u32 port_id, u64 data)
{
	spin_lock(&priv->fab_lock);
	priv->fab_users--;
	spin_unlock(&priv->fab_lock);
}

static u64 fabric_read_event_counter(struct fme_perf_priv *priv,
				     u32 event, u32 port_id, u64 data)
{
	void __iomem *base = priv->ioaddr;
	u64 v;

	v = readq(base + FAB_CTRL);
	v &= ~FAB_CTRL_EVNT;
	v |= FIELD_PREP(FAB_CTRL_EVNT, event);
	writeq(v, base + FAB_CTRL);

	if (readq_poll_timeout_atomic(base + FAB_CNTR, v,
				      FIELD_GET(FAB_CNTR_EVNT, v) == event,
				      1, PERF_TIMEOUT)) {
		dev_err(priv->dev, "timeout, unmatched fab event code in counter register.\n");
		return 0;
	}

	v = readq(base + FAB_CNTR);
	return FIELD_GET(FAB_CNTR_EVNT_CNTR, v);
}

static struct fme_perf_event_ops fme_perf_fab_ops = {
	.event_init = fabric_event_init,
	.event_destroy = fabric_event_destroy,
	.read_counter = fabric_read_event_counter,
};

static u64 vtd_read_event_counter(struct fme_perf_priv *priv,
				  u32 event, u32 port_id, u64 data)
{
	void __iomem *base = priv->ioaddr;
	u64 v;

	event += port_id;

	v = readq(base + VTD_CTRL);
	v &= ~VTD_CTRL_EVNT;
	v |= FIELD_PREP(VTD_CTRL_EVNT, event);
	writeq(v, base + VTD_CTRL);

	if (readq_poll_timeout_atomic(base + VTD_CNTR, v,
				      FIELD_GET(VTD_CNTR_EVNT, v) == event,
				      1, PERF_TIMEOUT)) {
		dev_err(priv->dev, "timeout, unmatched vtd event code in counter register.\n");
		return 0;
	}

	v = readq(base + VTD_CNTR);
	return FIELD_GET(VTD_CNTR_EVNT_CNTR, v);
}

static struct fme_perf_event_ops fme_perf_vtd_ops = {
	.read_counter = vtd_read_event_counter,
};

static u64 vtd_sip_read_event_counter(struct fme_perf_priv *priv,
				      u32 event, u32 port_id, u64 data)
{
	void __iomem *base = priv->ioaddr;
	u64 v;

	v = readq(base + VTD_SIP_CTRL);
	v &= ~VTD_SIP_CTRL_EVNT;
	v |= FIELD_PREP(VTD_SIP_CTRL_EVNT, event);
	writeq(v, base + VTD_SIP_CTRL);

	if (readq_poll_timeout_atomic(base + VTD_SIP_CNTR, v,
				      FIELD_GET(VTD_SIP_CNTR_EVNT, v) == event,
				      1, PERF_TIMEOUT)) {
		dev_err(priv->dev, "timeout, unmatched vtd sip event code in counter register\n");
		return 0;
	}

	v = readq(base + VTD_SIP_CNTR);
	return FIELD_GET(VTD_SIP_CNTR_EVNT_CNTR, v);
}

static struct fme_perf_event_ops fme_perf_vtd_sip_ops = {
	.read_counter = vtd_sip_read_event_counter,
};

#define FME_EVENT_GROUP(_name) {			\
	.ev_attrs = fme_perf_##_name##_events,		\
	.num = ARRAY_SIZE(fme_perf_##_name##_events),	\
	.ops = &fme_perf_##_name##_ops,			\
}

/* event group array is indexed by FME_EVTYPE_* */
static struct fme_perf_event_group fme_perf_event_groups[] = {
	FME_EVENT_GROUP(basic),
	FME_EVENT_GROUP(cache),
	FME_EVENT_GROUP(fab),
	FME_EVENT_GROUP(vtd),
	FME_EVENT_GROUP(vtd_sip),
};

static struct fme_perf_event_attr *
get_event_attr(u32 event_id, u32 event_type, u32 port_id)
{
	bool is_port_event = (port_id != FME_PORTID_ROOT);
	struct fme_perf_event_group *group;
	unsigned int i;

	if (event_type > FME_EVTYPE_MAX)
		return NULL;

	group = &fme_perf_event_groups[event_type];

	for (i = 0; i < group->num; i++) {
		if (event_id == group->ev_attrs[i].event_id &&
		    is_port_event == group->ev_attrs[i].is_port_event)
			return &group->ev_attrs[i];
	}

	return NULL;
}

static struct fme_perf_event_ops *get_event_ops(u32 event_type)
{
	return fme_perf_event_groups[event_type].ops;
}

static void fme_perf_event_destroy(struct perf_event *event)
{
	struct fme_perf_event_ops *ops = get_event_ops(event->hw.event_base);
	struct fme_perf_priv *priv = to_fme_perf_priv(event->pmu);

	if (ops->event_destroy)
		ops->event_destroy(priv, event->hw.idx,
				   event->hw.config_base, event->hw.config);
}

static int fme_perf_event_init(struct perf_event *event)
{
	struct fme_perf_priv *priv = to_fme_perf_priv(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	struct fme_perf_event_attr *ev_attr;
	u32 event_id, event_type, port_id;
	struct fme_perf_event_ops *ops;

	/* test the event attr type check for PMU enumeration */
	if (event->attr.type != event->pmu->type)
		return -ENOENT;

	/*
	 * fme counters are shared across all cores.
	 * Therefore, it does not support per-process mode.
	 * Also, it does not support event sampling mode.
	 */
	if (is_sampling_event(event) || event->attach_state & PERF_ATTACH_TASK)
		return -EINVAL;

	if (event->cpu < 0)
		return -EINVAL;

	event_id = FIELD_GET(FME_EVENT_MASK, event->attr.config);
	event_type = FIELD_GET(FME_EVTYPE_MASK, event->attr.config);
	port_id = FIELD_GET(FME_PORTID_MASK, event->attr.config);

	ev_attr = get_event_attr(event_id, event_type, port_id);
	if (!ev_attr)
		return -EINVAL;

	if ((ev_attr->is_port_event && port_id >= PERF_MAX_PORT_NUM) ||
	    (!ev_attr->is_port_event && port_id != FME_PORTID_ROOT))
		return -EINVAL;

	hwc->event_base = event_type;
	hwc->idx = (int)event_id;
	hwc->config_base = port_id;
	hwc->config = ev_attr->data;

	event->destroy = fme_perf_event_destroy;

	dev_dbg(priv->dev,
		"%s eventid=0x%x, evtype=0x%x, portid=0x%x, data=0x%llx\n",
		__func__, event_id, event_type, port_id, ev_attr->data);

	ops = get_event_ops(event->hw.event_base);

	if (ops->event_init)
		return ops->event_init(priv, hwc->idx,
				       hwc->config_base, hwc->config);

	return 0;
}

static void fme_perf_event_update(struct perf_event *event)
{
	struct fme_perf_event_ops *ops = get_event_ops(event->hw.event_base);
	struct fme_perf_priv *priv = to_fme_perf_priv(event->pmu);
	u64 now, prev, delta;

	now = ops->read_counter(priv, (u32)event->hw.idx,
				event->hw.config_base, event->hw.config);
	prev = local64_read(&event->hw.prev_count);
	delta = now - prev;

	local64_add(delta, &event->count);
}

static void fme_perf_event_start(struct perf_event *event, int flags)
{
	struct fme_perf_event_ops *ops = get_event_ops(event->hw.event_base);
	struct fme_perf_priv *priv = to_fme_perf_priv(event->pmu);
	u64 count;

	count = ops->read_counter(priv, (u32)event->hw.idx,
				  event->hw.config_base, event->hw.config);
	local64_set(&event->hw.prev_count, count);
}

static void fme_perf_event_stop(struct perf_event *event, int flags)
{
	fme_perf_event_update(event);
}

static int fme_perf_event_add(struct perf_event *event, int flags)
{
	if (flags & PERF_EF_START)
		fme_perf_event_start(event, flags);

	return 0;
}

static void fme_perf_event_del(struct perf_event *event, int flags)
{
	fme_perf_event_stop(event, PERF_EF_UPDATE);
}

static void fme_perf_event_read(struct perf_event *event)
{
	fme_perf_event_update(event);
}

static int fme_perf_setup_attrs(struct fme_perf_priv *priv)
{
	struct fme_perf_event_group *group;
	unsigned int i, idx = 0, num = 0;
	struct attribute **attrs;

	/*
	 * if feature id is FME_FEATURE_ID_GLOBAL_IPERF, hardware supports
	 * all performance counters, otherwise only basic and fabric counters.
	 */
	num += fme_perf_event_groups[FME_EVTYPE_BASIC].num;
	num += fme_perf_event_groups[FME_EVTYPE_FABRIC].num;

	if (priv->id == FME_FEATURE_ID_GLOBAL_IPERF) {
		num += fme_perf_event_groups[FME_EVTYPE_CACHE].num;
		num += fme_perf_event_groups[FME_EVTYPE_VTD].num;
		num += fme_perf_event_groups[FME_EVTYPE_VTD_SIP].num;
	}

	attrs = devm_kcalloc(priv->dev, num + 1, sizeof(*attrs), GFP_KERNEL);
	if (!attrs)
		return -ENOMEM;

	group = &fme_perf_event_groups[FME_EVTYPE_BASIC];
	for (i = 0; i < group->num; i++)
		attrs[idx++] = &group->ev_attrs[i].attr.attr;

	group = &fme_perf_event_groups[FME_EVTYPE_FABRIC];
	for (i = 0; i < group->num; i++)
		attrs[idx++] = &group->ev_attrs[i].attr.attr;

	if (priv->id == FME_FEATURE_ID_GLOBAL_IPERF) {
		group = &fme_perf_event_groups[FME_EVTYPE_CACHE];
		for (i = 0; i < group->num; i++)
			attrs[idx++] = &group->ev_attrs[i].attr.attr;

		group = &fme_perf_event_groups[FME_EVTYPE_VTD];
		for (i = 0; i < group->num; i++)
			attrs[idx++] = &group->ev_attrs[i].attr.attr;

		group = &fme_perf_event_groups[FME_EVTYPE_VTD_SIP];
		for (i = 0; i < group->num; i++)
			attrs[idx++] = &group->ev_attrs[i].attr.attr;
	}

	priv->events_group.name = "events";
	priv->events_group.attrs = attrs;

	priv->attr_groups[0] = &fme_perf_format_group;
	priv->attr_groups[1] = &fme_perf_cpumask_group;
	priv->attr_groups[2] = &priv->events_group;

	return 0;
}

static void fme_perf_setup_hardware(struct fme_perf_priv *priv)
{
	void __iomem *base = priv->ioaddr;
	u64 v;

	/* read and save current working mode for fabric counters */
	v = readq(base + FAB_CTRL);

	if (FIELD_GET(FAB_PORT_FILTER, v) == FAB_PORT_FILTER_DISABLE)
		priv->fab_port_id = FME_PORTID_ROOT;
	else
		priv->fab_port_id = FIELD_GET(FAB_PORT_ID, v);
}

static int fme_perf_pmu_register(struct platform_device *pdev,
				 struct fme_perf_priv *priv)
{
	struct pmu *pmu = &priv->pmu;
	char *name;
	int ret;

	spin_lock_init(&priv->fab_lock);

	ret = fme_perf_setup_attrs(priv);
	if (ret)
		return ret;

	fme_perf_setup_hardware(priv);

	pmu->task_ctx_nr =	perf_invalid_context;
	pmu->attr_groups =	priv->attr_groups;
	pmu->event_init =	fme_perf_event_init;
	pmu->add =		fme_perf_event_add;
	pmu->del =		fme_perf_event_del;
	pmu->start =		fme_perf_event_start;
	pmu->stop =		fme_perf_event_stop;
	pmu->read =		fme_perf_event_read;
	pmu->capabilities =	PERF_PMU_CAP_NO_INTERRUPT |
				PERF_PMU_CAP_NO_EXCLUDE;

	name = devm_kasprintf(priv->dev, GFP_KERNEL, "fme%d", pdev->id);

	ret = perf_pmu_register(pmu, name, -1);
	if (ret)
		return ret;

	return 0;
}

static void fme_perf_pmu_unregister(struct fme_perf_priv *priv)
{
	perf_pmu_unregister(&priv->pmu);
}

static int fme_perf_init(struct platform_device *pdev,
			 struct dfl_feature *feature)
{
	struct fme_perf_priv *priv;
	int ret;

	dev_dbg(&pdev->dev, "FME Perf Init\n");

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = &pdev->dev;
	priv->ioaddr = feature->ioaddr;
	priv->id = feature->id;

	ret = fme_perf_pmu_register(pdev, priv);
	if (ret)
		return ret;

	feature->priv = priv;
	return 0;
}

static void fme_perf_uinit(struct platform_device *pdev,
			   struct dfl_feature *feature)
{
	struct fme_perf_priv *priv = feature->priv;

	fme_perf_pmu_unregister(priv);
}

const struct dfl_feature_id fme_perf_id_table[] = {
	{.id = FME_FEATURE_ID_GLOBAL_IPERF,},
	{.id = FME_FEATURE_ID_GLOBAL_DPERF,},
	{0,}
};

const struct dfl_feature_ops fme_perf_ops = {
	.init = fme_perf_init,
	.uinit = fme_perf_uinit,
};
