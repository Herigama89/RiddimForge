import numpy as np, scipy.io.wavfile as w, warnings, os
warnings.filterwarnings('ignore')
D="/home/claude/samples_in/plugin spack"
_cache={}
def load(name):
    if name in _cache: return _cache[name]
    sr,x=w.read(os.path.join(D,f"1_{name}.wav"))
    x=x.astype(np.float32)/2**31
    _cache[name]=x
    return x
def mono(x): return x.mean(axis=1)
def onsets(x, sr=48000, thresh_db=-30, min_gap=0.06, rel_db=-24):
    m=np.abs(mono(x)); hop=128
    n=len(m)//hop; env=m[:n*hop].reshape(n,hop).max(axis=1)
    envdb=20*np.log10(env+1e-9)
    peak=envdb.max()
    # rise detection: env jumps > 9 dB over ~5ms vs previous 20ms min
    out=[]; last=-1e9
    for i in range(8,n):
        prev=envdb[i-8:i-1].mean()
        if envdb[i]>peak+rel_db and envdb[i]>thresh_db and envdb[i]-prev>9 and (i-last)*hop/sr>min_gap:
            # refine to sample: first sample above 30% of local peak
            s=i*hop-hop*2; seg=m[max(0,s):s+hop*6]
            lp=seg.max(); j=np.argmax(seg>0.3*lp)
            out.append((max(0,s)+j, float(20*np.log10(lp+1e-9))))
            last=i
    return out
