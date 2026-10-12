#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/reboot.h>
#include <nfc_t4t_lib.h>
#include <nfc/ndef/uri_msg.h>
#include <nfc/ndef/uri_rec.h>
#include "nfc_handlers.h"

LOG_MODULE_REGISTER(nfc_handler, LOG_LEVEL_INF);

#define SETTINGS_MAC_KEY "ble/mac"
#define NFC_NDEF_MSG_BUF_SIZE 256
static uint8_t ndef_msg_buf[NFC_NDEF_MSG_BUF_SIZE];

static void nfc_t4t_callback(void *context, nfc_t4t_event_t event,
                             const uint8_t *data, size_t data_length,
                             uint32_t flags)
{
    ARG_UNUSED(context);
    ARG_UNUSED(flags);

    switch (event) {
    case NFC_T4T_EVENT_FIELD_ON:
        LOG_INF("NFC Field detected");
        break;
    case NFC_T4T_EVENT_FIELD_OFF:
        LOG_INF("NFC Field lost");
        break;
    case NFC_T4T_EVENT_NDEF_UPDATED:
        LOG_INF("NFC NDEF updated! len=%zu", data_length);
        LOG_HEXDUMP_INF(data, data_length, "NFC Write Data:");
        if (data_length == 6) {
            int err = settings_save_one(SETTINGS_MAC_KEY, data, 6);
            if (err == 0) {
                LOG_INF("New MAC saved, rebooting to apply...");
                k_sleep(K_MSEC(500));
                sys_reboot(SYS_REBOOT_COLD);
            } else {
                LOG_ERR("Failed to save MAC: %d", err);
            }
        }
        break;
    default:
        break;
    }
}

int nfc_init(void)
{
    int err;

    err = nfc_t4t_setup(nfc_t4t_callback, NULL);
    if (err) {
        LOG_ERR("Cannot setup NFC T4T library (err: %d)", err);
        return err;
    }

    const uint8_t uri[] = "https://www.nordicsemi.com";
    uint32_t len = sizeof(ndef_msg_buf);
    err = nfc_ndef_uri_msg_encode(NFC_URI_HTTPS, uri, sizeof(uri) - 1,
                                   ndef_msg_buf, &len);
    if (err) {
        LOG_ERR("Cannot encode NDEF message (err: %d)", err);
        return err;
    }

    err = nfc_t4t_ndef_rwpayload_set(ndef_msg_buf, len);
    if (err) {
        LOG_ERR("Cannot set NDEF field (err: %d)", err);
        return err;
    }

    err = nfc_t4t_emulation_start();
    if (err) {
        LOG_ERR("Cannot start NFC emulation (err: %d)", err);
        return err;
    }

    LOG_INF("NFC T4T initialized and emulation started");
    return 0;
}

void nfc_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);
    if (nfc_init() < 0) {
        LOG_ERR("NFC init failed, thread exiting");
        return;
    }
    while (1) {
        k_sleep(K_SECONDS(10));
    }
}