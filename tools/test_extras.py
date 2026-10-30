"""Host production cores; fault-injected simulation is not hardware acceptance."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='halfstep-extras-') as temp:
 p=Path(temp)
 subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-Wno-sign-compare','-I'+str(root/'firmware/protocol'),'-c',str(root/'firmware/protocol/mesh_packet.c'),'-o',str(p/'packet.o')],check=True)
 for test,core in [('test_pet_gifts','pet_gifts'),('test_network_lab','network_lab')]:
  subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I'+str(root/'firmware/app'),'-I'+str(root/'firmware/protocol'),str(root/'tools'/f'{test}.c'),str(root/'firmware/app'/f'{core}.c'),str(p/'packet.o'),'-o',str(p/f'{test}.exe')],check=True)
  subprocess.run([str(p/f'{test}.exe')],check=True,timeout=30)
