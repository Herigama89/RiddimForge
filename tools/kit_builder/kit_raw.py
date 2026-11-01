import json, subprocess, numpy as np
K="/home/claude/RiddimForge/Resources/Kits/"; O="/home/claude/kitraw/"
d=json.load(open(K+"kits.json")); lines=[]
for ki,kit in enumerate(d["kits"]):
    lines.append(f"KIT {kit['name']}")
    for a in kit["articulations"]:
        routes=" ".join(f"{b},{ {'mono':0,'L':1,'R':2}[m]}" for b,m in a["routing"])
        lines.append(f"ART {a['key']} {a['choke']} {len(a['notes'])} {' '.join(map(str,a['notes']))} {len(a['routing'])} {routes}")
        for s in a["samples"]:
            ch=int(subprocess.run(["ffprobe","-v","error","-show_entries","stream=channels","-of","csv=p=0",K+s['file']],capture_output=True,text=True).stdout)
            raw=subprocess.run(["ffmpeg","-v","error","-i",K+s['file'],"-f","f32le","-acodec","pcm_f32le","-"],capture_output=True).stdout
            x=np.frombuffer(raw,np.float32).reshape(-1,ch)
            fn=s['file'].replace('.flac','.f32'); x.T.copy().tofile(O+fn)
            lines.append(f"SMP {fn} {s['level']} {ch} {len(x)}")
open(O+"manifest.txt","w").write("\n".join(lines)+"\n")
print(len(lines),"lines")
