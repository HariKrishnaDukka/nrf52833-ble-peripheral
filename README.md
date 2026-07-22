# nRF52833 BLE Peripheral — Custom GATT Service Demo

A learning project demonstrating a complete BLE peripheral firmware built on the **nRF52833-DK** using **nRF Connect SDK (NCS) v2.7.0** (Zephyr-based). The device advertises, accepts connections, exposes a custom GATT service, and streams live data to a mobile app via Read/Notify operations.

---

## 1. Project Overview

This firmware turns the nRF52833-DK into a **BLE Peripheral (GAP)** that:
- Advertises itself as `NRF52833_Custom`
- Accepts a connection from a Central device (e.g., a phone)
- Exposes a **custom GATT Service** containing one **Characteristic**
- That characteristic supports:
  - **READ** — phone can request the current value on demand
  - **NOTIFY** — phone can subscribe and receive automatic updates every 2 seconds
- Internally increments a counter every 2 seconds and pushes it as live data

This project is the foundational "Hello World" of BLE peripheral development — it validates the full connection lifecycle (advertise → connect → discover → read/notify) before layering on more complex features like write control, security, and sensor integration.

---

## 2. Hardware & Software Stack

| Component | Details |
|---|---|
| **Board** | nRF52833-DK (nRF52833_xxAA) |
| **SDK** | nRF Connect SDK (NCS) v2.7.0 |
| **RTOS** | Zephyr v3.6.99 (NCS variant) |
| **BLE Stack** | SoftDevice Controller (nrfxlib) + Zephyr Bluetooth Host |
| **Build System** | CMake + Ninja, driven by `west` |
| **OS (dev machine)** | Linux |
| **Test Client** | nRF Connect for Mobile app |

---

## 3. Project Structure

```
ble_peripheral_custom/
├── CMakeLists.txt          # Build configuration, links source files
├── prj.conf                 # Kconfig options (enables BT, logging, stack sizes)
├── src/
│   ├── main.c                # Entry point: BT init, advertising, connection callbacks
│   ├── custom_service.c       # GATT service/characteristic definition + handlers
│   └── custom_service.h       # Public API for the custom service
└── README.md
```

---

## 4. BLE Architecture — How the Pieces Fit

### 4.1 GAP Layer (Connection Management)
- **Role:** Peripheral (the DK never initiates connections; it advertises and waits)
- **Advertising:** Uses `BT_LE_ADV_CONN` — general connectable advertising with default fast intervals
- **Connection callbacks:** `connected()` / `disconnected()` registered via `BT_CONN_CB_DEFINE`

### 4.2 GATT Layer (Data Structure)
```
Service: Custom Service (128-bit UUID: 12345678-1234-5678-1234-56789abcdef0)
   └── Characteristic: Custom Value (128-bit UUID: 12345678-1234-5678-1234-56789abcdef1)
         Properties: READ, NOTIFY
         Backing variable: uint8_t custom_value
```

- **Service UUID** groups the characteristic under one identifiable container — it carries no data or ability itself, purely identity + grouping.
- **Characteristic UUID** uniquely addresses the actual data point. All read/notify traffic targets this UUID (via its GATT handle, resolved during discovery).
- **Property flags** (`BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY`) declare what operations the phone is allowed to perform — independent of the UUID itself.
- **CCCD (Client Characteristic Configuration Descriptor)** — an auto-added descriptor that the phone writes to (`0x0001`) to subscribe to notifications. Tracked here via the `ccc_cfg_changed()` callback, which sets `notify_enabled`.

### 4.3 Data Flow
```
main.c loop (every 2s)
   → custom_service_notify(counter)
        → updates custom_value
        → if notify_enabled: bt_gatt_notify() pushes update to phone
Phone taps "Read"
   → read_custom_char() callback fires
        → returns current custom_value on demand
```

Both Read and Notify operate on the **same backing variable** — a single source of truth exposed two ways: passively (subscribe and receive pushes) or actively (poll on request). This mirrors how real sensor characteristics (e.g., Battery Level, Heart Rate) are typically designed.

---

## 5. Connection Sequence (What Happens Over the Air)

| Stage | Description |
|---|---|
| **1. Discovery** | Phone scans and receives the DK's advertising packet (device name + flags). No data exchanged yet. |
| **2. Connection (GAP)** | Phone sends `CONNECT_IND`; DK accepts. A raw radio link is established (`HCI LE Connection Complete` event, `status 0x00`). No knowledge of services yet. |
| **3. Service/Ability Discovery (GATT)** | Phone queries the DK's GATT table, discovering the Service UUID, Characteristic UUID, and its property flags (Read/Notify). |
| **4. Operation** | Based on discovered properties, the phone reads the value on demand and/or writes `0x0001` to the CCCD to subscribe to notifications. Data now flows. |

