"""Read-only verification of the installed game against the scope call-site guard."""
from pathlib import Path
import re,struct,sys
import pefile
root=Path(__file__).resolve().parents[2]
s=(root/'src/Stereo/ScopeGraphDag.cpp').read_text()
site=int(re.search(r'SERIAL_TEST_CALL_RVA = (0x[0-9A-Fa-f]+)',s)[1],16)
target=int(re.search(r'FEATURE_TEST_RVA = (0x[0-9A-Fa-f]+)',s)[1],16)
expected=bytes(int(v,16) for v in re.search(r'expected\[8\] = \{([^}]+)',s)[1].split(','))
pe=pefile.PE(sys.argv[1],fast_load=True)
assert pe.get_data(site-8,8)==expected,'Scope feature-test preamble differs: do not deploy'
call=pe.get_data(site,5)
assert call[0]==0xe8 and site+5+struct.unpack('<i',call[1:])[0]==target,'Scope call target differs: do not deploy'
print(f'PASS installed game: scope feature64 call RVA {site:#x} -> {target:#x}, exact instruction guard matches.')
