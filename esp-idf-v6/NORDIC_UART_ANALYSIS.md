# Comprehensive Analysis: Nordic UART Service Implementation with NimBLE

## Executive Summary

Based on analysis of the ESP32 BLE UART project and NimBLE framework patterns, this document provides detailed guidance on implementing Nordic UART Service (NUS) for BLE peripheral applications.

---

## 1. SERVICE REGISTRATION PATTERNS (GATT TABLE STRUCTURE)

### 1.1 GATT Service Definition Format

The GATT service table is defined as a static array of `ble_gatt_svc_def` structures:

```c
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,      // Must be PRIMARY for Nordic UART
        .uuid = &gatt_svr_svc_uuid.u,           // Service UUID pointer
        .characteristics = (struct ble_gatt_chr_def[]) {
            // Characteristic definitions array
            { 0 }  // Terminator
        },
    },
    { 0 }  // Terminator
};
```

### 1.2 Critical: Characteristic Order for Nordic UART Service

**KEY FINDING**: Android NimBLE applications require RX BEFORE TX in GATT definition.

**Incorrect Order** (causes "no serial profile found"):
```c
.characteristics = (struct ble_gatt_chr_def[]) {
    { // TX first
        .uuid = &gatt_svr_chr_tx_uuid.u,  // 6e400003-...
        .flags = BLE_GATT_CHR_F_NOTIFY,
    }, {
        // RX second - WRONG ORDER!
        .uuid = &gatt_svr_chr_rx_uuid.u,  // 6e400002-...
        .flags = BLE_GATT_CHR_F_WRITE,
    },
    { 0 }
};
```

**Correct Order**:
```c
.characteristics = (struct ble_gatt_chr_def[]) {
    { // RX FIRST (Write)
        .uuid = &gatt_svr_chr_rx_uuid.u,  // 6e400002-b5a3-f393-e0a9-e50e24dcca9e
        .access_cb = gatt_svr_chr_access_rx,
        .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
        .val_handle = &rx_char_val_handle
    }, {
        // TX SECOND (Notify + Read)
        .uuid = &gatt_svr_chr_tx_uuid.u,  // 6e400003-b5a3-f393-e0a9-e50e24dcca9e
        .access_cb = gatt_svr_chr_access_tx,
        .val_handle = &tx_char_val_handle,
        .flags = BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_READ,
        .descriptors = (struct ble_gatt_dsc_def[]) {
            {
                .uuid = BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16),
                .att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
                .access_cb = gatt_svr_chr_access_tx,  // CCCD uses TX callback
            }, { 0 }
        },
    }, { 0 }
};
```

### 1.3 Nordic UART Service UUID Definitions

```c
// Service UUID: 6e400001-b5a3-f393-e0a9-e50e24dcca9e
static const ble_uuid128_t gatt_svr_svc_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);

// TX UUID (notify): 6e400003-b5a3-f393-e0a9-e50e24dcca9e
static const ble_uuid128_t gatt_svr_chr_tx_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e);

// RX UUID (write): 6e400002-b5a3-f393-e0a9-e50e24dcca9e
static const ble_uuid128_t gatt_svr_chr_rx_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e);
```

### 1.4 GATT Registration Flow

Services must be registered in this order:

```c
// Step 1: Setup sync callback (called when BLE host is ready)
ble_hs_cfg.sync_cb = on_ble_sync;
ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;

// Step 2: Count services and characteristics
int rc = ble_gatts_count_cfg(gatt_svr_svcs);
if (rc != 0) return error;

// Step 3: Register GAP and GATT base services
ble_svc_gap_init();
ble_svc_gatt_init();

// Step 4: Register custom services
rc = ble_gatts_add_svcs(gatt_svr_svcs);
if (rc != 0) return error;

// Step 5: Start the NimBLE host
nimble_port_freertos_init(ble_host_task);
```

---

## 2. ADVERTISING CONFIGURATION (128-bit UUIDs)

### 2.1 Nordic UART Service Discovery

For Android/iOS clients to find your Nordic UART service, include the service UUID in advertising data:

```c
static void ble_advertise(void) {
    struct ble_hs_adv_fields adv_fields = {0};
    
    // Required flags
    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    
    // TX Power (optional but recommended)
    adv_fields.tx_pwr_lvl_is_present = 1;
    adv_fields.tx_pwr_lvl = 9;  // dBm
    
    // CRITICAL: Include Nordic UART Service UUID in advertising
    // This allows apps to filter by service UUID
    adv_fields.uuids128 = &gatt_svr_svc_uuid;
    adv_fields.num_uuids128 = 1;
    adv_fields.uuids128_is_complete = 1;
    
    int rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set advertising fields: %d", rc);
        return;
    }
    
    // Scan response carries device name (separate from advertising)
    struct ble_hs_adv_fields rsp_fields = {0};
    rsp_fields.name = (uint8_t *)"DeviceName";
    rsp_fields.name_len = strlen("DeviceName");
    rsp_fields.name_is_complete = 1;
    
    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set scan response: %d", rc);
        return;
    }
    
    // Configure advertising parameters
    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;  // Undirected (all devices can connect)
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;  // General discoverable
    
    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                          &adv_params, gap_event_handler, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to start advertising: %d", rc);
        return;
    }
}
```

### 2.2 Advertisement Data Format

**Advertising Packet (31 bytes max)**:
- Flags (3 bytes)
- TX Power (3 bytes)
- 128-bit Service UUID (20 bytes)
- **Total: 26 bytes** ✓

**Scan Response Packet (31 bytes max)**:
- Device name (variable)

### 2.3 128-bit UUID Byte Order

NimBLE uses **Little-Endian** format for UUIDs. When specifying UUID `6e400001-b5a3-f393-e0a9-e50e24dcca9e`:

```
Correct order in BLE_UUID128_INIT():
BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                 0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e)
                 ^reversed first 8 bytes  ^   ^   ^   ^
                                            reversed last 8 bytes
```

---

## 3. GAP EVENT CALLBACKS

### 3.1 Required Event Types

```c
static int gap_event_handler(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        return handle_connect_event(event);
    
    case BLE_GAP_EVENT_DISCONNECT:
        return handle_disconnect_event(event);
    
    case BLE_GAP_EVENT_MTU:
        return handle_mtu_event(event);
    
    case BLE_GAP_EVENT_ENC_CHANGE:
        return handle_encryption_event(event);
    
    case BLE_GAP_EVENT_PASSKEY_ACTION:
        return handle_passkey_event(event);
    
    case BLE_GAP_EVENT_REPEAT_PAIRING:
        return handle_repeat_pairing_event(event);
    
    case BLE_GAP_EVENT_ADV_COMPLETE:
        // Restart advertising if needed
        ble_advertise();
        break;
    
    default:
        break;
    }
    return 0;
}
```

### 3.2 Connection Event Handling

```c
static int handle_connect_event(struct ble_gap_event *event) {
    if (event->connect.status != 0) {
        ESP_LOGW(TAG, "Connection failed, status=%d", event->connect.status);
        ble_advertise();  // Restart advertising
        return 0;
    }
    
    // Store connection handle
    conn_handle = event->connect.conn_handle;
    ESP_LOGI(TAG, "BLE client connected: %d", conn_handle);
    
    // Optimize connection parameters for throughput
    struct ble_gap_upd_params params = {
        .itvl_min = 0x0006,      // 6 * 1.25ms = 7.5ms
        .itvl_max = 0x000C,      // 12 * 1.25ms = 15ms
        .latency = 0,
        .supervision_timeout = 500,  // 500 * 10ms = 5 seconds
    };
    int rc = ble_gap_update_params(conn_handle, &params);
    if (rc != 0) {
        ESP_LOGW(TAG, "Failed to update params: %d", rc);
    }
    
    // Initiate pairing/security if required
    rc = ble_gap_security_initiate(conn_handle);
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGW(TAG, "Security initiate failed: %d", rc);
    }
    
    return 0;
}
```

### 3.3 Disconnection and Cleanup

