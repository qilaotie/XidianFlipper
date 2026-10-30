"""Production rules and transport; simulated devices are not RF acceptance."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='halfstep-party-') as temp:
    out=Path(temp)
    for name,files in [('rules',['tools/test_party_rules.c','firmware/app/party_rules.c']),
                       ('link',['tools/test_party_link.c','firmware/app/party_link.c','firmware/app/party_rules.c'])]:
        command=['gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',
                 '-I'+str(root/'firmware/app'),'-I'+str(root/'firmware/protocol')]
        subprocess.run(command+[str(root/f) for f in files]+['-o',str(out/(name+'.exe'))],check=True)
        subprocess.run([str(out/(name+'.exe'))],check=True,timeout=60)
