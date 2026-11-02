"""Join video at exact frame boundaries and encode one continuous narration track."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import wave


def stamp(seconds):
    ms=round(seconds*1000)
    return f'{ms//3600000:02}:{ms//60000%60:02}:{ms//1000%60:02},{ms%1000:03}'


def unstamp(value):
    h,m,s,ms=map(int,re.split('[:,]',value))
    return h*3600+m*60+s+ms/1000


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output',type=Path)
    args=parser.parse_args();out=args.output.resolve()
    timeline=json.loads((out/'timeline.json').read_text(encoding='utf-8'))
    elapsed=0;concat=[];captions=[];audio_parts=[];params=None
    for i,scene in enumerate(timeline):
        directory=out/f'scene-{i:02}'
        info=json.loads(subprocess.check_output(['ffprobe','-v','error','-select_streams','v:0',
            '-show_entries','stream=nb_frames,r_frame_rate','-of','json',str(directory/'segment.mp4')]))['streams'][0]
        numerator,denominator=map(int,info['r_frame_rate'].split('/'))
        duration=int(info['nb_frames'])*denominator/numerator
        concat.extend(["file '"+(directory/'segment.mp4').as_posix()+"'",f'duration {duration:.12f}'])
        with wave.open(str(directory/'narration.wav'),'rb')as audio:
            if params is not None and audio.getparams()[:3]!=params[:3]:raise RuntimeError('Narration PCM formats differ')
            params=audio.getparams();pcm=audio.readframes(audio.getnframes())
        expected=round(duration*params.framerate)*params.nchannels*params.sampwidth
        if len(pcm)>expected:raise RuntimeError('Narration exceeds video segment')
        audio_parts.append(pcm+b'\0'*(expected-len(pcm)))
        text=(directory/'captions.srt').read_text(encoding='utf-8')
        for block in text.strip().split('\n\n'):
            lines=block.splitlines()
            if len(lines)<3:continue
            start,end=lines[1].split(' --> ')
            captions.append(f'{len(captions)+1}\n{stamp(elapsed+unstamp(start))} --> {stamp(elapsed+unstamp(end))}\n'+ '\n'.join(lines[2:])+'\n')
        scene['start']=elapsed;scene['duration']=duration;elapsed+=duration
    (out/'segments-video.txt').write_text('\n'.join(concat),encoding='utf-8')
    with wave.open(str(out/'narration-full.wav'),'wb')as audio:
        audio.setparams(params);audio.writeframes(b''.join(audio_parts))
    silent=out/'pvz_perf-v3.2.5-performance-silent.mp4'
    voiced=out/'pvz_perf-v3.2.5-performance-narrated.mp4'
    subprocess.run(['ffmpeg','-y','-hide_banner','-loglevel','warning','-f','concat','-safe','0',
        '-i',str(out/'segments-video.txt'),'-map','0:v:0','-c:v','copy','-an','-movflags','+faststart',str(silent)],check=True)
    subprocess.run(['ffmpeg','-y','-hide_banner','-loglevel','warning','-i',str(silent),'-i',str(out/'narration-full.wav'),
        '-map','0:v:0','-map','1:a:0','-c:v','copy','-c:a','aac','-b:a','128k','-t',str(elapsed),
        '-movflags','+faststart',str(voiced)],check=True)
    (out/'captions.srt').write_text('\n'.join(captions),encoding='utf-8')
    (out/'timeline.json').write_text(json.dumps(timeline,ensure_ascii=False,indent=2),encoding='utf-8')
    print(f'Finalized {elapsed:.3f}s; exact video frame boundaries and continuous PCM narration')


if __name__=='__main__':main()