```c
static int handle_disconnect_event(struct ble_gap_event *event) {
    ESP_LOGI(TAG, "BLE client disconnected, reason=%d", 
             event->disconnect.reason);
    
    // Reset connection state
    conn_handle = BLE_HS_CONN_HANDLE_NONE;
    notify_enabled = false;
    
    // Restart advertising
    ble_advertise();
    
    return 0;
}
```

### 3.4 MTU Exchange

```c
static int handle_mtu_event(struct ble_gap_event *event) {
    ESP_LOGI(TAG, "MTU updated: %d bytes (conn_handle=%d)",
             event->mtu.value, event->mtu.conn_handle);
    
    // Adjust send buffer sizes based on MTU
    // Formula: max_payload = MTU - 3 (ATT header)
    return 0;
}
```

### 3.5 Pairing and Security

```c
static int handle_passkey_event(struct ble_gap_event *event) {
    struct ble_sm_io pkey = {0};
    pkey.action = event->passkey.params.action;
    
    if (event->passkey.params.action == BLE_SM_IOACT_DISP) {
        // Display passkey (for Display Only)
        pkey.passkey = 123456;  // Fixed PIN
        ESP_LOGI(TAG, "PIN: %06lu", (unsigned long)pkey.passkey);
    } else if (event->passkey.params.action == BLE_SM_IOACT_NUMCMP) {
        // Numeric comparison
        pkey.numcmp_accept = 1;
    }
    
    int rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
    if (rc != 0) {
        ESP_LOGW(TAG, "Passkey injection failed: %d", rc);
    }
    
    return 0;
}
```

---

## 4. CHARACTERISTICS WITH NOTIFICATIONS

### 4.1 RX Characteristic (Write-only)

```c
static int gatt_svr_chr_access_rx(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        // Extract written data from mbuf
        uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
        uint8_t data[512];
        
        if (len > sizeof(data)) len = sizeof(data);
        os_mbuf_copydata(ctxt->om, 0, len, data);
        
        // Process received data
        // Note: BLE_GATT_CHR_F_WRITE_NO_RSP doesn't wait for response
        ESP_LOGI(TAG, "RX received %d bytes", len);
        
        // Store in ring buffer for UART TX
        ring_buffer_write(g_rx_buffer, data, len);
        
        return 0;  // BLE_HS_ENOTCONN if not connected
    }
    return BLE_ATT_ERR_UNLIKELY;
}
```

### 4.2 TX Characteristic (Notify + Read)

```c
static int gatt_svr_chr_access_tx(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt, void *arg) {
    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        // Client reads TX characteristic value
        // This allows clients to read buffered data on demand
        {
            uint8_t temp_buf[256];
            size_t to_read = ring_buffer_available(g_tx_buffer);
            if (to_read > 256) to_read = 256;
            
            if (to_read > 0) {
                size_t read = ring_buffer_read(g_tx_buffer, temp_buf, to_read);
                int rc = os_mbuf_append(ctxt->om, temp_buf, read);
                return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            }
        }
        return 0;
    
    case BLE_GATT_ACCESS_OP_READ_DSC:
        // Client reads CCCD descriptor value
        // Returns current notification/indication status
        {
            uint16_t val = notify_enabled ? 0x0001 : 0x0000;
            int rc = os_mbuf_append(ctxt->om, &val, sizeof(val));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
    
    case BLE_GATT_ACCESS_OP_WRITE_DSC:
        // Client writes CCCD descriptor (subscribes to notifications)
        // 0x0001 = notifications, 0x0002 = indications
        {
            uint16_t val = 0;
            os_mbuf_copydata(ctxt->om, 0, sizeof(val), &val);
            notify_enabled = (val == 0x0001);
            ESP_LOGI(TAG, "TX notifications: %s", notify_enabled ? "ENABLED" : "DISABLED");
        }
        return 0;
    
    default:
        return BLE_ATT_ERR_UNLIKELY;
    }
}
```

### 4.3 Sending Notifications

