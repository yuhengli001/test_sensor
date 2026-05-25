# CubeMX BLE Characteristic Parameters Explained

This document explains every parameter visible in the **BLE Services** tab of STM32CubeMX, covering both the top-level service configuration and the individual characteristic settings.

---

## Part 1: Top-Level BLE Services Configuration
These parameters appear at the top of the **BLE Services** tab, before you open any specific service.

### Server Mode

| Parameter | What It Means | For Our Project |
| :--- | :--- | :--- |
| **Number of services** | How many custom GATT Services your STM32 will host. | Set to **4** (Control, Vital, Fall, Vibration). |

### BLE Services Configuration

| Parameter | What It Means | For Our Project |
| :--- | :--- | :--- |
| **Peripheral Role** | The number of BLE Peripheral Role connections (i.e., how many phones can connect to this STM32 at once). | Keep at **1** — only one phone connects at a time. |
| **Central Role** | The number of BLE Central Role connections (i.e., how many other BLE devices this STM32 can connect *to*). | Keep at **0** — your STM32 is not scanning for other devices. |
| **BLE_CFG_SVC_MAX_NBR_CB** | Maximum number of GATT **attributes** (not services) the BLE stack can handle. This is a shared pool. Each characteristic typically costs 2–3 attribute slots (value + descriptor + properties). | Must be **≥ the total attribute count** across all your services. With 4 services and ~10 characteristics, set to at least **30–35**. Your current value of `7` is too low and must be increased! |
| **BLE_CFG_CLT_MAX_NBR_CB** | Maximum number of GATT **Client** callbacks — for when *this* STM32 is reading characteristics from another BLE device. | Keep at **0** — your STM32 is a server only. |
| **BLE_CFG_MAX_NBR_CB** | Maximum number of GATT **Server** event callbacks the BLE stack will handle. Must cover all your Write/Read/Notify characteristics. | Set this to match the number of characteristics that have callbacks enabled (Write + Notify characteristics). |

> [!WARNING]
> **`BLE_CFG_SVC_MAX_NBR_CB`** is currently set to `7` in your screenshot. Once you add all 4 services and ~10 characteristics, this will need to be **increased to at least 35** or the BLE stack will silently fail to register your characteristics!

---

## Part 2: Individual Characteristic Parameters

---

## Identity & UUID

| Parameter | What It Means |
| :--- | :--- |
| **UUID type** | The format of the UUID. We use `128 bits UUID (0x02)` for custom app-specific services. Standard Bluetooth SIG services use 16-bit. |
| **UUID 128 input type** | `reduced` means you only type the unique part of the UUID (e.g., `FE 41`), and STM32 fills in the rest of the base UUID automatically. |
| **UUID** | The actual unique identifier for this characteristic (e.g., `FE 41`). Every characteristic must have a different UUID. |

---

## Naming

| Parameter | What It Means |
| :--- | :--- |
| **Characteristic long name** | The full descriptive name. Used internally in generated code as the variable/function name prefix (e.g., `Active_Mode` becomes `Custom_APP_Active_Mode_Callback`). |
| **Characteristic short name** | A compact version of the name. Typically the same as the long name. |

---

## Size & Length

| Parameter | What It Means | Practical Advice |
| :--- | :--- | :--- |
| **Value length** | The size in **bytes** of the data payload this characteristic holds. | `uint8_t` = 1, `uint16_t` = 2, `float` = 4, `struct of 3 floats` = 12. |
| **Length characteristic** | `Variable` means the payload can be shorter than the max. `Fixed` means it is always exactly `Value length` bytes. | Use `Variable` for flexibility. |

---

## Security & Encryption
These are all `No` for standard development unless you need pairing/bonding security.

| Parameter | What It Means |
| :--- | :--- |
| **Encryption Key Size** | `0x10` (16 bytes) is the default max key size for encrypted connections. Not relevant if all ATTR_PERMISSION_ENCRY settings are `No`. |
| **ATTR_PERMISSION_AUTHEN_READ** | Requires the phone to be **authenticated** (passkey paired) before it can READ this characteristic. |
| **ATTR_PERMISSION_AUTHOR_READ** | Requires **authorization** (app-level approval) before the phone can READ. |
| **ATTR_PERMISSION_ENCRY_READ** | Requires an **encrypted BLE link** before the phone can READ. |
| **ATTR_PERMISSION_AUTHEN_WRITE** | Requires authentication before the phone can WRITE. |
| **ATTR_PERMISSION_AUTHOR_WRITE** | Requires authorization before the phone can WRITE. |
| **ATTR_PERMISSION_ENCRY_WRITE** | Requires an encrypted link before the phone can WRITE. |

---

## Properties (The Most Important Section!)
These define **what operations** the phone is allowed to perform on this characteristic.

