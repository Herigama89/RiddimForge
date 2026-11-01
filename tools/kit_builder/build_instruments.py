"""Turn the instrument stems into sampled instruments for RiddimForge (Resources/Instruments)."""
import json, os, subprocess, shutil, numpy as np, scipy.io.wavfile as wavio
from instr import get, mono, SR
from collections import defaultdict
OUT="/home/claude/RiddimForge/Resources/Instruments"

def fade(x, fin=24, fout=1200):
    x=x.copy(); n=len(x); fin=min(fin,n//4); fout=min(fout,n//2)
    w=np.linspace(0,1,fin); x[:fin]*= w[:,None] if x.ndim==2 else w
    w=np.cos(np.linspace(0,np.pi/2,fout))**2; x[-fout:]*= w[:,None] if x.ndim==2 else w
    return x
def trim(x, floor_db=-60):
    a=np.abs(x).max(axis=1) if x.ndim==2 else np.abs(x); p=a.max()+1e-12
    idx=np.where(a>p*10**(floor_db/20))[0]
    return x[:idx[-1]+480] if len(idx) else x
PENDING=[]
def write(fn, x):
    PENDING.append((fn, x))
def flush_set(target_peak_db=-7.0):
    """normalise a whole set so its median sample peak sits at target (keeps relative dynamics)"""
    global PENDING
    peaks=[20*np.log10(np.abs(x).max()+1e-9) for _,x in PENDING]
    g=10**((target_peak_db-np.median(peaks))/20) if peaks else 1.0
    for fn,x in PENDING: _write(fn, x*g)
    PENDING=[]
    return 20*np.log10(g)
def _write(fn, x):
    if x.ndim==1: x=x[:,None]
    p=np.abs(x).max()
    if p>0.98: x=x*0.98/p
    tmp=f"{OUT}/{fn}.wav"
    wavio.write(tmp, SR, (np.clip(x,-1,1)*8388607).astype(np.int32)<<8)
    subprocess.run(["ffmpeg","-y","-loglevel","error","-i",tmp,"-c:a","flac","-compression_level","8","-sample_fmt","s32","-bits_per_raw_sample","24",f"{OUT}/{fn}"],check=True)
    os.remove(tmp)
def stereo_needed(x):
    if x.ndim==1 or x.shape[1]<2: return False
    side=np.sqrt(((x[:,0]-x[:,1])**2).mean()); mid=np.sqrt(((x[:,0]+x[:,1])**2).mean())+1e-12
    return side/mid>0.15

def build_notes(key, name, part, stem, maxlen=2.0, per_pitch=3, lo=0, hi=127):
    x=get(stem); st=stereo_needed(x); xs=x if st else mono(x)
    notes=[n for n in json.load(open(f"notes_{stem}.json")) if lo<=n["midi"]<=hi]
    starts=sorted(n["start"] for n in notes)
    by=defaultdict(list)
    for n in notes: by[n["midi"]].append(n)
    samples=[]; dropped=0
    xm=mono(x)
    def spec_f0(seg, flo, fhi):
        seg=seg[int(0.02*SR):int(0.35*SR)]
        if len(seg)<4800: return None
        S=np.abs(np.fft.rfft(seg*np.hanning(len(seg)),1<<17)); N=1<<17
        cands=np.arange(flo,fhi,0.25); sc=[sum(S[int(c*h*N/SR)]/h**0.5 for h in range(1,6)) for c in cands]
        return cands[int(np.argmax(sc))]
    flo,fhi=(28,200) if part=="bass" else (140,1400)
    def verified(n):
        nonlocal dropped
        f=spec_f0(xm[n["start"]:n["end"]+int(0.3*SR)], flo, fhi)
        if f is None: return True
        err=(69+12*np.log2(f/440)-(n["midi"]+n["cents"]/100))*100
        fold=err-1200*np.round(err/1200)
        if abs(fold)>40: dropped+=1; return False      # slide / bend / ghost note: wrong pitch label
        n["cents"]=float(n["cents"]+0.5*fold)           # refine tuning with both estimates
        return True
    for midi in list(by):
        by[midi]=[n for n in by[midi] if verified(n)]
        if not by[midi]: del by[midi]
    for midi,ns in sorted(by.items()):
        # prefer stable, long, loud notes
        ns=sorted(ns, key=lambda n: (n["stab"]>0.25, -(n["end"]-n["start"]) - 0.002*n["level"]))[:per_pitch]
        for j,n in enumerate(ns):
            s=n["start"]-48
            nxt=[t for t in starts if t>n["start"]+int(0.03*SR)]
            e=min(s+int(maxlen*SR), (nxt[0]-32) if nxt else s+int(maxlen*SR))
            seg=fade(trim(xs[s:e].astype(np.float32)), fout=min(2400, (e-s)//3))
            fn=f"{key}_{midi}_{j+1}.flac"; write(fn, seg)
            samples.append(dict(file=fn, midi=midi, cents=round(n["cents"],1), level=round(n["level"],2)))
    gdb=flush_set()
    for smp in samples: smp["level"]=round(float(smp["level"]+gdb),2)
    print(f"  {name:22s} {len(samples):3d} note samples, {len(by)} pitches, {dropped} mislabeled notes dropped, {'stereo' if st else 'mono'}")
    return dict(key=key, name=name, part=part, kind="notes", samples=samples)

def build_chords(key, name, part, stem, maxlen=1.4, per_chord=6, mute_pool=0):
    x=get(stem); st=stereo_needed(x); xs=x if st else mono(x)
    ch=json.load(open(f"chops_{stem}.json"))
    by=defaultdict(list)
    for c in ch:
        if c["fit"]>=0.5: by[(c["root"],c["q"])].append(c)
    samples=[]
    # re-judge quality from the chop's own third: clear major / clear minor / neutral (no third)
    from instr import chroma
    xm=mono(get(stem)); rejudged=defaultdict(list)
    for (r,q),cs in by.items():
        for c in cs:
            cc=chroma(xm[c["start"]+240:min(c["end"], c["start"]+int(0.3*SR))])
            M3, m3 = cc[(r+4)%12], cc[(r+3)%12]
            if max(M3,m3) < 0.06: q2="5"
            elif M3 > 2.0*m3: q2="maj"
            elif m3 > 2.0*M3: q2="min"
            else: continue
            rejudged[(r,q2)].append(c)
    by=rejudged
    for (r,q),cs in sorted(by.items(), key=lambda kv:-len(kv[1])):
        if len(cs)<2: continue
        cs=sorted(cs, key=lambda c:-c["fit"])[:per_chord*2]
        cs=sorted(cs, key=lambda c:c["level"]); idx=np.unique(np.round(np.linspace(0,len(cs)-1,min(per_chord,len(cs)))).astype(int))
        for j,i in enumerate(idx):
            c=cs[i]; s=c["start"]-48; e=min(c["end"]-32, s+int(maxlen*SR))
            seg=fade(trim(xs[s:e].astype(np.float32)), fout=min(2400,(e-s)//3))
            fn=f"{key}_{r}{q}_{j+1}.flac"; write(fn, seg)
            samples.append(dict(file=fn, root=r, q=q, level=round(c["level"],2)))
    mutes=0
    if mute_pool:
        cand=[c for c in ch if c["fit"]<0.4 and (c["end"]-c["start"])<0.2*SR]
        cand=sorted(cand, key=lambda c:-c["level"])[:mute_pool]
        for j,c in enumerate(cand):
            s=c["start"]-48; e=min(c["end"]-32, s+int(0.2*SR))
            seg=fade(trim(xs[s:e].astype(np.float32)), fout=min(960,(e-s)//3))
            fn=f"{key}_mute_{j+1}.flac"; write(fn, seg)
            samples.append(dict(file=fn, root=-1, q="mute", level=round(c["level"],2))); mutes+=1
    gdb=flush_set(-8.0)
    for smp in samples: smp["level"]=round(float(smp["level"]+gdb),2)
    roots=sorted(set((s["root"],s["q"]) for s in samples if s["q"]!="mute"))
    NN=["C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"]
    print(f"  {name:22s} {len(samples):3d} chord samples ({mutes} muted), chords {[NN[r]+{'min':'m','maj':'','5':'5'}[q] for r,q in roots]}, {'stereo' if st else 'mono'}")
    return dict(key=key, name=name, part=part, kind="chords", samples=samples)

if __name__=="__main__":
    shutil.rmtree(OUT, ignore_errors=True); os.makedirs(OUT)
    insts=[
        build_notes("bass1","Session Bass","bass","bass1", maxlen=2.0, per_pitch=4),
        build_notes("bass2","5-String Fingered","bass","bass2", maxlen=2.0, per_pitch=3),
        build_chords("skanky","Skanky Guitar","skank","skanky", maxlen=0.6, per_chord=8, mute_pool=6),
        build_chords("chop","Chop Guitar","skank","chop", maxlen=1.2, per_chord=6, mute_pool=4),
        build_chords("clean","Clean Guitar","skank","clean", maxlen=1.4, per_chord=4),
        build_chords("bubble","Bubble Organ","organ","bubble", maxlen=0.6, per_chord=8),
        build_chords("percorg","Perc Organ","organ","percorg", maxlen=0.6, per_chord=6),
        build_chords("organ4","Organ 4","organ","organ4", maxlen=1.0, per_chord=5),
        build_chords("piano1","Session Piano","piano","piano1", maxlen=1.4, per_chord=6),
        build_chords("piano2","Acoustic Piano","piano","piano2", maxlen=1.4, per_chord=4),
        build_notes("leadgtr","Lead Guitar","lead","leadgtr", maxlen=1.6, per_pitch=2),
        build_notes("pluckjc","Pluck Guitar (JC)","lead","pluckjc", maxlen=1.0, per_pitch=4),
        build_notes("pluckk","Pluck Guitar (Kahn)","lead","pluckk", maxlen=1.0, per_pitch=4, hi=80),
    ]
    # loudness trims measured against the synth sound of the same part (riddim playback, RMS)
    TRIM={"bass1":7.0,"bass2":4.0,"skanky":17.0,"chop":14.0,"clean":13.0,"bubble":6.0,"percorg":8.5,"organ4":7.0,
          "piano1":12.5,"piano2":12.5,"leadgtr":-2.0,"pluckjc":8.5,"pluckk":8.0}
    for ins in insts: ins["trimDb"]=TRIM.get(ins["key"],0.0)
    json.dump(dict(sampleRate=SR, instruments=insts), open(f"{OUT}/instruments.json","w"), indent=1)
    tot=sum(os.path.getsize(os.path.join(OUT,f)) for f in os.listdir(OUT))
    print("total %.1f MB, %d files"%(tot/1e6, len(os.listdir(OUT))))
