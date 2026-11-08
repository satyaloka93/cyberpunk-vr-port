"""Local hotfix for the exact deployed bridge; preserves every other compiled change.

The installed binary includes later TOW2 code absent from the clean bridge release
worktree. Rebuilding that older tree would silently discard it. Refuse all other
inputs, signed images, existing output files and changed instruction context.
"""
from pathlib import Path
import argparse,hashlib,json
import pefile
SOURCE_SHA='7d214ed4a46730fb5a574e40967437832b2a4c478a3340c322b5b6b865558c26'
RVA=0xc3ea
EXPECTED=bytes.fromhex('b8fa000000b91900000085ff0f45c189442450')
def patch(data):
    if hashlib.sha256(data).hexdigest()!=SOURCE_SHA:
        raise ValueError('Unknown bridge build; inspect it rather than patching by guess.')
    pe=pefile.PE(data=data)
    if pe.OPTIONAL_HEADER.DATA_DIRECTORY[4].Size:
        raise ValueError('Refusing to invalidate an Authenticode signature.')
    offset=pe.get_offset_from_rva(RVA)
    if data[offset:offset+len(EXPECTED)]!=EXPECTED:
        raise ValueError('Receive timeout instruction sequence differs.')
    out=bytearray(data)
    # MOV EAX,250 -> MOV EAX,8. The subsequent TOW2 selection still overrides it to25.
    out[offset+1]=8
    assert sum(a!=b for a,b in zip(data,out))==1
    return bytes(out),offset+1
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source',type=Path);parser.add_argument('output',type=Path)
    args=parser.parse_args()
    original=args.source.read_bytes();output,offset=patch(original)
    with args.output.open('xb') as f:f.write(output)
    print(json.dumps({'source':str(args.source),'output':str(args.output),'before':SOURCE_SHA,
        'after':hashlib.sha256(output).hexdigest(),'changed_file_offset':offset,
        'cyberpunk_poll_ms':8,'tow2_poll_ms':25,'driver_changed':False},indent=2))