| Parameter | What It Means | When to Use |
| :--- | :--- | :--- |
| **CHAR_PROP_BROADCAST** | The characteristic value can be broadcast in BLE advertising packets. | Almost never. Leave `No`. |
| **CHAR_PROP_READ** | The phone can explicitly request and read the current value at any time. | Use for Config characteristics and Sensor Status. |
| **CHAR_PROP_WRITE_WITHOUT_RESP** | The phone can write a new value without waiting for an acknowledgement from the STM32. Faster, but no delivery guarantee. | Use for frequent, non-critical writes (e.g., System Command). |
| **CHAR_PROP_WRITE** | The phone writes a value and waits for the STM32 to acknowledge. Slower but reliable. | Use for critical config writes (e.g., Distance Config struct). |
| **CHAR_PROP_NOTIFY** | The STM32 can **push** new data to the phone automatically whenever the value changes, without the phone asking. The phone must subscribe first by enabling CCCD. | Use for all live data (Vital Data, Vibration Data, Sensor Status). |
| **CHAR_PROP_INDICATE** | Like Notify but the phone sends an acknowledgement back to the STM32 for every notification received. Reliable but slower. | Use only if you need guaranteed delivery (e.g., Fall Event Alert). |

---

## GATT Event Callbacks (Critical for your C Code!)

| Parameter | What It Means | Set to |
| :--- | :--- | :--- |
| **GATT_NOTIFY_ATTRIBUTE_WRITE** | When `Yes`, CubeMX generates a C callback function (`Custom_APP_..._WriteEvt_cb`) that fires every time the phone writes to this characteristic. **This is where you put your command handling code.** | `Yes` for all WRITE characteristics. |
| **GATT_NOTIFY_WRITE_REQ_AND_WAIT_FOR_APPL_RESP** | When `Yes`, the STM32 pauses and waits for your application code to call `aci_gatt_write_resp()` before confirming the write to the phone. Gives you manual control. | `Yes` if you need to validate data before accepting it. `No` for simple commands. |
| **GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP** | When `Yes`, the STM32 pauses and waits for your app code to call `aci_gatt_read_resp()` before returning the value to the phone. Lets you compute a fresh value on-demand. | `Yes` if value needs to be freshly calculated on read. `No` otherwise. |
| **GATT_NOTIFY_NOTIFICATION_COMPLETION** | When `Yes`, a callback fires after every BLE Notification is successfully sent. Useful for flow control if you are sending large arrays and want to wait for each one before sending the next. | `Yes` for Spectrum Array characteristic. `No` for everything else. |

---

## Quick Reference: Recommended Settings per Characteristic Type

| Characteristic Type | READ | WRITE | WRITE_WITHOUT_RESP | NOTIFY | GATT_NOTIFY_ATTRIBUTE_WRITE |
| :--- | :---: | :---: | :---: | :---: | :---: |
| Command (e.g., System Command) | No | No | **Yes** | No | **Yes** |
| Config (e.g., Distance Config) | **Yes** | **Yes** | No | No | **Yes** |
| Status (e.g., Sensor Status) | **Yes** | No | No | **Yes** | No |
| Live Data (e.g., Vital Data) | No | No | No | **Yes** | No |
| Alert (e.g., Fall Event Alert) | No | No | No | **Yes** (or Indicate) | No |

---

## Part 3: BLE Advertising Configuration
These settings (found in the **BLE Advertising** tab) define how your STM32 "shouts" its presence to the world before a phone connects to it.

### Advertising Configuration

| Parameter | What It Means | Recommendation |
| :--- | :--- | :--- |
| **Advertising Type** | `Undirected scannable and connectable` is the standard mode that allows any phone to see the device and connect to it. | Keep as is. |
| **ADV_INTERVAL_MIN/MAX** | How often the device sends an advertising packet (Units of 0.625ms). 80 to 100 means roughly every **50–60ms**. | This is very "fast" advertising. It makes the device appear instantly on the phone but uses more battery. For production, **160 to 320 (100–200ms)** is a better balance. |
| **ADV_LP_INTERVAL** | The interval used when the device enters a "Low Power" advertising state. | Keep as is. |

### Advertising Elements (The Data Packet)
Legacy BLE advertising packets are limited to **31 bytes**.

| Parameter | What It Means | Practical Advice |
| :--- | :--- | :--- |
| **AD_TYPE_COMPLETE_LOCAL_NAME** | The name that appears in the Bluetooth list on your phone. | Change `radarS_XX` to something unique like `Radar_Vital_01`. Keep it short to save packet space! |
| **AD_TYPE_MANUFACTURER_SPECIFIC_DATA** | Allows you to broadcast custom data bytes that the phone can read **without connecting**. | You are currently using **12 user-defined data items**. This is very useful if you want the phone to see "Presence Detected" just by looking at the scan list! |
| **ad_data[] length** | The total size of your advertising packet. | **Watch this carefully!** Yours is currently **27**. If it hits **31**, CubeMX will error out because the packet is full. |

---

## Part 4: Final Generation Checklist

Before you click **Generate Code**, make sure these specific values are set in the **BLE Services** tab to avoid common "Silent Failures":

1. **`BLE_CFG_SVC_MAX_NBR_CB`** → Set to **35** (Pool for all attributes).
2. **`BLE_CFG_MAX_NBR_CB`** → Set to **10** (Pool for callbacks).
3. **`AD_TYPE_COMPLETE_LOCAL_NAME`** → Change to your preferred device name.
4. **Data Length Check** → Double-check that `Distance_Config` (40), `Vibration_Config` (48), and `Spectrum_Array` (240) are correct.
