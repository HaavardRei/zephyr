/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bs_bt_utils.h"
#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/settings/settings.h>

#include <stdint.h>
#include <string.h>

#include <zephyr/bluetooth/bluetooth.h>

#include "babblekit/testcase.h"
#include "babblekit/flags.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static bool bond_addr_saved;

static void bond_info_cb(const struct bt_bond_info *info, void *user_data)
{
	char addr_str[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(&info->addr, addr_str, sizeof(addr_str));
	LOG_INF("Found bond: %s", addr_str);

	bond_addr_saved = true;
}

void central_save(void)
{
	bs_bt_utils_setup();

	LOG_INF("== Bonding (Save Phase) ==");
	BUILD_ASSERT(CONFIG_BT_BONDABLE, "CONFIG_BT_BONDABLE must be enabled by default.");
	scan_connect_to_first_result();
	wait_connected();
	set_security(BT_SECURITY_L2);
	TAKE_FLAG(flag_pairing_complete);
	TAKE_FLAG(flag_bonded);

	const bt_addr_le_t *addr = bt_conn_get_dst(test_conn);

	if (addr) {
		char addr_str[BT_ADDR_LE_STR_LEN];

		bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
		LOG_INF("Bonded to: %s", addr_str);
	}

	disconnect();
	wait_disconnected();

	k_sleep(K_MSEC(CONFIG_BT_SETTINGS_DELAYED_STORE_MS + 100));
	clear_test_conn();

	LOG_INF("Bond saved");
	TEST_PASS("PASS");
}

void central_load(void)
{
	bs_bt_utils_setup();

	LOG_INF("== Loading Settings (Load Phase) ==");
	bond_addr_saved = false;
	bt_foreach_bond(BT_ID_DEFAULT, bond_info_cb, NULL);

	TEST_ASSERT(bond_addr_saved, "Bond was not found after settings_load");

	unpair(BT_ID_DEFAULT);

	TEST_PASS("PASS");
}

void peripheral_save(void)
{
	bs_bt_utils_setup();

	LOG_INF("== Bonding (Save Phase) ==");
	BUILD_ASSERT(CONFIG_BT_BONDABLE, "CONFIG_BT_BONDABLE must be enabled by default.");
	advertise_connectable(BT_ID_DEFAULT, NULL);
	wait_connected();
	/* Central should bond here and trigger a disconnect. */
	wait_disconnected();
	TAKE_FLAG(flag_pairing_complete);
	TAKE_FLAG(flag_bonded);

	k_sleep(K_MSEC(CONFIG_BT_SETTINGS_DELAYED_STORE_MS + 100));

	clear_test_conn();

	LOG_INF("Bond saved");
	TEST_PASS("PASS");
}

void peripheral_load(void)
{
	bs_bt_utils_setup();

	LOG_INF("== Loading Settings (Load Phase) ==");
	bond_addr_saved = false;
	bt_foreach_bond(BT_ID_DEFAULT, bond_info_cb, NULL);

	TEST_ASSERT(bond_addr_saved, "Bond was not found after settings_load");

	unpair(BT_ID_DEFAULT);

	TEST_PASS("PASS");
}


static const struct bst_test_instance test_to_add[] = {
	{
		.test_id = "central_save",
		.test_main_f = central_save,
	},
	{
		.test_id = "central_load",
		.test_main_f = central_load,
	},
	{
		.test_id = "peripheral_save",
		.test_main_f = peripheral_save,
	},
	{
		.test_id = "peripheral_load",
		.test_main_f = peripheral_load,
	},
	BSTEST_END_MARKER,
};

static struct bst_test_list *install(struct bst_test_list *tests)
{
	return bst_add_tests(tests, test_to_add);
};

bst_test_install_t test_installers[] = {install, NULL};

int main(void)
{
	bst_main();
	return 0;
}