```c
void ble_notify_tx_data(const uint8_t *data, size_t len) {
    if (conn_handle == BLE_HS_CONN_HANDLE_NONE || !notify_enabled) {
        return;  // No client or notifications not enabled
    }
    
    // Get current MTU (includes 3-byte ATT header)
    uint16_t mtu = ble_att_mtu(conn_handle);
    size_t max_payload = mtu - 3;
    
    // Send data in chunks
    for (size_t offset = 0; offset < len; offset += max_payload) {
        size_t chunk_size = (len - offset) > max_payload ? 
                           max_payload : (len - offset);
        
        // Create mbuf from flat buffer
        struct os_mbuf *om = ble_hs_mbuf_from_flat(data + offset, chunk_size);
        if (!om) {
            ESP_LOGE(TAG, "Failed to allocate mbuf");
            break;
        }
        
        // Send notification
        int rc = ble_gatts_notify_custom(conn_handle, tx_char_val_handle, om);
        if (rc != 0) {
            ESP_LOGW(TAG, "Notification failed: %d", rc);
            os_mbuf_free_chain(om);
            break;
        }
    }
}
```

### 4.4 CCCD Descriptor (Client Characteristic Configuration)

```c
// Part of TX characteristic definition
.descriptors = (struct ble_gatt_dsc_def[]) {
    {
        // CCCD UUID (always 0x2902 for notifications)
        .uuid = BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16),
        // Readable and writable
        .att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
        // Use TX characteristic callback to handle CCCD writes
        .access_cb = gatt_svr_chr_access_tx,
    }, { 0 }
}
```

---

## 5. INITIALIZATION SEQUENCE (CRITICAL)

### 5.1 Correct Initialization Order

```c
esp_err_t ble_service_init(void) {
    ESP_LOGI(TAG, "Initializing BLE...");
    
    // STEP 0: Release classic Bluetooth memory
    esp_err_t ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to release classic BT: %s", esp_err_to_name(ret));
    }
    
    // STEP 1: Initialize NimBLE port (auto-initializes BT controller)
    // DO NOT manually call esp_bt_controller_init() - nimble_port_init() does it!
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "NimBLE port initialized (controller auto-enabled)");
    
    // STEP 2: Configure host callbacks and security BEFORE starting
    ble_hs_cfg.sync_cb = on_ble_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    
    // Security settings
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_DISPLAY_ONLY;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 1;
    ble_hs_cfg.sm_sc = 0;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | 
                                 BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | 
                                   BLE_SM_PAIR_KEY_DIST_ID;
    
    // STEP 3: Initialize GAP and GATT base services
    ble_svc_gap_init();
    ble_svc_gatt_init();
    
    // STEP 4: Register custom Nordic UART service
    int rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg failed: %d", rc);
        return ESP_FAIL;
    }
    
    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs failed: %d", rc);
        return ESP_FAIL;
    }
    
    // STEP 5: Set device name
    rc = ble_svc_gap_device_name_set("DeviceName");
    if (rc != 0) {
        ESP_LOGW(TAG, "Failed to set device name: %d", rc);
    }
    
    // STEP 6: Initialize bonding storage
    ble_store_config_init();
    
    // STEP 7: Start NimBLE host task
    // on_ble_sync() callback will be called when host is ready
    nimble_port_freertos_init(ble_host_task);
    
    ESP_LOGI(TAG, "BLE initialization complete");
    return ESP_OK;
}
```

### 5.2 Common Initialization Mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Manual `esp_bt_controller_init()` before `nimble_port_init()` | `ESP_ERR_INVALID_STATE` | Remove manual init, let `nimble_port_init()` do it |
| Missing `ble_store_config_init()` | Bonding doesn't persist | Add after GATT service registration |
| Configuring `ble_hs_cfg` after `nimble_port_init()` | Settings ignored | Configure BEFORE calling `nimble_port_init()` |
| Wrong GATT service order | "no serial profile found" error | Put RX before TX characteristic |
| Missing 128-bit UUID in advertising | Apps can't find service | Add `adv_fields.uuids128` to advertising |

---

## 6. SPECIAL CONFIGURATIONS FOR SERVICE VISIBILITY

### 6.1 GAP Service Configuration

