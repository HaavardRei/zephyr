#!/usr/bin/env bash
# Copyright 2023 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

set -eu
source ${ZEPHYR_BASE}/tests/bsim/sh_common.source

verbosity_level=2

simulation_id="security_bond_persistence"

central_exe="${BSIM_OUT_PATH}/bin/bs_${BOARD_TS}_$(guess_test_long_name)_prj_conf"
peripheral_exe="${central_exe}"

cd ${BSIM_OUT_PATH}/bin

# Test persistence: Save phase - bond and save to flash
Execute "$central_exe" \
    -v=${verbosity_level} -s="${simulation_id}_save" -d=0 -testid=central_save \
    -RealEncryption=1 -flash="${simulation_id}_central.bin" -flash_erase

Execute "$peripheral_exe" \
    -v=${verbosity_level} -s="${simulation_id}_save" -d=1 -testid=peripheral_save \
    -RealEncryption=1 -flash="${simulation_id}_peripheral.bin" -flash_erase

Execute ./bs_2G4_phy_v1 -v=${verbosity_level} -s="${simulation_id}_save" \
    -D=2 -sim_length=60e6 $@

wait_for_background_jobs

# Test persistence: Load phase - load from flash and verify bond exists
Execute "$central_exe" \
    -v=${verbosity_level} -s="${simulation_id}_load" -d=0 -testid=central_load \
    -RealEncryption=1 -flash="${simulation_id}_central.bin" -flash_rm

Execute "$peripheral_exe" \
    -v=${verbosity_level} -s="${simulation_id}_load" -d=1 -testid=peripheral_load \
    -RealEncryption=1 -flash="${simulation_id}_peripheral.bin" -flash_rm

Execute ./bs_2G4_phy_v1 -v=${verbosity_level} -s="${simulation_id}_load" \
    -D=2 -sim_length=60e6 $@

wait_for_background_jobs
