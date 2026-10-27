"""Cyberpunk 2.50 SSR asymmetric reconstruction correction.

Exact shipped staticshader_final.cache keys; no cross-eye history or CB growth.
The native stereo branch already uses inverse projection; only its legacy
symmetric tangent branch needs correction. Preserve the native alternate
material projection selection (primary rows0..3, alternate rows16..19).
"""
import argparse
import json
import re
import tempfile
from pathlib import Path
import asym_ssr_shaders as base

KEYS = """07c867b70fba5da4 07d070bd8084c6f2 116eb49a16506e84
3a91e675a6927d5d 5e01aaacc7c617d2 70f6bc4c7c450b0a
77adea4e84469478 842ab722bfe403cd 8661cc86756e2ecc
9c9679515ec51bd3 acaedc11005a92d0 b2abe4d809bf2fb5
b76fb6b3b79f4925 b86f8871454997e6 b89376a214706621
bdf51af7acfab45e c0813d22e924ca92 c827b32cf54f25fb
d26038a71d52b0b7 d8ecb848b998a50b f3b5e66d1f6fe2c5
f9adc8ca12d869fa""".split()


def projection_selector(ir, b6):
    """Find the engine's material-dependent choice of the two projection matrices."""
    extracts = {}
    for row in (0, 16):
        for load in re.findall(base.LOAD.format(h=re.escape(b6), row=row), ir):
            for value in re.findall(r'(?m)^  (%\d+) = extractvalue %dx\.types\.CBufRet\.f32 ' + re.escape(load) + ', 2$', ir):
                extracts[value] = row
    choices = set()
    for cond, a, b in re.findall(r'(?m)^  %\d+ = select i1 (%\d+), float (%\d+), float (%\d+)$', ir):
        if extracts.get(a) == 16 and extracts.get(b) == 0:
            choices.add(cond)
    if len(choices) > 1:
        raise ValueError('ambiguous alternate projection selector')
    if 16 in extracts.values() and not choices:
        raise ValueError('alternate projection exists without proven selector')
    return next(iter(choices), None)


def patch(ir):
    b6 = base.cbv_handle(ir, 6)
    if not re.search(r'i32 6, i32 1, i32 656, null', ir):
        raise ValueError('not the audited 656-byte SSR block')
    selector = projection_selector(ir, b6)
    ir, count = base.shift_reconstruction(ir, tangent_row=33)
    if count != 2:
        raise ValueError('expected exactly one x/y reconstruction pair')
    ir, fades = base.shared_view_fade(ir)
    # Both reconstruction and edge fading must use the same effective projection
    # as the native raymarch. Insert alternate selects at each new load site;
    # DXIL validation verifies that the native selector dominates all these uses.
    if selector:
        pattern = re.compile(r'(?m)^  (%asym\.[\w.]+) = call %dx\.types\.CBufRet\.f32 @dx\.op\.cbufferLoadLegacy\.f32\(i32 59, %dx\.types\.Handle ' + re.escape(b6) + r', i32 ([013])\)$')
        def select_row(m):
            name, row = m.groups()
            # Aggregate selects are not legal DXIL; select the extracted scalar.
            return m.group(0) + '\n  ' + name + '.alt = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle ' + b6 + ', i32 ' + str(int(row)+16) + ')'
        loads = [m.group(1) for m in pattern.finditer(ir)]
        ir = pattern.sub(select_row, ir)
        for load in loads:
            pattern_ex = re.compile(r'(?m)^  (%asym\.[\w.]+) = extractvalue %dx\.types\.CBufRet\.f32 ' + re.escape(load) + r', 2$')
            def select_component(m):
                name = m.group(1)
                return ('  '+name+'.primary = extractvalue %dx.types.CBufRet.f32 '+load+', 2\n'
                        '  '+name+'.alternate = extractvalue %dx.types.CBufRet.f32 '+load+'.alt, 2\n'
                        '  '+name+' = select i1 '+selector+', float '+name+'.alternate, float '+name+'.primary')
            ir = pattern_ex.sub(select_component, ir)
    return ir, {'reconstruction_sites': count, 'shared_fades': fades, 'alternate_projection': bool(selector)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--shaders', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--assembler', type=Path, required=True)
    args = parser.parse_args()
    base.ham.DXIL_TOOL = args.assembler
    args.out.mkdir(parents=True, exist_ok=True)
    report = []
    for key in KEYS:
        original = (args.shaders / (key+'.dxil')).read_bytes()
        if base.ham.fnv1a(original) != int(key,16):
            raise ValueError('source key mismatch: '+key)
        with tempfile.TemporaryDirectory() as temp:
            work = Path(temp)
            ir, evidence = patch(base.ham.disassemble(original, work))
            container = base.ham.assemble(ir, work)
            if base.dc.dxil_hash(container) != container[4:20]:
                raise ValueError('validator hash mismatch: '+key)
        (args.out / (key+'.dxil')).write_bytes(container)
        report.append(dict(key=key, **evidence))
        print(key, evidence, flush=True)
    (args.out / 'migration-250.json').write_text(json.dumps(report, indent=2))

if __name__ == '__main__':
    main()
