from load import *
import numpy as np, scipy.signal as ss
sr=48000; PRE=1024; N=16384
def stft_stats(src, y, hop=2048):
    C=np.stack([mono(load(s)) for s in src]); S,T=C.shape; win=np.hanning(N)
    Rcc=np.zeros((N//2+1,S,S),complex); Rcy=np.zeros((N//2+1,S),complex); E=np.zeros(S)
    for st in range(0,T-N,hop):
        blk=C[:,st:st+N]
        if np.abs(blk).max()<1e-4: continue
        Cf=np.fft.rfft(blk*win,axis=1); yf=np.fft.rfft(y[st:st+N]*win)
        Rcc+=np.einsum('sf,tf->fst',Cf,Cf.conj()); Rcy+=(Cf*yf.conj()).T
    return Rcc,Rcy
def solve(Rcc,Rcy,lam):
    F,S,_=Rcc.shape; H=np.zeros((F,S),complex)
    # per-source ridge proportional to that source's own power (smoothed over freq)
    d=np.real(np.einsum('fss->fs',Rcc)); ds=ss.convolve(d,np.ones((65,1))/65,mode='same')
    for f in range(F):
        H[f]=np.linalg.solve(Rcc[f]+np.diag(lam*ds[f]+1e-12),Rcy[f]).conj()
    h=np.roll(np.fft.irfft(H,n=N,axis=0).T,PRE,axis=1)[:,:8192]
    w=np.ones(8192); w[-2048:]=np.hanning(4096)[2048:]; w[:512]=np.hanning(1024)[:512]
    return h*w
def predict(src,h):
    return sum(ss.fftconvolve(mono(load(s)),h[i])[PRE:PRE+len(load(s))] for i,s in enumerate(src))
def score(y,p):
    out=[]
    for band in [(40,200),(200,1000),(1000,5000)]:
        b,a=ss.butter(4,band,btype='band',fs=sr); r=ss.lfilter(b,a,y-p); t=ss.lfilter(b,a,y)
        out.append(10*np.log10((r**2).sum()/(t**2).sum()))
    return out
if __name__=="__main__":
    y=mono(load("OH L"))
    for kick in ["KICK 1","KICK 2"]:
        src=[kick,"SN T","HI HAT","TOM 1","TOM 2","TAMBOURINE"]
        Rcc,Rcy=stft_stats(src,y)
        for lam in [0.05,0.2]:
            h=solve(Rcc,Rcy,lam); p=predict(src,h)
            print(kick,"lam",lam,"residual by band (40-200, 200-1k, 1k-5k):",np.round(score(y,p),1))
