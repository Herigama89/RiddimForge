import numpy as np, scipy.io.wavfile as w, warnings, os
warnings.filterwarnings('ignore')
D2="/home/claude/samples_in2/stems"
_c={}
def load2(name):
    if name in _c: return _c[name]
    sr,x=w.read(os.path.join(D2,f"Reggae Instrumental 2_{name}.wav")); x=x.astype(np.float32)/2**31; _c[name]=x; return x
