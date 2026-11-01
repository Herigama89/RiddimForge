from instr import *
import numpy as np, os
HOP=256
def segment_notes(name, fmin, fmax, lo, hi):
    x=mono(get(name))
    f=f"yin_{name}.npy"
    if os.path.exists(f): f0,rms,conf=np.load(f)
    else:
        f0,rms,conf=yin_track(x,fmin=fmin,fmax=fmax); np.save(f,np.stack([f0,rms,conf]))
    on=set(i//HOP for i in onsets_generic(x, rel_db=-36, gap=0.06, jump=5))
    midi=np.where(f0>0, 69+12*np.log2(np.maximum(f0,1)/440), 0)
    good=(f0>0)&(conf>0.75)&(rms>rms.max()*0.02)&(midi>=lo)&(midi<=hi)
    notes=[]; i=0; n=len(f0)
    while i<n:
        if not good[i]: i+=1; continue
        j=i; ref=midi[i]
        while j+1<n and good[j+1] and abs(midi[j+1]-ref)<0.6 and (j+1) not in on: j+=1; ref=0.8*ref+0.2*midi[j]
        if (j-i+1)*HOP/SR>=0.08:
            seg=midi[i+2:j+1] if j-i>4 else midi[i:j+1]
            m=float(np.median(seg))
            # start: look back for the attack (onset frame up to 60 ms earlier)
            st=i
            for b in range(i, max(0,i-12), -1):
                if b in on: st=b; break
            notes.append(dict(start=st*HOP, end=(j+1)*HOP, midi=int(round(m)), cents=float((m-round(m))*100),
                              level=float(20*np.log10(rms[i:j+1].max()+1e-9)), stab=float(np.std(seg))))
        i=j+1
    return notes
if __name__=="__main__":
    from collections import Counter
    for name,fmin,fmax,lo,hi in [("bass1",30,400,26,62),("bass2",30,400,26,62),("pluckjc",70,1200,45,90),("pluckk",70,1200,45,90),("leadgtr",70,1400,48,96)]:
        ns=segment_notes(name,fmin,fmax,lo,hi)
        import json; json.dump(ns,open(f"notes_{name}.json","w"))
        c=Counter(n["midi"] for n in ns)
        dur=np.array([(n["end"]-n["start"])/SR for n in ns])
        print(f"{name:8s} notes={len(ns):3d} distinct pitches={len(c):2d} range {min(c)}..{max(c)} dur med {np.median(dur):.2f}s  cents med {np.median([n['cents'] for n in ns]):+.0f}")
        print("          per pitch:", dict(sorted(c.items())))
