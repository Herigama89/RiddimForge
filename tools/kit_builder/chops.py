from instr import *
import json, numpy as np
from load import load, mono
from load2 import load2
NN=["C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"]
SESSION={"skanky":1,"chop":1,"bubble":1,"percorg":1,"piano1":1,"clean":2,"organ4":2,"piano2":2}
_bn={1:json.load(open("notes_bass1.json")),2:json.load(open("notes_bass2.json"))}
_master={1:mono(load("Master")),2:mono(load2("Master"))}
def bass_root(sess, s):
    best=None
    for n in _bn[sess]:
        if n["start"]<=s+int(0.06*SR) and s-n["start"]<int(2.5*SR): best=n
    return None if best is None else best["midi"]%12
def label(name):
    x=mono(get(name)); sess=SESSION[name]; M=_master[sess]
    on=onsets_generic(x, rel_db=-32, gap=0.07, jump=5)
    out=[]
    for k,s in enumerate(on):
        e=on[k+1] if k+1<len(on) else s+SR
        seg=x[s+240:min(e,s+int(0.3*SR))]
        if len(seg)<2400: continue
        c=chroma(seg)
        cm=chroma(M[max(0,s-int(0.2*SR)):s+int(0.4*SR)])
        comb=0.6*c+0.4*cm
        r=bass_root(sess,s)
        if r is None:
            sc,r,q=chord_from_chroma(comb)
        maj=comb[(r+4)%12]; mn=comb[(r+3)%12]
        q="min" if mn>maj else "maj"
        # agreement of chop with this chord (root/third/fifth energy share)
        fit=float(c[r]+c[(r+(3 if q=="min" else 4))%12]+c[(r+7)%12])
        out.append(dict(start=int(s), end=int(min(e, s+int(1.6*SR))), root=int(r), q=q, fit=fit,
                        level=float(20*np.log10(np.abs(x[s:s+2400]).max()+1e-9))))
    return out
if __name__=="__main__":
    from collections import Counter
    for name in SESSION:
        ch=label(name); json.dump(ch,open(f"chops_{name}.json","w"))
        good=[c for c in ch if c["fit"]>0.55]
        cnt=Counter(NN[c["root"]]+("m" if c["q"]=="min" else "") for c in good)
        dur=np.median([(c["end"]-c["start"])/SR for c in ch])
        print(f"{name:8s} chops {len(ch):3d}, fit>0.55: {len(good):3d}  dur med {dur:.2f}s  chords {cnt.most_common(6)}")