Device addresses (e.g., `FA:79:02:B1:DA:2A` for the DK, random rotating addresses for the phone) operate at the GAP/link layer and are separate from GATT UUIDs — the address gets you connected to the *device*; the UUID then addresses specific *data* inside it.

---

## 6. Key Firmware Files Explained

### `prj.conf`
Enables the Bluetooth subsystem and sets the device's advertised name:
```conf
CONFIG_BT=y
CONFIG_BT_PERIPHERAL=y
CONFIG_BT_DEVICE_NAME="NRF52833_Custom"
CONFIG_BT_SMP=n
CONFIG_LOG=y
CONFIG_BT_LOG_LEVEL_INF=y
CONFIG_MAIN_STACK_SIZE=2048
CONFIG_BT_HCI_TX_STACK_SIZE=1024
CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE=2048
CONFIG_ISR_STACK_SIZE=2048
```
> Note: Logging is kept at `INF` level (not `DBG`) with default stack sizes increased — an earlier `DBG` + immediate-log configuration caused a **stack overflow crash** on connection events due to excessive logging inside HCI callback context. This is a common pitfall in early Zephyr BLE projects.

### `src/main.c`
- Initializes Bluetooth (`bt_enable`)
- Starts advertising once the stack is ready (`bt_ready` callback)
- Registers connection lifecycle callbacks
- Runs the main loop that updates and notifies the counter value every 2 seconds

### `src/custom_service.c` / `.h`
- Defines the custom Service and Characteristic using `BT_GATT_SERVICE_DEFINE`
- Implements `read_custom_char()` — the READ callback
- Implements `ccc_cfg_changed()` — tracks notification subscription state
- Implements `custom_service_notify()` — pushes updated values when subscribed

---

## 7. Build & Flash Instructions

```bash
cd ~/ncs/v2.7.0/my_apps/ble_peripheral_custom
west build -b nrf52833dk_nrf52833 -p always
west flash
```
- `-p always` forces a clean/pristine build, avoiding stale CMake cache issues after config changes
- `west flash` uses the DK's onboard J-Link debugger over SWD/USB — a wired connection, entirely separate from BLE/GATT (no UUIDs involved in this step)

### Viewing logs
```bash
sudo screen /dev/ttyACM0 115200
```
Expected boot sequence:
```
Bluetooth initialized
Custom service initialized
Advertising started as NRF52833_Custom
Notify value: 1
Notify value: 2
...
```

---

## 8. Testing with nRF Connect for Mobile

1. Open the app → tap **Scan** → find `NRF52833_Custom` → tap **Connect**
2. Expand the discovered service (UUID starting `12345678...`)
3. Tap the characteristic → tap **Read** icon → confirm current value returns
4. Tap the **Notify (subscribe)** icon → observe the value auto-updating every 2 seconds
5. Serial log should show `Notifications enabled` once subscribed, and `Connected` should remain stable (no unexpected disconnects)

> Troubleshooting note: An immediate disconnect (`reason 19` — Remote User Terminated Connection) was observed when connecting without interacting further. This resolved once notifications were actively subscribed to in the app — this is standard behavior, not a firmware fault.

---

## 9. Concepts Validated by This Project

- ✅ GAP roles, advertising, and connection lifecycle
- ✅ GATT service/characteristic hierarchy and UUID addressing
- ✅ Difference between **identity (UUID)**, **ability (property flags)**, and **logic (callback functions)**
- ✅ CCCD-based notification subscription flow
- ✅ Debugging real Zephyr build errors (missing headers, renamed macros)
- ✅ Diagnosing and fixing a stack overflow caused by verbose logging in interrupt/callback context
- ✅ End-to-end verification using a mobile BLE inspection tool

---

## 10. Planned Next Steps

- [ ] Add a **WRITE** characteristic (e.g., LED control) — enables phone → device command flow
- [ ] Integrate GPIO control via Devicetree (`gpio_dt_spec`) for actuating LED1 on the DK
- [ ] Add **BLE security** (pairing/bonding/encryption) using `CONFIG_BT_SMP`, starting with Just Works (Level 2), optionally upgrading to Passkey Entry (Level 3)
- [ ] Persist bonding keys using `CONFIG_SETTINGS` + NVS flash storage
- [ ] Integrate real sensor data (I2C/SPI) in place of the simulated counter
- [ ] Explore BLE OTA DFU (firmware updates over Bluetooth, using Nordic's DFU service UUID)

---

## 11. References

- [nRF Connect SDK Documentation](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/index.html)
- [Zephyr Bluetooth API Reference](https://docs.zephyrproject.org/latest/connectivity/bluetooth/api/index.html)
- [Bluetooth GATT Specification Overview](https://www.bluetooth.com/specifications/specs/)
- nRF Connect for Mobile (Android/iOS) — BLE scanning/inspection tool