# Wireless tools

Open the eighth main-menu tile, `TOOLS`.

| Tool | Controls | Radio behavior |
| --- | --- | --- |
| BLE scanner / RSSI | Rotate through devices; short press opens signal view; short press there toggles manufacturer/UUID summary; BACK returns; hold on the list clears it | Reuses the continuous BLE receiver; messaging stays enabled |
| Slide remote | Clockwise: Page Up; counterclockwise: Page Down; short press: Space; hold: F5; BACK exits | Pauses messaging and advertises a connectable HID device |
| Media remote | Clockwise: volume down; counterclockwise: volume up; short press: play/pause; hold: next track; BACK exits | Same HID device and pairing as slide remote |
| Wi-Fi scanner | Rotate through results; short press toggles SSID/BSSID; hold rescans; BACK exits | Pauses messaging, passively scans 2.4 GHz, then releases Wi-Fi on exit |
| Memory status | Read current free heap, boot minimum and largest free block; BACK exits | No radio change |
| Wi-Fi connect | Scan/select AP, masked rotary password editor, status/IP, saved reconnect, disconnect, confirmed forget, optional boot auto-connect | Shares one Wi-Fi owner with scanner; see wifi_connect.md |

## Bluetooth remote setup

Enter either remote page, then use the computer or phone's Bluetooth settings to pair with **Halfstep Remote**. Wait for the on-device page to show the controls. Reports require an encrypted link and a subscribed input characteristic. Pairing uses Just Works, with bonding records saved in NimBLE's NVS namespace. Keyboard and Consumer Control reports use separate report IDs. A key press is followed by a zero report after at least 45 ms; failed releases are retried while connected. Exit sends release reports and disconnects the host.

The device only advertises for HID connections while a remote page is open. If the host has cached an incompatible profile or stale bond, remove its Halfstep Remote entry and pair again. Actual mapping of Page Up/Down, Space, F5 and media keys depends on the foreground application and operating system.

## Scanner limits

- The BLE cache holds 24 advertisers identified by address and address type. When full, it replaces the least recently seen entry. Random private addresses can therefore appear as separate devices.
- BLE scanning is passive. Devices that put their name only in scan responses may appear by address. Advertising names, manufacturer company ID and presence of service UUID/data fields are parsed; this is not a GATT service explorer or a packet sniffer.
- RSSI is signal strength, not distance. The signal view shows a sampled recent trace; stale samples are labelled LAST and stop adding live bars.
- Wi-Fi holds up to 24 AP records. Hidden SSIDs appear as `<HIDDEN SSID>`. The small built-in font renders non-ASCII bytes as `?`.
- Wi-Fi scanning does not associate, obtain an IP address or save credentials. WPA2/WPA3 labels describe observed AP security, not supported network-login modes.
- Wi-Fi runs in a separate worker. BACK requests shutdown; another exclusive tool waits for cleanup before starting. Messaging resumes after cleanup. Pending messages or active friend pairing can make exclusive mode entry fail; finish that operation and retry.

## Build and verification

The 4 MiB partition table, NVS v7 and storage ABI remain unchanged. The firmware uses ESP-IDF size optimization. Wi-Fi SoftAP and unused WPA3 connection support are disabled because only passive scanning is provided. BLE SMP/bond persistence is enabled for HID; the messaging transport still uses its existing plaintext broadcast protocol.

```powershell
.\build.ps1
python tools/verify.py
python tools/test_tool_reports.py
python tools/test_ble_scan.py
```

The report tests inspect the production HID report descriptor and the actual legacy advertising payload. The parser harness compiles production BLE parsing/cache code and checks names, manufacturer/service fields, duplicate identity, truncation, bounded cache and 10,000 malformed inputs. These tests do not establish RF reception or host HID compatibility.

After flashing, `python tools/smoke_wireless_tools.py --port COM10` drives real pages through the existing USB input protocol and saves OLED screenshots and telemetry under `build/tool-smoke`. It checks page navigation and mode exit; pairing and actual keyboard/media delivery require a host acceptance test.

## Hardware check (2026-10-20)

Flashed ESP32-C6FH4 on COM10 using ESP-IDF 5.5.4. BLE reception and RSSI view, Wi-Fi AP discovery and rescan, memory view, and navigation back to the normal BLE receiver were exercised. The slide remote now shows `PAIR: Halfstep Remote`; switching between the messaging extended advertising set and legacy HID advertising requires removing the inactive advertising set first. Background friend synchronization is preserved across a tool lease; pending user messages and active pairing are not interrupted. Host pairing and delivery of keyboard/media events still require an acceptance test.

The later Wi-Fi connection build is 1,211,792 bytes; each unchanged application slot has 33,392 bytes (about 32.6 KiB) left. This is application-slot headroom, not total unused flash. Connection uses IPv4/DHCP client; the earlier scan-only lifecycle notes above are superseded by wifi_connect.md for shared driver ownership and online coexistence.
