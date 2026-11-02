"""Read-only PE32 recursive-descent ISA census; no game execution, no linear sweep.
Run: python analyze.py [path-to-retail-exe] > report.json
Statistics describe statically reached code, not execution frequency.
"""
import collections, hashlib, json, pathlib, struct, sys
import capstone, pefile
from capstone.x86_const import X86_OP_IMM, X86_GRP_FPU
exe=pathlib.Path(sys.argv[1] if len(sys.argv)>1 else r'C:/UserProgram/Plants_Vs_Zombies_V1.2.0.1073_EN/PlantsVsZombies.exe')
raw=exe.read_bytes(); pe=pefile.PE(data=raw); base=pe.OPTIONAL_HEADER.ImageBase; data=pe.get_memory_mapped_image()
assert hashlib.sha256(raw).hexdigest()=='5f650f52d12751e08b63e590edb0d9b9f09d087cea900baa0eb39e4af8eda251'
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);md.detail=True
ranges=[(base+s.VirtualAddress,base+s.VirtualAddress+min(s.Misc_VirtualSize,s.SizeOfRawData)) for s in pe.sections if s.Characteristics&0x20000000]
def executable(a):return any(lo<=a<hi for lo,hi in ranges)
samples={'Zombie::UpdateZombiePosition':0x53b630,'Reanimation::Update':0x4761c0,'TodParticleSystem::Update':0x521a20,'TodParticleEmitter::Update':0x521c00,'TodParticleEmitter::UpdateParticle':0x5215b0,'FloatTrackEvaluate':0x447400,'TodCurveEvaluate':0x51bc10}
# Explicitly validated switch: CMP EAX,0xD / JA default / JMP [EAX*4+0x51BD58].
curve_targets=list(struct.unpack_from('<14I',data,0x51bd58-base))
def walk(seeds,follow_calls):
 todo=list(seeds); seen={}; occupied={}; indirect=[]; conflicts=[]
 while todo:
  a=todo.pop()
  while executable(a) and a not in seen:
   if a in occupied:conflicts.append(hex(a));break
   i=next(md.disasm(data[a-base:a-base+15],a,count=1),None)
   if i is None or i.mnemonic in ('int3','ud2'):break
   if any(a+j in occupied for j in range(i.size)):conflicts.append(hex(a));break
   seen[a]=i
   for j in range(i.size):occupied[a+j]=a
   nxt=a+i.size; imm=i.operands[0].imm if i.operands and i.operands[0].type==X86_OP_IMM else None
   if i.group(capstone.CS_GRP_CALL):
    if follow_calls and imm is not None and executable(imm):todo.append(imm)
   elif i.group(capstone.CS_GRP_JUMP):
    if imm is not None and executable(imm):todo.append(imm)
    else:
     indirect.append(hex(a))
     if a==0x51bc1c:todo.extend(curve_targets)
    if i.mnemonic=='jmp':break
   if i.group(capstone.CS_GRP_RET) or i.mnemonic in ('iret','iretd','hlt'):break
   a=nxt
 return seen,indirect,conflicts
arith={'fadd','faddp','fiadd','fsub','fsubp','fsubr','fsubrp','fisub','fisubr','fmul','fmulp','fimul','fdiv','fdivp','fdivr','fdivrp','fidiv','fidivr','fsqrt','fsin','fcos','fsincos','fptan','fpatan','fyl2x','fyl2xp1','f2xm1','fscale','fprem','fprem1','frndint','fabs','fchs'}
sse_arith={a+s for a in ('add','sub','mul','div','min','max','sqrt','rcp','rsqrt','hadd','hsub','addsub') for s in ('ss','sd','ps','pd')}|{'dpps','dppd'}
def summarize(ins):
 c=collections.Counter(); x87=collections.Counter(); simd=collections.Counter(); examples={}
 for a,i in sorted(ins.items()):
  m=i.mnemonic
  if X86_GRP_FPU in i.groups:c['x87_all']+=1;x87[m]+=1
  if m in arith:c['x87_arithmetic']+=1
  xmm='xmm' in i.op_str
  if xmm:
   simd[m]+=1;c['xmm_any']+=1
   k='sse_fp_arithmetic' if m in sse_arith else ('sse_fp_compare' if (m in {'comiss','ucomiss','comisd','ucomisd'} or (m.startswith('cmp') and m.endswith(('ss','sd','ps','pd')))) else 'xmm_other')
   c[k]+=1
   if len(examples.setdefault(k,[]))<12:examples[k].append(f'{a:08X}: {m} {i.op_str}')
 c['instructions']=len(ins)
 return {'sse_arithmetic_addresses':[hex(a) for a,i in sorted(ins.items()) if i.mnemonic in sse_arith], 'counts':dict(c),'x87_mnemonics':dict(x87),'xmm_mnemonics':dict(simd),'examples':examples}
ins,indirect,conflicts=walk([base+pe.OPTIONAL_HEADER.AddressOfEntryPoint,*samples.values()],True)
result={'sha256':hashlib.sha256(raw).hexdigest(),'image_base':hex(base),'entry':hex(base+pe.OPTIONAL_HEADER.AddressOfEntryPoint),'method':'recursive direct-control-flow descent, follows direct calls, no linear sweep; one validated curve jump table expanded','global_static_reachable':summarize(ins),'unresolved_indirect_jump_sites':len(set(indirect)),'overlap_conflicts':len(conflicts),'curve_jump_table':[hex(x) for x in curve_targets],'samples':{}}
for name,a in samples.items():
 ss,ii,cc=walk([a],False); summary=summarize(ss)
 # A compact representative excerpt only, omitting most instruction bytes.
 fp=[f'{x:08X}: {i.mnemonic} {i.op_str}' for x,i in sorted(ss.items()) if i.mnemonic in arith or i.mnemonic in ('fld','fstp','fild')]
 summary.update({'entry':hex(a),'fp_excerpt':fp[:12],'unresolved_indirect_jumps':ii})
 result['samples'][name]=summary
print(json.dumps(result,indent=2))

