#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/services/nus.h>
#include <zephyr/fs/fs.h>
#include <string.h>
#include "message.h"
#include "log.h"

LOG_MODULE_REGISTER(log_read, LOG_LEVEL_DBG);

void send_message(char *buf, size_t len, worker_t *worker) {
    struct tx_msg tx_msg;
    memcpy(tx_msg.data, buf, len);
    tx_msg.len = len;
    if (k_msgq_put(worker->tx_msg, &tx_msg, K_NO_WAIT) != 0) {
        LOG_WRN("log data TX queue full");
    } else {
        LOG_DBG("log data enqueued");
    }
}

void send_log_file(worker_t *worker, const char *filepath) {
    struct fs_file_t file;
    fs_file_t_init(&file);

    if (fs_open(&file, filepath, FS_O_READ) != 0) {
        char err_msg[] = "Failed to open log file\r\n";
        send_message(err_msg, strlen(err_msg), worker);
        return;
    }

    char buf[128];           // 每次发送块大小
    ssize_t br;
    while ((br = fs_read(&file, buf, 128)) > 0) {
        send_message(buf, br, worker);
        k_sleep(K_MSEC(20)); // 避免BLE发送过载，适当延时
    }
    fs_close(&file);
}

/* ---------- 发送所有日志（遍历目录） ---------- */
uint8_t send_all_logs(worker_t *worker) {
    const char *dir_path = "/lfs";
    struct fs_dir_t dir;
    fs_dir_t_init(&dir);

    if (fs_opendir(&dir, dir_path) != 0) {
        char err_msg[] = "Failed to open /lfs\r\n";    
        send_message(err_msg, strlen(err_msg), worker);
        return 0;
    }

    struct fs_dirent entry;
    while (fs_readdir(&dir, &entry) == 0 && entry.name[0] != 0) {
        if (entry.type == FS_DIR_ENTRY_FILE) {
            /* 构造完整路径 */
            char full_path[264];
            snprintf(full_path, 264, "%s/%s", dir_path, entry.name);
            /* 发送文件头信息 */
            char header[280];
            snprintf(header, 280, "\r\n=== File: %s ===\r\n", entry.name);
            send_message(header, strlen(header), worker);
            send_log_file(worker, full_path);
        }
    }
    fs_closedir(&dir);

    /* 发送结束标记 */
    char footer[] = "\r\n=== END OF LOGS ===\r\n";
    send_message(footer, strlen(footer), worker);
    return 1;
}
