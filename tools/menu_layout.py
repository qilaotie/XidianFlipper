"""Use firmware menu destinations instead of historic row numbers in USB smoke tests."""
from pathlib import Path
import re

def menu_index(group,page):
    text=(Path(__file__).resolve().parents[1]/'firmware/ui/menu_layout.h').read_text(encoding='utf-8')
    name=group+'_MENU_ITEMS' if group in ('FUN','TOOLS') else group+'_ITEMS'
    body=text.split('#define '+name+'(X)',1)[1].split('#define',1)[0]
    pages=re.findall(r'X\("[^"\n]+",(M_\w+)\)',body)
    return pages.index(page)
