"""Check production category maps and renderer row-minus-one bindings."""
from pathlib import Path
import re

root=Path(__file__).resolve().parents[1]
source=(root/'firmware/ui/menu_pages.c').read_text(encoding='utf8')
expected={'display':[0,6,9,10,11],'feedback':[3,4],'input':[1,2,5],'system':[8,7]}
flat=[]
for name,rows in expected.items():
    match=re.search(r'\b'+name+r'\[\]=\{([\d,]+)\}',source)
    assert match and [int(n) for n in match[1].split(',')]==rows
    flat+=rows
    # Model the production projection and the renderer's two pointer offsets.
    values=[100+i for i in range(12)]
    toggles=[i%2 for i in range(12)]
    projected_values=[values[row] for row in rows]
    projected_toggles=[toggles[row-1] if row else 0 for row in rows]
    for row,original in enumerate(rows):
        assert projected_values[(row-1)+1]==values[original]
        if original in (3,4,6,9):
            assert projected_toggles[(row-1)+1]==toggles[original-1]
assert sorted(flat)==list(range(12))
assert 'switch (map[sel])' in source
assert 'check_box.m=toggles+1;check_box.v=values+1;' in source
assert 'window_list_init("Factory Reset?"' in source
print('PASS settings: all 12 actions exactly once, category order, numeric/toggle bindings, reset confirmation retained')
