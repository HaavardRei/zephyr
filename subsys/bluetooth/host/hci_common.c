/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/bluetooth/buf.h>
#include <zephyr/bluetooth/hci_types.h>
#include <zephyr/drivers/bluetooth.h>
#include <zephyr/kernel.h>
#include <zephyr/net_buf.h>
#include <zephyr/sys/byteorder.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(bt_hci_common, CONFIG_BT_HCI_DRIVER_LOG_LEVEL);

#include "common/assert.h"

struct net_buf *bt_hci_evt_create(uint8_t evt, uint8_t len)
{
	struct bt_hci_evt_hdr *hdr;
	struct net_buf *buf;

	buf = bt_buf_get_evt(evt, false, K_FOREVER);

	BT_ASSERT(buf);

	hdr = net_buf_add(buf, sizeof(*hdr));
	hdr->evt = evt;
	hdr->len = len;

	return buf;
}

struct net_buf *bt_hci_cmd_complete_create(uint16_t op, uint8_t plen)
{
	struct net_buf *buf;
	struct bt_hci_evt_cmd_complete *cc;

	buf = bt_hci_evt_create(BT_HCI_EVT_CMD_COMPLETE, sizeof(*cc) + plen);

	cc = net_buf_add(buf, sizeof(*cc));

	/* The Num_HCI_Command_Packets parameter allows the Controller to
	 * indicate the number of HCI command packets the Host can send to the
	 * Controller. If the Controller requires the Host to stop sending
	 * commands, Num_HCI_Command_Packets will be set to zero.
	 *
	 * NOTE: Zephyr Controller (and may be other Controllers) do not support
	 *       higher Number of HCI Command packets than 1.
	 */
	cc->ncmd = 1U;

	cc->opcode = sys_cpu_to_le16(op);

	return buf;
}

struct net_buf *bt_hci_cmd_status_create(uint16_t op, uint8_t status)
{
	struct net_buf *buf;
	struct bt_hci_evt_cmd_status *cs;

	buf = bt_hci_evt_create(BT_HCI_EVT_CMD_STATUS, sizeof(*cs));

	cs = net_buf_add(buf, sizeof(*cs));
	cs->status = status;

	/* The Num_HCI_Command_Packets parameter allows the Controller to
	 * indicate the number of HCI command packets the Host can send to the
	 * Controller. If the Controller requires the Host to stop sending
	 * commands, Num_HCI_Command_Packets will be set to zero.
	 *
	 * NOTE: Zephyr Controller (and may be other Controllers) do not support
	 *       higher Number of HCI Command packets than 1.
	 */
	cs->ncmd = 1U;

	cs->opcode = sys_cpu_to_le16(op);

	return buf;
}

#if defined(CONFIG_BT_EXT_ADV)
struct discarded_ext_adv_chain {
	bt_addr_le_t addr;
	uint8_t sid;
	bool active;
};

static struct discarded_ext_adv_chain discarded_chain;

static struct discarded_ext_adv_chain *find_discarded_chain(const bt_addr_le_t *addr, uint8_t sid)
{
	if (discarded_chain.active && discarded_chain.sid == sid &&
	    bt_addr_le_eq(&discarded_chain.addr, addr)) {
		return &discarded_chain;
	}
	return NULL;
}

static struct discarded_ext_adv_chain *alloc_discarded_chain(const bt_addr_le_t *addr, uint8_t sid)
{
	if (!discarded_chain.active) {
		bt_addr_le_copy(&discarded_chain.addr, addr);
		discarded_chain.sid = sid;
		discarded_chain.active = true;
		return &discarded_chain;
	}
	return NULL;
}

bool bt_hci_ext_adv_report_process(bt_hci_recv_t recv, const struct device *dev,
				   const uint8_t *data, size_t len)
{
	struct bt_hci_evt_hdr hdr;
	const struct bt_hci_evt_le_ext_advertising_report *report;
	const struct bt_hci_evt_le_ext_advertising_info *info;
	uint16_t evt_type;
	uint16_t data_status;
	struct discarded_ext_adv_chain *chain;
	struct net_buf *buf;
	size_t buf_tailroom;

	if (len < sizeof(hdr) + sizeof(struct bt_hci_evt_le_meta_event)) {
		return false;
	}

	memcpy(&hdr, data, sizeof(hdr));

	if (hdr.evt != BT_HCI_EVT_LE_META_EVENT) {
		return false;
	}

	if (data[sizeof(hdr)] != BT_HCI_EVT_LE_EXT_ADVERTISING_REPORT) {
		return false;
	}

	report = (const void *)&data[sizeof(hdr) + sizeof(struct bt_hci_evt_le_meta_event)];
	info = report->adv_info;
	evt_type = sys_le16_to_cpu(info->evt_type);
	data_status = BT_HCI_LE_ADV_EVT_TYPE_DATA_STATUS(evt_type);

	chain = find_discarded_chain(&info->addr, info->sid);
	if (chain) {
		if (data_status != BT_HCI_LE_ADV_EVT_TYPE_DATA_STATUS_PARTIAL) {
			LOG_DBG("Discarding of tracked ext adv chain completed");
			chain->active = false;
		}
		LOG_DBG("Discarding tracked ext adv chain fragment");
		return true;
	}

	buf = bt_buf_get_evt(hdr.evt, true, K_NO_WAIT);
	if (!buf) {
		if (data_status == BT_HCI_LE_ADV_EVT_TYPE_DATA_STATUS_PARTIAL) {
			LOG_DBG("Allocating new discarded chain");
			chain = alloc_discarded_chain(&info->addr, info->sid);
			if (!chain) {
				LOG_ERR("Failed to allocate new discarded chain");
			}
		}
		LOG_DBG("Discarding ext adv report");
		return true;
	}

	buf_tailroom = net_buf_tailroom(buf);
	if (buf_tailroom < len) {
		LOG_ERR("Not enough space in buffer %zu/%zu", len, buf_tailroom);
		net_buf_unref(buf);
		return true;
	}

	net_buf_add_mem(buf, &hdr, sizeof(hdr));
	net_buf_add_mem(buf, data + sizeof(hdr), len - sizeof(hdr));

	recv(dev, buf);
	return true;
}
#endif /* CONFIG_BT_EXT_ADV */
