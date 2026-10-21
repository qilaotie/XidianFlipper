"""Host checks for the production inbox sender-name resolver."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'firmware/ui/local_pages.c').read_text(encoding='utf-8')
start = source.index('static void message_sender_name(')
end = source.index('\nstatic void inbox_page', start)
shim = r'''
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define FRIEND_NAME_MAX 13
typedef struct { char name[14]; } friend_record_t;
static uint8_t known[6] = {1,2,3,4,0xE2,0x70};
static friend_record_t record;
static bool friend_store_get_active(const uint8_t mac[6],friend_record_t *out) {
    if (memcmp(mac,known,6)) return false;
    *out=record; return true;
}
'''
tests = r'''
int main(void) {
    struct { char text[14]; char guard; } out = {.guard=42};
    strcpy(record.name,"Alice");
    message_sender_name(known,out.text); assert(!strcmp(out.text,"Alice"));
    uint8_t unknown[6]={9,2,3,4,0xE2,0x70};
    message_sender_name(unknown,out.text); assert(!strcmp(out.text,"E270"));
    record.name[0]=0;
    message_sender_name(known,out.text); assert(!strcmp(out.text,"E270"));
    memset(record.name,'X',sizeof(record.name));
    message_sender_name(known,out.text); assert(strlen(out.text)==13);
    assert(out.guard==42);
    puts("PASS: nickname, full-MAC identity, unknown/empty fallback, bounded name");
}
'''
compiler = shutil.which('gcc')
if not compiler:
    raise SystemExit('Native gcc required')
with tempfile.TemporaryDirectory(prefix='sender-test-') as folder:
    c = Path(folder) / 'sender.c'
    exe = Path(folder) / 'sender.exe'
    c.write_text(shim + source[start:end] + tests, encoding='utf-8')
    subprocess.run([compiler,'-std=c11','-Wall','-Wextra','-Werror','-fno-stack-protector',str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
