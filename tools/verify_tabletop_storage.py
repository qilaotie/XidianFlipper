"""Read-only post-game comparison with the protected app-update NVS backups.
Pet time may advance normally. Existing user messaging/contact storage must match.
"""
import argparse,json,hashlib
from pathlib import Path
import esptool
from flash_update import read_range,receive_buffer
from flash_verify_pending import verify_pet_values
from inspect_message_nvs import parse_nvs,mac
ROOT=Path(__file__).resolve().parents[1]

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--port',required=True);ap.add_argument('--record',type=Path,required=True);ap.add_argument('--tag',required=True);args=ap.parse_args()
    record=args.record.resolve();assert record.is_relative_to(ROOT/'build/migration-private')
    meta=json.loads(record.read_text());assert meta['status'] in ('passed_before_first_boot','passed_after_usb_reconnect')
    out=ROOT/'build/tabletop-private'/args.tag/args.port;out.mkdir(parents=True,exist_ok=False)
    esp=esptool.detect_chip(port=args.port)
    try:
        assert esp.CHIP_NAME=='ESP32-C6' and mac(esp.read_mac())==meta['mac'];receive_buffer(esp);esp=esp.run_stub();esp.flash_spi_attach(0);esp.change_baud(230400)
        result={'port':args.port,'mac':meta['mac'],'application_sha256':meta['application_sha256'],'partitions':[]}
        assert esp.read_flash(0x8000,0x1000)==(record.parent/'partition-table.bin').read_bytes()
        for address in (0x9000,0x3b0000):
            raw=read_range(esp,address,0x6000);assert raw==read_range(esp,address,0x6000)
            (out/f'nvs-{address:x}.bin').write_bytes(raw)
            original=(record.parent/f'nvs-{address:x}.bin').read_bytes();before=parse_nvs(original);after=parse_nvs(raw)
            namespaces={'messages_v3','friends_v3','msg_drafts','msg_outbox'}
            protected={key for key in before.keys()|after.keys() if key[0] in namespaces}
            for key in protected:assert before.get(key)==after.get(key),(args.port,'User data changed',key)
            pet_elapsed=0
            if address==0x9000:
                for key in before.keys()|after.keys():
                    if key[0]=='pet_user' and key[1] not in ('state_v2','extra_v1'):assert before.get(key)==after.get(key),('Pet preference changed',key)
                if before.get(('pet_user','state_v2'))!=after.get(('pet_user','state_v2')) or before.get(('pet_user','extra_v1'))!=after.get(('pet_user','extra_v1')):pet_elapsed=verify_pet_values(before,after,out)
            # The secondary partition is not touched by tabletop games at all.
            if address==0x3b0000:assert original==raw,'Secondary NVS changed'
            result['partitions'].append({'address':hex(address),'protected_keys':len(protected),'unchanged_bytes':original==raw,'pet_elapsed_replayed_seconds':pet_elapsed,'sha256':hashlib.sha256(raw).hexdigest()})
        (out/'result.json').write_text(json.dumps(result,indent=2),encoding='utf8')
        print('PASS',args.port,'existing messages/friends/work draft/saved slots/outbox, table and secondary NVS preserved; both reads matched')
    finally:esp.hard_reset();esp._port.close()
if __name__=='__main__':main()
