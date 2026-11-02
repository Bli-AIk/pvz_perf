"""Render the evidence-backed Chinese lecture with editable slides and local TTS."""
import argparse
import json
import math
from pathlib import Path
import re
import shutil
import subprocess
import sys
import wave

from PIL import Image, ImageDraw, ImageFont

W, H = 1920, 1080
BG, PANEL, TEXT, MUTED, GREEN, BLUE = '#101820', '#1a2934', '#edf4f7', '#a7bac7', '#c3ed6d', '#68c6ed'
FONTS = Path('C:/Windows/Fonts')


def font(size, bold=False, code=False):
    return ImageFont.truetype(str(FONTS/('consola.ttf' if code else 'msyhbd.ttc' if bold else 'msyh.ttc')), size)


def wrap(draw, text, selected_font, width):
    result, line = [], ''
    for char in text:
        if char == '\n' or draw.textlength(line+char, font=selected_font) > width:
            result.append(line)
            line = '' if char == '\n' else char
        else:
            line += char
    result.append(line)
    return result


def paragraph(draw, xy, text, size, width, color=TEXT, bold=False, spacing=15):
    x, y = xy
    selected_font = font(size, bold)
    for line in wrap(draw, text, selected_font, width):
        draw.text((x,y), line, font=selected_font, fill=color)
        y += size+spacing
    return y


def box(draw, rect, title, subtitle='', color=BLUE):
    draw.rounded_rectangle(rect, radius=20, fill=PANEL, outline=color, width=3)
    x,y,x2,y2 = rect
    paragraph(draw, (x+24,y+22), title, 32, x2-x-48, color, True)
    if subtitle:
        paragraph(draw, (x+24,y+88), subtitle, 27, x2-x-48, MUTED)


def arrow(draw, x1,y1,x2,y2):
    draw.line((x1,y1,x2,y2), fill=GREEN, width=5)
    angle = math.atan2(y2-y1,x2-x1)
    points = [(x2,y2)] + [(x2-20*math.cos(angle+a),y2-20*math.sin(angle+a)) for a in (-.45,.45)]
    draw.polygon(points, fill=GREEN)


