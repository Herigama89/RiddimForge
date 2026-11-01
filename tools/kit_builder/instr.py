import numpy as np, scipy.signal as ss
from load import load, mono
from load2 import load2
SR=48000
STEMS={ # name: (loader, file)
 "bass1":(load,"BASS_2"), "bass2":(load2,"5String BopBass Fingered"),
 "skanky":(load,"SKANKY GTR"), "chop":(load,"CHOP GTR"), "clean":(load2,"Electric Guitar Clean"),
 "bubble":(load,"BUB ORG"), "percorg":(load,"PERC ORG"), "organ4":(load2,"Organ 4"),
 "piano1":(load,"PIANO"), "piano2":(load2,"Acoustic Piano"),
 "pluckjc":(load,"PLUCK GTR JACK CARVER"), "pluckk":(load,"PLUCK GTR KAHN"), "leadgtr":(load,"LEAD GTR JACK CARVER"),
}
def get(name):
    f,fn=STEMS[name]; return f(fn)

def yin_track(x, fmin=35, fmax=1200, hop=256, win=2048, thresh=0.15):
    """frame-wise YIN f0 (Hz, 0 = unvoiced) and rms"""
    n=(len(x)-win)//hop; f0=np.zeros(n); rms=np.zeros(n); conf=np.zeros(n)
    tmin=int(SR/fmax); tmax=min(int(SR/fmin), win//2)
    for i in range(n):
        fr=x[i*hop:i*hop+win]; rms[i]=np.sqrt((fr**2).mean())
        if rms[i]<1e-4: continue
        # difference function via FFT autocorrelation
        w=win//2
        a=fr[:w+tmax]
        r=np.fft.irfft(np.abs(np.fft.rfft(a,2*len(a)))**2)[:tmax+1]
        e=np.cumsum(np.concatenate([[0],a**2]))
        d=e[w]+ (e[np.arange(tmax+1)+w]-e[np.arange(tmax+1)]) - 2*r
        d[0]=1
        cm=d[1:]*np.arange(1,tmax+1)/np.maximum(np.cumsum(d[1:]),1e-12); cm=np.concatenate([[1],cm])
        idx=np.where(cm[tmin:]<thresh)[0]
        if len(idx)==0: continue
        t=idx[0]+tmin
        while t+1<len(cm) and cm[t+1]<cm[t]: t+=1
        if 1<=t<len(cm)-1:
            a0,b0,c0=cm[t-1],cm[t],cm[t+1]; den=a0-2*b0+c0; p=0.5*(a0-c0)/den if den!=0 else 0
        else: p=0
        f0[i]=SR/(t+p); conf[i]=1-cm[t]
    return f0,rms,conf

def chroma(seg):
    """12-bin chroma from a segment (log-frequency mapping, 55 Hz .. 2 kHz)"""
    n=len(seg); w=seg*np.hanning(n); S=np.abs(np.fft.rfft(w,1<<16)); f=np.fft.rfftfreq(1<<16,1/SR)
    m=(f>55)&(f<2000); f=f[m]; S=S[m]**2
    midi=69+12*np.log2(f/440); pc=np.mod(np.round(midi),12).astype(int)
    c=np.bincount(pc,weights=S,minlength=12); return c/(c.sum()+1e-12)

QUALS={"maj":[0,4,7],"min":[0,3,7],"7":[0,4,7,10],"m7":[0,3,7,10],"sus4":[0,5,7]}
def chord_from_chroma(c):
    best=None
    for r in range(12):
        for q,iv in QUALS.items():
            t=np.zeros(12); t[[(r+i)%12 for i in iv]]=1; t/=np.linalg.norm(t)
            sc=float(np.dot(c/np.linalg.norm(c),t))
            if best is None or sc>best[0]: best=(sc,r,q)
    return best

def env_onsets(x, rel_db=-30, gap=0.07, jump=6):
    return onsets_generic(mono(x), rel_db, gap, jump)

def onsets_generic(m, rel_db=-30, gap=0.07, jump=6, hop=128):
    a=np.abs(m); n=len(a)//hop; env=a[:n*hop].reshape(n,hop).max(1); e=20*np.log10(env+1e-9); pk=e.max()
    out=[]; last=-1e9
    for i in range(8,n):
        if e[i]>pk+rel_db and e[i]-e[i-8:i-1].mean()>jump and (i-last)*hop/SR>gap:
            s=i*hop-2*hop; seg=a[max(0,s):s+6*hop]; j=np.argmax(seg>0.3*seg.max()); out.append(max(0,s)+j); last=i
    return out
