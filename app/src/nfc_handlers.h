#ifndef NFC_HANDLERS_H_
#define NFC_HANDLERS_H_

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/reboot.h>
#include <nfc_t4t_lib.h>
#include <nfc/ndef/uri_msg.h>
#include <nfc/ndef/uri_rec.h>

int nfc_init(void);
void nfc_thread_entry(void *p1, void *p2, void *p3);
#endif