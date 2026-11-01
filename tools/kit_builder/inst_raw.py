import json, subprocess, numpy as np
K="/home/claude/RiddimForge/Resources/Instruments/"; O="/home/claude/instraw/"
d=json.load(open(K+"instruments.json")); lines=[]
qmap={"maj":0,"min":1,"5":2,"mute":3}
for ins in d["instruments"]:
    lines.append(f"SET {ins['key']} {ins['part']} {1 if ins['kind']=='chords' else 0} {ins.get('trimDb',0)} {ins['name']}")
    for s in ins["samples"]:
        ch=int(subprocess.run(["ffprobe","-v","error","-show_entries","stream=channels","-of","csv=p=0",K+s['file']],capture_output=True,text=True).stdout)
        raw=subprocess.run(["ffmpeg","-v","error","-i",K+s['file'],"-f","f32le","-acodec","pcm_f32le","-"],capture_output=True).stdout
        x=np.frombuffer(raw,np.float32).reshape(-1,ch); fn=s['file'].replace('.flac','.f32'); x.T.copy().tofile(O+fn)
        lines.append(f"SMP {fn} {ch} {len(x)} {s.get('midi',-1)} {s.get('cents',0)} {s.get('root',-1)} {qmap.get(s.get('q','maj'),0)} {s['level']}")
open(O+"manifest.txt","w").write("\n".join(lines)+"\n"); print(len(lines))
