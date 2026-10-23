"""Enter credentials through the real masked editor. Never save input or entry frames."""
import argparse
import getpass
import time
from pathlib import Path
from smoke_party import Device
from smoke_wireless_tools import png
from menu_layout import menu_index


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', default='COM9')
    args = parser.parse_args()
    password = getpass.getpass('Hotspot password (hidden): ')
    groups = ['ABCDEFGHIJKLMNOPQRSTUVWXYZ', 'abcdefghijklmnopqrstuvwxyz',
              '0123456789', '!"#$%&\'()*+,-./:;<=>?@[\\]^_`{|}~', ' ']
    if not 8 <= len(password) <= 63 or any(not any(c in g for g in groups) for c in password):
        raise SystemExit('Unsupported password format')
    d = Device(args.port)
    heartbeat = 0
    def pump(seconds):
        nonlocal heartbeat
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            if time.monotonic() - heartbeat > .5:
                d.send(7); heartbeat = time.monotonic()
            d.read()
            # Do not retain logs during credential entry.
            d.log.clear()
            time.sleep(.005)
    def click(long=False):
        d.send(2); pump(1.7 if long else .15); d.send(3); pump(.65)
    def back():
        d.send(4); pump(.1); d.send(5); pump(.65)
    def move(delta, count):
        delta %= count
        if delta > count // 2:
            delta -= count
        for _ in range(abs(delta)):
            d.send(1, -1 if delta > 0 else 1); pump(.25)
        pump(.35)
    try:
        d.send(9, 0); pump(4)
        if d.telemetry.get('page') == 1:
            def menu_rotate(amount):
                for _ in range(amount):
                    d.send(1, 1); pump(.25)
                pump(.35)
            click(); menu_rotate(5); click(); menu_rotate(menu_index('TOOLS','M_WIFI_CONNECT')); click()
            click(); pump(6)
            assert d.telemetry.get('page') == 55, 'Network list unavailable'
            target = Path('build/wifi-input-target.png')
            target.write_bytes(png(d.frame))
            print('Verify first network now:', target.resolve(), flush=True)
            confirmed = input('Inspect the selected SSID in the image; type YES only if it is your intended network: ')
            if confirmed != 'YES':
                raise SystemExit('Target not verified; no credentials submitted')
            pump(.3)
            click()
        assert d.telemetry.get('page') == 56, ('Open an empty Wi-Fi password editor first', d.telemetry.get('page'))
        group, selecting, positions = 1, True, [0] * 5
        for char in password:
            target = next(i for i, g in enumerate(groups) if char in g)
            if target != group:
                if not selecting:
                    back(); selecting = True
                move(target - group, 7); group = target
            if target < 4:
                if selecting:
                    click(); selecting = False
                position = groups[target].index(char)
                move(position - positions[target], len(groups[target]))
                positions[target] = position
            click()
            assert d.telemetry.get('page') == 56, 'Editor unexpectedly changed page'
        password = ''
        click(True); click(); pump(35)
        assert d.telemetry.get('page') == 57, 'Connection status page not shown'
        output = Path('build/wifi-connect-result.png')
        output.write_bytes(png(d.frame))
        print('Status captured:', output.resolve(), flush=True)
    finally:
        d.log.clear(); d.serial.close()


if __name__ == '__main__':
    main()
