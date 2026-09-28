#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <extra_profiles/hid_profile.h>

#define AMITABLET_BLE_NAME_LENGTH 20

typedef struct {
    char name[AMITABLET_BLE_NAME_LENGTH];
    uint8_t mac[GAP_MAC_ADDR_SIZE];
    bool bonding;
    GapPairing pairing;
} AmiTabletBleProfileParams;

extern const FuriHalBleProfileTemplate* amitablet_ble_profile;