```c
// Automatically configured by ble_svc_gap_init()
// Sets appearance and device flags
ble_svc_gap_init();

// Manually set device name if needed
ble_svc_gap_device_name_set("MyNordicUART");

// Optional: Set appearance code
// Appearance codes: https://www.bluetooth.com/specifications/assigned-numbers/gatt-namespace-descriptors
ble_svc_gap_device_appearance_set(BLE_APPEARANCE_GENERIC_COMPUTER);
```

### 6.2 GATT Service Configuration

```c
// Automatically configured by ble_svc_gatt_init()
// Exposes GATT service for discovery
ble_svc_gatt_init();
```

### 6.3 Advertising Flags and Discoverability

```c
struct ble_hs_adv_fields adv_fields = {0};

// Flags: Make device discoverable
adv_fields.flags = BLE_HS_ADV_F_DISC_GEN |    // General discoverable
                   BLE_HS_ADV_F_BREDR_UNSUP;  // BR/EDR not supported

// Service UUID: Nordic UART Service
adv_fields.uuids128 = &gatt_svr_svc_uuid;
adv_fields.num_uuids128 = 1;
adv_fields.uuids128_is_complete = 1;  // All services listed

// TX Power
adv_fields.tx_pwr_lvl_is_present = 1;
adv_fields.tx_pwr_lvl = 9;  // dBm

ble_gap_adv_set_fields(&adv_fields);
```

### 6.4 Periodic Re-advertising

```c
case BLE_GAP_EVENT_ADV_COMPLETE:
    // Advertising stopped - restart
    ESP_LOGI(TAG, "Advertising completed, restarting");
    ble_advertise();
    break;
```

---

## 7. CONFIGURATION PARAMETERS FOR PERFORMANCE

### 7.1 Connection Parameters

```c
struct ble_gap_upd_params params = {
    // Connection interval: 7.5-15ms for maximum throughput
    .itvl_min = 0x0006,      // 6 * 1.25ms = 7.5ms
    .itvl_max = 0x000C,      // 12 * 1.25ms = 15ms
    
    // Slave latency (number of connection events device can skip)
    .latency = 0,            // No latency for real-time data
    
    // Supervision timeout
    .supervision_timeout = 500,  // 500 * 10ms = 5 seconds
    
    // Min/max connection event duration
    .min_ce_len = 0x0010,
    .max_ce_len = 0x0300
};
ble_gap_update_params(conn_handle, &params);
```

### 7.2 MTU Configuration

```c
// Set preferred MTU (for notification payload size)
ble_att_set_preferred_mtu(517);  // Max for BLE 4.2+

// Get actual MTU after negotiation
uint16_t mtu = ble_att_mtu(conn_handle);
size_t max_payload = mtu - 3;  // Subtract 3-byte ATT header
```

### 7.3 PHY Selection for 2Mbps

```c
// Set preferred PHY to 2M for higher throughput
ble_gap_set_prefered_default_le_phy(BLE_GAP_LE_PHY_2M_MASK,
                                     BLE_GAP_LE_PHY_2M_MASK);
```

---

## 8. EXAMPLE: COMPLETE NORDIC UART SERVICE

### Minimal Implementation