def diagram(draw, kind, left):
    right = 1815
    width = right-left
    if kind in ('lookup','identity'):
        box(draw,(left,345,right,485),'已知的类型与身份','type / trackId / emitterId')
        arrow(draw,left+width//2,500,left+width//2,560)
        box(draw,(left,580,right,750),'编译期生成的数据','静态参数表 / MPHF / 整数区间',GREEN)
    elif kind in ('candidates','mask'):
        paragraph(draw,(left,345),'全体对象 / 全部轨道',32,width)
        for i in range(30):
            x=left+(i%10)*int(width/10);y=420+(i//10)*75
            color=GREEN if i in (2,7,12,17,22,27) else '#354957'
            draw.rounded_rectangle((x,y,x+int(width/10)-12,y+52),radius=8,fill=color)
        arrow(draw,left+width//2,670,left+width//2,720)
        paragraph(draw,(left,752),'只处理相关候选，保留顺序与语义',30,width,GREEN)
    elif kind=='batches':
        for i,label in enumerate(['N','A','N','A','N']):
            x=left+i*int(width/5)
            draw.rounded_rectangle((x,365,x+int(width/5)-15,470),radius=12,fill=PANEL,outline=BLUE,width=3)
            draw.text((x+35,385),label,font=font(45,True),fill=TEXT)
        arrow(draw,left+width//2,500,left+width//2,555)
        box(draw,(left,580,right,760),'保持原顺序，合并兼容状态','减少draw与管线切换；不是删效果',GREEN)
    elif kind=='quad':
        draw.rectangle((left+40,360,right-40,650),outline=BLUE,width=5)
        draw.line((left+40,360,right-40,650),fill=GREEN,width=4)
        for x,y in [(left+40,360),(right-40,360),(left+40,650),(right-40,650)]:
            draw.ellipse((x-13,y-13,x+13,y+13),fill=GREEN)
        paragraph(draw,(left,710),'4个动态顶点 + 6个静态索引',31,width,GREEN)
        paragraph(draw,(left,770),'240 → 160 字节 / 矩形',36,width,TEXT,True)
    elif kind=='phase':
        y=490;draw.line((left+20,y,right-20,y),fill=BLUE,width=6)
        for i in range(5):
            x=left+30+i*(width-60)/4
            draw.line((x,y-24,x,y+24),fill=GREEN,width=5)
            draw.text((x-15,y+40),str(i),font=font(30),fill=TEXT)
        paragraph(draw,(left,345),'整数帧号 + 纳秒余数',38,width,GREEN,True)
        paragraph(draw,(left,650),'同一时间点 → 同一插值结果\n根运动、绘制、保龄球共用',31,width)
    elif kind=='trig':
        box(draw,(left,345,right,510),'每条轨道：一份变换矩阵','共享sin/cos范围归一化')
        arrow(draw,left+width//2,525,left+width//2,580)
        box(draw,(left,600,right,770),'矩形四个角复用','不是四次重新求三角函数',GREEN)
    else:
        box(draw,(left,345,right,520),'CPU：业务与共享求值','动画、粒子、根运动、最终几何')
        arrow(draw,left+width//2,535,left+width//2,585)
        box(draw,(left,605,right,785),'GPU：采样、混合、光栅化','按实际负载选择分工，用测量决定',GREEN)


def slide(scene, number, tables, output):
    image = Image.new('RGB',(W,H),BG)
    draw = ImageDraw.Draw(image)
    draw.rectangle((0,0,W,8),fill=GREEN)
    draw.text((100,52),scene['chapter'],font=font(26,True),fill=GREEN)
    draw.text((1740,52),f'{number:02}',font=font(26),fill=MUTED)
    paragraph(draw,(100,110),scene['title'],61,1720,TEXT,True)
    paragraph(draw,(102,205),scene['subtitle'],30,1700,MUTED)
    if scene.get('table'):
        table=tables[scene['table']]
        rows=[table['columns']]+table['rows']
        col_widths=table.get('widths',[1720/len(rows[0])]*len(rows[0]))
        row_h=min(105, int(530/len(rows)))
        y=325
        for row_index,row in enumerate(rows):
            x=100
            draw.rounded_rectangle((100,y,1820,y+row_h-8),radius=9,fill='#293e4c' if row_index==0 else PANEL)
            for col_index,cell in enumerate(row):
                paragraph(draw,(x+22,y+16),str(cell),28 if len(rows)>6 else 31,
                          int(col_widths[col_index])-40,GREEN if row_index==0 else TEXT,row_index==0,spacing=5)
                x+=col_widths[col_index]
            y+=row_h
        paragraph(draw,(102,850),table.get('note',''),23,1700,MUTED,spacing=8)
    elif scene.get('code'):
        draw.rounded_rectangle((95,310,1080,840),radius=24,fill=PANEL)
        y=340
        for line in scene['code'].splitlines():
            draw.text((125,y),line,font=font(27,code=all(ord(c)<128 for c in line)),fill=MUTED if line.startswith('//') else TEXT)
            y+=43
        diagram(draw,scene.get('diagram'),1140)
    else:
        diagram(draw,scene.get('diagram'),1030)
        text=scene['narration'].split('。')
        bullets=text[:3]
        y=350
        for item in bullets:
            if item:
                draw.ellipse((105,y+14,116,y+25),fill=GREEN)
                y=paragraph(draw,(140,y),item,33,780,spacing=15)+38
    if scene.get('footer'):
        paragraph(draw,(100,900),scene['footer'],22,1700,MUTED,spacing=5)
    draw.text((100,1020),'pvz_perf  /  v3.2.5  /  实现与数据可追溯',font=font(20),fill=MUTED)
    image.save(output)


def timestamp(seconds):
    ms=round(seconds*1000)
    return f'{ms//3600000:02}:{ms//60000%60:02}:{ms//1000%60:02},{ms%1000:03}'


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--tables',type=Path,required=True)
    parser.add_argument('--slides-only',action='store_true')
    args=parser.parse_args()
    here=Path(__file__).resolve().parent
    scenes=json.loads((here/'storyboard.json').read_text(encoding='utf-8'))
    tables=json.loads(args.tables.read_text(encoding='utf-8'))
    output=args.output.resolve();output.mkdir(parents=True,exist_ok=True)
    sentences=[]
    for index,scene in enumerate(scenes):
        directory=output/f'scene-{index:02}';directory.mkdir(exist_ok=True)
        slide(scene,index+1,tables,directory/'slide.png')
        for j,text in enumerate(re.findall(r'[^。！？]+[。！？]?',scene['narration'])):
            sentences.append(dict(text=text,path=str(directory/f'voice-{j:02}.wav')))
    # A contact sheet is useful before expensive video encoding.
    sheet=Image.new('RGB',(960,math.ceil(len(scenes)/3)*180),BG)
    for i in range(len(scenes)):
        with Image.open(output/f'scene-{i:02}/slide.png')as im:sheet.paste(im.resize((320,180)),((i%3)*320,(i//3)*180))
    sheet.save(output/'contact-sheet.png')
    if args.slides_only:return
    (output/'speech.json').write_text(json.dumps(sentences,ensure_ascii=False),encoding='utf-8')
    speech=output/'synthesize.ps1'
    speech.write_text('''param([string]$Manifest)
Add-Type -AssemblyName System.Speech
$synth=[System.Speech.Synthesis.SpeechSynthesizer]::new()
$synth.SelectVoice('Microsoft Huihui Desktop')
$synth.Rate=0
try {
    foreach($item in (Get-Content -LiteralPath $Manifest -Raw -Encoding utf8 | ConvertFrom-Json)) {
        $synth.SetOutputToWaveFile($item.path)
        $synth.Speak($item.text)
        $synth.SetOutputToNull()
    }
} finally {$synth.Dispose()}
''',encoding='utf-8-sig')
    subprocess.run(['pwsh','-NoProfile','-File',str(speech),'-Manifest',str(output/'speech.json')],check=True)
    global_srt=[];timeline=[];elapsed=0;concat=[]
    ffmpeg=shutil.which('ffmpeg')
    if not ffmpeg:raise RuntimeError('ffmpeg is required')
    for i,scene in enumerate(scenes):
        directory=output/f'scene-{i:02}';parts=[s for s in sentences if Path(s['path']).parent==directory]
        local_srt=[];position=0;pcm=[];params=None
        for sentence in parts:
            with wave.open(sentence['path'],'rb')as audio:
                params=audio.getparams();duration=audio.getnframes()/audio.getframerate();pcm.append(audio.readframes(audio.getnframes()))
            caption=sentence['text'];lines=[caption[j:j+34]for j in range(0,len(caption),34)];caption='\n'.join(lines)
            local_srt.append(f'{len(local_srt)+1}\n{timestamp(position)} --> {timestamp(position+duration)}\n{caption}\n')
            global_srt.append(f'{len(global_srt)+1}\n{timestamp(elapsed+position)} --> {timestamp(elapsed+position+duration)}\n{caption}\n')
            position+=duration
        duration=max(position+1.2,10 if scene.get('table')else 5)
        with wave.open(str(directory/'narration.wav'),'wb')as out:
            out.setparams(params);out.writeframes(b''.join(pcm))
        (directory/'captions.srt').write_text('\n'.join(local_srt),encoding='utf-8')
        vf="subtitles=captions.srt:force_style='FontName=Microsoft YaHei,FontSize=11,MarginV=15,Outline=1',fade=t=in:st=0:d=0.25,fade=t=out:st="+str(max(0,duration-.25))+":d=0.25"
        command=[ffmpeg,'-y','-hide_banner','-loglevel','warning','-loop','1','-framerate','24','-i','slide.png','-i','narration.wav','-vf',vf,'-af','apad','-t',str(duration),'-c:v','libx264','-preset','fast','-crf','21','-pix_fmt','yuv420p','-r','24','-c:a','aac','-b:a','128k','segment.mp4']
        subprocess.run(command,cwd=directory,check=True)
        concat.append("file '"+(directory/'segment.mp4').as_posix()+"'")
        timeline.append(dict(scene=i+1,title=scene['title'],start=elapsed,duration=duration,narration=scene['narration']))
        elapsed+=duration
        print(f'Rendered {i+1}/{len(scenes)}; {elapsed:.1f}s',flush=True)
    (output/'segments.txt').write_text('\n'.join(concat),encoding='utf-8')
    (output/'captions.srt').write_text('\n'.join(global_srt),encoding='utf-8')
    (output/'timeline.json').write_text(json.dumps(timeline,ensure_ascii=False,indent=2),encoding='utf-8')
    (output/'narration.txt').write_text('\n\n'.join(f"{x['scene']:02} {x['title']}\n{x['narration']}"for x in timeline),encoding='utf-8')
    subprocess.run([sys.executable,str(here/'finalize_video.py'),str(output)],check=True)
    print('Completed '+str(output),flush=True)


if __name__=='__main__':main()
