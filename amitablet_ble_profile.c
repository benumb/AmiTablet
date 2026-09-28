#include "amitablet_ble_profile.h"

static FuriHalBleProfileBase* amitablet_ble_profile_start(FuriHalBleProfileParams profile_params) {
    UNUSED(profile_params);
    return ble_profile_hid->start(NULL);
}

static void amitablet_ble_profile_stop(FuriHalBleProfileBase* profile) {
    ble_profile_hid->stop(profile);
}

static void amitablet_ble_profile_get_config(
    GapConfig* config,
    FuriHalBleProfileParams profile_params) {
    furi_check(config);
    furi_check(profile_params);

    AmiTabletBleProfileParams* params = profile_params;

    ble_profile_hid->get_gap_config(config, NULL);

    memcpy(config->mac_address, params->mac, sizeof(config->mac_address));

    if(params->name[0] != '\0') {
        strlcpy(config->adv_name + 1, params->name, sizeof(config->adv_name) - 1);
    }

    config->bonding_mode = params->bonding;
    config->pairing_method = params->pairing;
}

static const FuriHalBleProfileTemplate amitablet_ble_profile_callbacks = {
    .start = amitablet_ble_profile_start,
    .stop = amitablet_ble_profile_stop,
    .get_gap_config = amitablet_ble_profile_get_config,
};

const FuriHalBleProfileTemplate* amitablet_ble_profile = &amitablet_ble_profile_callbacks;
