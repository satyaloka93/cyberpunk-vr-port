"""Delta cumulative sampled GPU timings; never sum nested stages or infer FPS."""
import argparse,json,math,re
from pathlib import Path
HEADER=re.compile(r'GPU stages v1 pid=(\d+) qpc=(\d+) frequency=(\d+) (.*)')
METRIC=re.compile(r'(\w+)=([0-9.]+),(\d+)')

def analyze(text):
    previous={}; rows=[]; session=0
    for line in text.splitlines():
        if 'Standalone initializing;' in line:
            previous.clear(); session+=1
        match=HEADER.search(line)
        if not match: continue
        pid,qpc,freq=map(int,match.group(1,2,3))
        if freq<=0: raise ValueError('Invalid QPC frequency')
        values={name:(float(ms),int(n)) for name,ms,n in METRIC.findall(match[4])}
        if len(values)!=9 or any(not math.isfinite(ms) or ms<0 for ms,n in values.values()):
            raise ValueError('Incomplete/nonfinite stage record')
        prior=previous.get(pid)
        previous[pid]=(qpc,freq,values)
        if prior is None: continue
        old_qpc,old_freq,old=prior
        if qpc<=old_qpc or freq!=old_freq or old.keys()!=values.keys(): continue
        if any(ms<old[name][0] or n<old[name][1] for name,(ms,n) in values.items()): continue
        stages={}
        for name,(ms,n) in values.items():
            dms=ms-old[name][0]; dn=n-old[name][1]
            if dn==0 and abs(dms)>0.000002: raise ValueError('Total changed without samples')
            stages[name]={'samples':dn,'sampled_ms':round(dms,6),'mean_ms':round(dms/dn,6) if dn else None}
        rows.append({'session':session,'pid':pid,'begin_qpc':old_qpc,'end_qpc':qpc,'seconds':(qpc-old_qpc)/freq,'stages':stages})
    return {'semantics':'Per-evaluation sampled GPU durations, NOT per-eye/frame totals or FPS. Pipeline stages include children. Null means no fresh samples. First records and counter resets establish a baseline. Delayed GPU completion may cross interval boundaries.', 'intervals':rows}

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('log',type=Path);args=p.parse_args()
    print(json.dumps(analyze(args.log.read_text(encoding='utf-8-sig',errors='replace')),indent=2))
