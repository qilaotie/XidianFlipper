"""Resume complete read-back of an already written app; never write device Flash.
Default: no reset, exact app/table/both NVS bytes before first boot. Explicit
USB reconnect recovery is recorded separately. Its optional pet check replays
the exact production model; every other NVS value and secondary byte must match.
"""
import argparse,json,ctypes,subprocess
from pathlib import Path
import esptool
from build import ROOT
from verify import verify
from flash_update import read_range,receive_buffer
from flash_migrate import sha

def verify_elapsed_pet(original,current,folder):
 from inspect_message_nvs import parse_nvs
 before,after=parse_nvs(original),parse_nvs(current)
 allowed={('pet_user','state_v2'),('pet_user','extra_v1')}
 for key in before.keys()|after.keys():
  if key not in allowed:assert before.get(key)==after.get(key),('Unexpected NVS value change',key)
 return verify_pet_values(before,after,folder)

def verify_pet_values(before,after,folder):
 """Verify only the pet values; callers separately protect other namespaces."""
 old_extra,new_extra=before[('pet_user','extra_v1')],after[('pet_user','extra_v1')]
 assert len(old_extra)==len(new_extra)==20 and old_extra[:12]==new_extra[:12] and old_extra[16:]==new_extra[16:],'Pet extra changed beyond elapsed time'
 elapsed=int.from_bytes(new_extra[12:16],'little')-int.from_bytes(old_extra[12:16],'little');assert 0<elapsed<=86400,'Unexpected elapsed pet time'
 old_state,new_state=before[('pet_user','state_v2')],after[('pet_user','state_v2')]
 assert len(old_state)==len(new_state)==28,'Unexpected pet ABI'
 # Replay the exact production model rather than exempting arbitrary pet fields.
 dll=folder/'pet-elapsed-replay.dll'
 subprocess.run(['gcc','-shared','-O2','-I'+str(ROOT/'firmware/app'),str(ROOT/'firmware/app/pet.c'),'-o',str(dll)],check=True,capture_output=True)
 model=ctypes.CDLL(str(dll));model.pet_valid.argtypes=[ctypes.c_void_p];model.pet_valid.restype=ctypes.c_bool;model.pet_advance.argtypes=[ctypes.c_void_p,ctypes.c_uint32]
 expected=ctypes.create_string_buffer(old_state,len(old_state));assert model.pet_valid(expected)
 model.pet_advance(expected,elapsed);assert expected.raw==new_state and model.pet_valid(expected),'Pet differs from exact elapsed-time replay'
 return elapsed
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--port',required=True);ap.add_argument('--record',type=Path,required=True);ap.add_argument('--connect-mode',choices=['no_reset','default_reset'],default='no_reset',help='default_reset is only for a device that unexpectedly booted after USB disconnect; never reports pre-boot verification');ap.add_argument('--verify-elapsed-pet',action='store_true',help='after an unexpected boot, permit only an exact replay of production pet_advance; every other NVS value and secondary bytes must match');args=ap.parse_args()
 folder=args.record.resolve().parent;assert folder.is_relative_to(ROOT/'build/migration-private')
 record=json.loads(args.record.read_text());release=ROOT/'build/release';verify(release);manifest=json.loads((release/'manifest.json').read_text())
 app=(release/manifest['application']['path']).read_bytes();assert sha(app)==record['application_sha256'] and record['status']=='backup_verified'
 esp=esptool.detect_chip(port=args.port,connect_mode=args.connect_mode);success=False
 try:
  assert esp.CHIP_NAME=='ESP32-C6' and ':'.join(f'{x:02x}' for x in esp.read_mac())==record['mac']
  receive_buffer(esp);esp=esp.run_stub();esp.flash_spi_attach(0);esp.change_baud(230400)
  assert read_range(esp,0x20000,len(app))==app,'Application mismatch'
  assert esp.read_flash(0x8000,0x1000)==(folder/'partition-table.bin').read_bytes(),'Partition mismatch'
  unchanged=True;elapsed=0
  for address in (0x9000,0x3b0000):
   current=read_range(esp,address,0x6000);original=(folder/f'nvs-{address:x}.bin').read_bytes()
   if args.verify_elapsed_pet:assert current==read_range(esp,address,0x6000),'NVS double read differs'
   if current!=original:
    assert args.verify_elapsed_pet and address==0x9000,'NVS changed'
    (folder/'nvs-after-usb-reconnect.bin').write_bytes(current);elapsed=verify_elapsed_pet(original,current,folder);unchanged=False
  after_boot=args.connect_mode!='no_reset' or args.verify_elapsed_pet
  record.update(status='passed_after_usb_reconnect' if after_boot else 'passed_before_first_boot',both_nvs_unchanged=unchanged,partition_table_unchanged=True,verification_resumed=True,connect_mode=args.connect_mode,all_other_nvs_values_unchanged=True,pet_elapsed_replayed_seconds=elapsed)
  args.record.write_text(json.dumps(record,indent=2)+'\n');success=True
  print('PASS resumed read-back: exact application/table; '+('both NVS byte-identical' if unchanged else 'all other NVS values unchanged; exact pet elapsed replay '+str(elapsed)+'s')+'; no device write')
 finally:
  if success:esp.hard_reset()
  esp._port.close()
if __name__=='__main__':main()
