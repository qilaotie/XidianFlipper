# Nearby rock-paper-scissors

Install the same nickname-enabled RPS firmware on both ESP32-C6 devices, then open `FUN > RPS nearby`. Friend pairing is not required. The top line shows `VS <device nickname>` throughout matching, play and results. An empty nickname falls back to the peer MAC suffix. Names use the existing seven-character device setting and the ASCII screen font.

- Discovery takes at least 3.5 seconds. It selects the strongest recent smoothed RSSI among up to eight players. RSSI estimates proximity, not physical distance.
- Both players must select each other. With several players, bring the intended pair close together. The matching screen shows the selected MAC suffix and RSSI.
- Rotate to choose ROCK, SCISSORS, or PAPER; short press locks the choice. The screen hides the remote choice until the local choice is locked.
- The result shows WIN/LOSE/DRAW followed by `your move/peer move`. After confirmation, short press starts a new match.
- BACK exits. After 25 seconds without a matching game packet, press to search again.

Transport: custom BLE message type 8; 19-byte payload containing protocol version, operation, sender round token (little endian uint32), recipient round token (uint32), choice, and an eight-byte zero-terminated device name. The receiver also accepts legacy 11-byte packets, but the old firmware does not accept the new packets, so update both boards. HELLO is broadcast; other packets target the selected MAC and require both round tokens to match. Frames repeat every 700 ms. Linking alternates discovery and handshake packets so staggered entry remains discoverable. The game does not use NVS, inbox, or the public messaging channel.

Choices are plaintext. This casual game hides choices in its UI but does not implement cryptographic commit/reveal or authentication.

## Validation

Build with `build.ps1`, then run `python tools/verify.py`. The host test runs the actual game source with mocked clock, radio and display:

```powershell
gcc -std=c11 -Wall -Wextra -Werror -Wno-error=sign-compare -Ifirmware/protocol tools/test_rps.c firmware/protocol/mesh_packet.c -o build/test_rps.exe
.\build\test_rps.exe
```

It checks all nine outcomes, RSSI selection, stale round rejection, invalid choices, locking, result confirmation, rematch, departure, timeout and text width. The pre-existing mesh_packet.c signedness warnings remain warnings in this host command.

Host tests and flashing do not establish real two-device RF operation. Hardware acceptance requires two updated boards: simultaneous and staggered entry, both move orders, all outcomes, dropped packets, peer exit and disconnection.
