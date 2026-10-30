from flash_update import read_range,receive_buffer
class Port:
 timeout=None
 def set_buffer_size(self,rx_size,tx_size):self.buffers=(rx_size,tx_size)
class Device:_port=Port()
d=Device();receive_buffer(d);assert d._port.timeout==5 and d._port.buffers==(1048576,65536)
class Reader:
 def __init__(self,short=False):self.calls=[];self.short=short
 def read_flash(self,address,size):
  assert 0<size<=0x800
  self.calls.append((address,size))
  return bytes((address+i)%251 for i in range(size-(1 if self.short else 0)))
r=Reader();data=read_range(r,0x20000,0x8007)
assert data==bytes((0x20000+i)%251 for i in range(0x8007))
assert r.calls==[(0x20000+i*0x800,0x800) for i in range(16)]+[(0x28000,7)]
try:read_range(Reader(True),0x20000,100)
except RuntimeError:pass
else:raise AssertionError('short read was accepted')
print('PASS Flash reads: bounded bursts, exact coverage including tail, short transfers refused')
