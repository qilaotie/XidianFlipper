"""Run the production BLE advertising parser/cache with hardware calls stubbed."""
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "firmware/app/wireless_tools.c").read_text(encoding="utf-8")


def function(signature):
    start = SOURCE.index(signature)
    brace = SOURCE.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        if SOURCE[end] == "{": depth += 1
        if SOURCE[end] == "}": depth -= 1
        end += 1
    return SOURCE[start:end]


def main():
    compiler = shutil.which("gcc")
    if not compiler:
        raise SystemExit("Native gcc is required for parser checks")
    typedef = re.search(r"typedef struct\s*\{.*?\}\s*scan_peer_t;", SOURCE, re.S)[0]
    shim = r'''
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#define TOOL_PEERS 24
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
static int s_lock;
static bool s_observe;
static unsigned s_peer_count;
static uint32_t clock_ms = 100;
static uint32_t ms(void) { return clock_ms++; }
'''
    tests = r'''
int main(void) {
    uint8_t mac[6] = {1,2,3,4,5,6};
    const uint8_t ad[] = {5,9,'T','E','S','T', 3,0xff,0x34,0x12, 3,3,0x12,0x18};
    wireless_tools_ble_report(mac,0,ad,sizeof(ad),-70);
    assert(s_peer_count == 0);
    s_observe=true;
    wireless_tools_ble_report(mac,0,ad,sizeof(ad),-70);
    assert(s_peer_count == 1 && !strcmp(s_peers[0].name,"TEST"));
    assert(s_peers[0].manufacturer && s_peers[0].company == 0x1234 && s_peers[0].service);
    const uint8_t empty[] = {0};
    wireless_tools_ble_report(mac,0,empty,sizeof(empty),-40);
    assert(s_peer_count == 1 && s_peers[0].rssi == -40 && !strcmp(s_peers[0].name,"TEST"));
    wireless_tools_ble_report(mac,1,empty,sizeof(empty),-50);
    assert(s_peer_count == 2); /* Address type belongs to identity. */
    const uint8_t broken[] = {250,9,'X'};
    wireless_tools_ble_report(mac,1,broken,sizeof(broken),-55);
    assert(s_peer_count == 2);
    for(unsigned i=0;i<60;i++) {
        mac[0]=(uint8_t)i;
        wireless_tools_ble_report(mac,0,ad,sizeof(ad),-60);
        assert(s_peer_count <= TOOL_PEERS);
    }
    assert(s_peer_count == TOOL_PEERS);
    uint32_t rng=1;
    for(unsigned i=0;i<10000;i++) {
        uint8_t bytes[80];
        for(unsigned j=0;j<80;j++) { rng=rng*1664525u+1013904223u;bytes[j]=rng>>24; }
        wireless_tools_ble_report(mac,0,bytes,i%81,-60);
        assert(s_peer_count == TOOL_PEERS);
        for(unsigned j=0;j<s_peer_count;j++) assert(s_peers[j].name[24] == 0);
    }
    puts("PASS: BLE name, manufacturer, services, dedup, address types, truncated AD, bounded cache, 10000 malformed reports");
}
'''
    code = shim + typedef + "\nstatic scan_peer_t s_peers[TOOL_PEERS];\n"
    code += function("static void printable(") + "\n" + function("void wireless_tools_ble_report(") + "\n" + tests
    output = ROOT / "build/test_ble_scan.exe"
    # This MinGW installation emits an invalid FS-relative stack guard on x64
    # Windows. Firmware stack protection is independent of this host harness.
    subprocess.run([compiler, "-g", "-std=c11", "-Wall", "-Wextra", "-Werror", "-fno-stack-protector", "-x", "c", "-", "-o", str(output)], input=code, text=True, check=True)
    result = subprocess.run([str(output)])
    if result.returncode:
        debugger = shutil.which("gdb")
        if debugger:
            subprocess.run([debugger, "--batch", "-x", str(ROOT / "tools/debug_ble_scan.gdb"), str(output)], timeout=15)
        result.check_returncode()


if __name__ == "__main__":
    main()
