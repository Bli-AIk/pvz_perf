"""Build slide tables from verified benchmark artifacts, with explicit scopes."""
import argparse
import csv
import json
from pathlib import Path
import re
import statistics


def read_csv(path):
    return list(csv.DictReader(line for line in path.read_text().splitlines() if not line.startswith('#')))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--benchmarks',type=Path,required=True)
    parser.add_argument('--project-endless',type=Path,required=True)
    parser.add_argument('--external',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();base=args.benchmarks
    if not json.loads((base/'manifest.json').read_text()).get('complete'):
        raise RuntimeError('Microbenchmark suite is incomplete')
    production=json.loads((base/'production/summary.json').read_text())

    def query(kind,kernel,run=1):
        return next(r['min_ns']for r in production if r['kind']==kind and r['kernel']==kernel and r['process']==run)

    tables={}
    tables['tables']=dict(columns=['查询','原版EXE','v3.2.4生产函数'],widths=[880,420,420],rows=[
        [name,f'{query("retail",key):.3f}',f'{query("current",key):.3f}']for name,key in [
            ('植物伤害范围','damage_range'),('花盆高度','flower_pot_height'),
            ('种子特殊价格','seed_cost_tail'),('商店有效商品','store_priced_items')]],
        note='ns/查询。第一进程min-of-5；两个进程与结果向量均归档。x86/x64、ABI、编译器与LTO不同。')
    tables['candidates']=dict(columns=['负载','全扫','索引／掩码'],widths=[900,410,410],rows=[
        ['1024僵尸／本行171（ns）',f'{query("structure","zombies_actual_pool_1024"):.1f}',f'{query("structure","zombies_actual_row_171"):.1f}'],
        ['48植物／54格（ns）',f'{query("structure","plants_actual_pool_48"):.2f}',f'{query("structure","plants_actual_bucket_48"):.2f}'],
        ['0%活动轨道（µs/pass）',f'{query("structure","tracks_scan_0pct")/1000:.2f}',f'{query("structure","tracks_mask_0pct")/1000:.2f}'],
        ['绘制组（µs/pass）',f'{query("structure","groups_all_18")/1000:.2f}',f'{query("structure","groups_occupied")/1000:.2f}']],
        note='当前真实对象布局；轨道与绘制组使用相同下游桩，不代表完整业务Update/Draw。')
    identity=list((base/'table-models').glob('*emitter_names/reports-1/summary.csv'))
    particle=read_csv(identity[0]);wanted={'named_exact_full_corpus':'粒子精确名字／固定语料',
        'family_prefix_full_corpus':'粒子家族前缀／固定语料','find_direct_full_corpus':'粒子定义直接查找'}
    identity_rows=[[wanted[r['case']],f"{float(r['old_median_ns']):.2f}",f"{float(r['new_median_ns']):.2f}"]for r in particle if r['case']in wanted]
    reanim=read_csv(base/'reanim_identity_benchmark/run-1/samples.csv')
    values={k:min(float(r['ns_per_query'])for r in reanim if r['kernel']==k)for k in {r['kernel']for r in reanim}}
    identity_rows.append(['reanim：当前ID扫描／MPHF',f"{values['current_id_scan_model']:.2f}",f"{values['production_mphf_api']:.2f}"])
    tables['identity']=dict(columns=['身份查询','扫描模型','枚举／区间／MPHF'],widths=[900,410,410],rows=identity_rows,
        note='ns/查询。粒子是固定112系统/222定义语料；reanim穷举2641条合法查询，未恢复原字符串路径。')
    frozen=json.loads((base/'frozen-draw-summary.json').read_text())
    tables['gpu']=dict(columns=['指标','原机制／历史1.0','当前'],widths=[900,410,410],rows=[
        ['固定7687三角形夹具draw数','1989','34'],['动态顶点字节／矩形','240','160'],
        ['固定CPU Draw中位数（µs）','2303.925',f"{frozen['results'][0]['median_cpu_us']:.3f}"],
        ['固定CPU Draw p90（µs）','2453.100',f"{frozen['results'][0]['p90_nearest_rank_us']:.3f}"]],
        note='前两项是结构计数，不能换算成GPU提速倍率。后两项DX12 800×600、32植物40僵尸；不含flush/fence/readback。')
    roots=read_csv(base/'reanim_frame_cache_benchmark/run-1/samples.csv')
    root=lambda k:min(float(r['ns_per_operation'])for r in roots if r['kernel']==k and r['definition']=='normal')
    trig={m[1].strip():float(m[2])for line in (base/'trig/run-1/samples.csv').read_text().splitlines()
          if (m:=re.match(r'^(.*?)\s+median\s+([\d.]+) ns/eval',line))}
    tables['time']=dict(columns=['局部工作','参考机制','v3.2.4'],widths=[900,410,410],rows=[
        ['Root查询（ns）','范围不同，不作原版倍率',f'{root("v4_root"):.2f}'],
        ['推进＋Root（ns）','与查询分开计时',f'{root("v4_update_root"):.2f}'],
        ['四个FastSin/FastCos调用（ns）',f"{trig['matrix fast (4)']:.2f}",f"{trig['matrix shared reduction']:.2f}"],
        ['帧号／根运动独立对拍','逐项验证','见validation.csv']],
        note='ns/操作。三角函数同输入真实FastMath翻译单元；根运动是完整生产API，不与冻结算术模型混算。')
    project=json.loads(args.project_endless.read_text());retail=json.loads((args.project_endless.parents[1]/'original-1073-11x-full-20261029/summary.json').read_text())
    tables['endless']=dict(columns=['完整无尽观测','原版1073','本项目240Hz上限'],widths=[900,410,410],rows=[
        ['平均绘制FPS',f"{retail['fps']:.2f}",f"{project['fps']:.2f}"],
        ['平均逻辑更新Hz',f"{retail['update_hz']:.2f}",f"{project['update_hz']:.2f}"],
        ['进程CPU占用／单核100%',f"{retail['cpu_percent']:.2f}%",f"{project['cpu_percent']:.2f}%"],
        ['CPU ms／绘制帧',f"{retail['cpu_ms_per_frame']:.3f}",f"{project['cpu_ms_per_frame']:.3f}"]],
        note='LP14、用户11x；同源基准0→20波，时长不同。更新步长、渲染器和轨迹不同，不是孤立算法倍率。')
    external=json.loads(args.external.read_text())
    tables['external']=dict(columns=['横向场景观测','精华版','本项目'],widths=[900,410,410],rows=[
        ['电脑平均FPS',f"{external['ultimate']['fps']:.2f}",f"{project['fps']:.2f}"],
        ['电脑CPU占用／单核100%',f"{external['ultimate']['cpu_percent']:.2f}%",f"{project['cpu_percent']:.2f}%"],
        ['手机平均整机功耗','2.657 W','2.438 W'],
        ['手机有效放电时间','335.29秒','380.01秒']],
        note='电脑：精华版100Hz逻辑/解帧，RTSS呈现计数；本项目游戏绘制计数。手机未测FPS，温度不同。非同一条战斗轨迹。')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(tables,ensure_ascii=False,indent=2),encoding='utf-8')


if __name__=='__main__':main()
