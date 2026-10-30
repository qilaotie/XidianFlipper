"""Nearby production state machines and drawing bounds; not RF acceptance."""
from pathlib import Path
import subprocess,tempfile

root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='halfstep-nearby-graphics-') as temp:
    for name in ('rps','party','nearby'):
        exe=Path(temp)/(name+'.exe')
        subprocess.run(['gcc','-std=c11','-O1','-Wall','-Wextra','-Werror',
                        '-Wno-sign-compare','-Wno-misleading-indentation',
                        '-I'+str(root/'firmware/protocol'),str(root/f'tools/test_{name}.c'),
                        str(root/'firmware/protocol/mesh_packet.c'),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True,timeout=60)
