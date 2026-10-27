"""2.50 exact-container fog migration. Only ordinary-camera ray sites change.
Native stereo branches use b6 matrices and are left intact. See
build/diagnostics/game250/native-stereo-fog for shader and camera evidence.
"""
import argparse,json,tempfile
from pathlib import Path
import asym_fog_shaders as fog
SHADERS={
 0x215948257f9d0a3e:4, 0x3bec9b299e5c0610:2,
 0x3eea2ad1d75a8d7d:4, 0x601954c7c7c00b49:4,
 0x734abb11195dbe52:4, 0xb098a198a7565e1d:4,
 0xf60d19af775ca034:4,
}
def main():
 p=argparse.ArgumentParser();p.add_argument('--shaders',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--tool',type=Path,required=True);a=p.parse_args()
 fog.ham.DXIL_TOOL=a.tool;a.out.mkdir(parents=True,exist_ok=True);report=[]
 for key,expected in SHADERS.items():
  source=a.shaders/f'{key:016x}.dxil';raw=source.read_bytes()
  if fog.ham.fnv1a(raw)!=key:raise ValueError('Original shader identity mismatch')
  with tempfile.TemporaryDirectory() as tmp:
   work=Path(tmp);original=fog.ham.disassemble(raw,work);patched,count=fog.shift_rays(original)
   if count!=expected:raise ValueError(f'{key:x}: expected {expected} ray components, got {count}')
   output=fog.ham.assemble(patched,work)
  if fog.dc.dxil_hash(output)!=output[4:20]:raise ValueError('Invalid output container hash')
  target=a.out/source.name;target.write_bytes(output)
  report.append({'source':source.name,'ray_components':count,'bytes':len(output)})
 (a.out/'migration-250.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
if __name__=='__main__':main()