```c
// UUIDs
static const ble_uuid128_t uart_svc_uuid = BLE_UUID128_INIT(
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);
static const ble_uuid128_t uart_tx_uuid = BLE_UUID128_INIT(
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e);
static const ble_uuid128_t uart_rx_uuid = BLE_UUID128_INIT(
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e);

// State
static uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t tx_val_handle;
static uint16_t rx_val_handle;
static bool notify_enabled = false;

// RX Characteristic Callback
static int uart_rx_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
        uint8_t buf[512];
        os_mbuf_copydata(ctxt->om, 0, len > 512 ? 512 : len, buf);
        // Process received data
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

// TX Characteristic Callback
static int uart_tx_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_DSC) {
        uint16_t val = 0;
        os_mbuf_copydata(ctxt->om, 0, sizeof(val), &val);
        notify_enabled = (val == 0x0001);
    }
    return 0;
}

// GATT Service Definition
static const struct ble_gatt_svc_def uart_svc[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &uart_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &uart_rx_uuid.u,
                .access_cb = uart_rx_access_cb,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
                .val_handle = &rx_val_handle,
            }, {
                .uuid = &uart_tx_uuid.u,
                .access_cb = uart_tx_access_cb,
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &tx_val_handle,
                .descriptors = (struct ble_gatt_dsc_def[]) {
                    {
                        .uuid = BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16),
                        .att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
                        .access_cb = uart_tx_access_cb,
                    }, { 0 }
                },
            }, { 0 }
        },
    },
    { 0 }
};

// GAP Event Handler
static int gap_event_handler(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status != 0) {
            ble_advertise();
        } else {
            conn_handle = event->connect.conn_handle;
            ble_gap_security_initiate(conn_handle);
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        conn_handle = BLE_HS_CONN_HANDLE_NONE;
        notify_enabled = false;
        ble_advertise();
        break;
    }
    return 0;
}

// Advertising
static void ble_advertise(void) {
    struct ble_hs_adv_fields adv_fields = {0};
    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    adv_fields.uuids128 = &uart_svc_uuid;
    adv_fields.num_uuids128 = 1;
    adv_fields.uuids128_is_complete = 1;
    
    ble_gap_adv_set_fields(&adv_fields);
    
    struct ble_gap_adv_params params = {
        .conn_mode = BLE_GAP_CONN_MODE_UND,
        .disc_mode = BLE_GAP_DISC_MODE_GEN,
    };
    ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                      &params, gap_event_handler, NULL);
}

// Sync callback (called when host is ready)
static void on_ble_sync(void) {
    ble_advertise();
}

// Host task
static void ble_host_task(void *param) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}

// Initialization
esp_err_t uart_service_init(void) {
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    
    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) return ret;
    
    ble_hs_cfg.sync_cb = on_ble_sync;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_DISPLAY_ONLY;
    
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set("UARTDevice");
    
    ble_gatts_count_cfg(uart_svc);
    ble_gatts_add_svcs(uart_svc);
    
    ble_store_config_init();
    nimble_port_freertos_init(ble_host_task);
    
    return ESP_OK;
}

// Send notification
void uart_notify(const uint8_t *data, size_t len) {
    if (conn_handle == BLE_HS_CONN_HANDLE_NONE || !notify_enabled) return;
    
    uint16_t mtu = ble_att_mtu(conn_handle);
    size_t max_chunk = mtu - 3;
    
    for (size_t i = 0; i < len; i += max_chunk) {
        size_t chunk = (len - i) > max_chunk ? max_chunk : (len - i);
        struct os_mbuf *om = ble_hs_mbuf_from_flat(data + i, chunk);
        if (om) {
            ble_gatts_notify_custom(conn_handle, tx_val_handle, om);
        }
    }
}
```

---

## 9. DEBUGGING CHECKLIST

- [ ] Service UUID appears in `Registered service` log messages
- [ ] RX characteristic registered before TX (check `Registered characteristic` order)
- [ ] CCCD descriptor registered (separate `Registered descriptor` message)
- [ ] Advertising includes 128-bit UUID (`uuids128_is_complete = 1`)
- [ ] Device name in scan response
- [ ] Connection callback fires when client connects
- [ ] CCCD write callback fires when client enables notifications
- [ ] MTU negotiation completes (MTU event)
- [ ] Notifications send successfully (no `ble_gatts_notify_custom` errors)
- [ ] Bonding storage initialized (`ble_store_config_init()` called)

---

## 10. REFERENCE PATTERNS

### Service Registration Pattern
```
1. Define UUID 128-bit constants
2. Create characteristic definitions with callbacks
3. Create service definition with UUID and characteristics
4. Count and register services in ble_hs_cfg.sync_cb callback
5. Handle events in GAP callback
```

### Notification Flow Pattern
```
1. Client connects → store conn_handle
2. Client enables notifications → set notify_enabled flag
3. Application data ready → check flags, get MTU
4. Split data by MTU, create mbufs, send notifications
5. Client disconnects → reset flags
```

### Security Pattern
```
1. Configure ble_hs_cfg.sm_* settings
2. Handle BLE_GAP_EVENT_PASSKEY_ACTION
3. Inject PIN via ble_sm_inject_io()
4. Initialize storage via ble_store_config_init()
```

