#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <extra_profiles/hid_profile.h>
#include <ble_glue/gap.h>

typedef struct {
    char name[FURI_HAL_BT_ADV_NAME_LENGTH];
    uint8_t mac[GAP_MAC_ADDR_SIZE];
    bool bonding;
    GapPairing pairing;
} AmiTabletBleProfileParams;

extern const FuriHalBleProfileTemplate* amitablet_ble_profile;
