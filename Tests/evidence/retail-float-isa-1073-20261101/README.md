# Retail GOTY 1.2.0.1073 floating-point ISA evidence (2026-11-01)

Conclusion: the sampled gameplay paths use x87 for floating-point arithmetic. A conservative recursive code census also contains substantially more x87 arithmetic than SSE floating-point arithmetic. The EXE does contain real SSE2 arithmetic, so “no SSE” would be false. These are static instruction counts, not runtime instruction frequencies or a benchmark.

Input: `C:/UserProgram/Plants_Vs_Zombies_V1.2.0.1073_EN/PlantsVsZombies.exe`, SHA-256 `5f650f52d12751e08b63e590edb0d9b9f09d087cea900baa0eb39e4af8eda251`, PE32 x86, image base `0x400000`. Addresses below are preferred-image virtual addresses.

## Verified business paths

Names are matched by control flow and structure, using source comments only as leads. In particular, historical 1051 addresses were not treated as 1073 addresses.

| Path | Actual entry | Reached instructions* | x87 all | x87 arithmetic | SSE FP arithmetic |
|---|---:|---:|---:|---:|---:|
| Zombie::UpdateZombiePosition | 0x53B630 | 105 | 23 | 7 | 0 |
| Reanimation::Update | 0x4761C0 | 227 | 66 | 11 | 0 |
| TodParticleSystem::Update | 0x521A20 | 43 | 0 | 0 | 0 |
| TodParticleEmitter::Update | 0x521C00 | 112 | 7 | 1 | 0 |
| TodParticleEmitter::UpdateParticle | 0x5215B0 | 202 | 75 | 13 | 0 |
| FloatTrackEvaluate | 0x447400 | 94 | 37 | 3 | 0 |
| TodCurveEvaluate | 0x51BC10 | 97 | 58 | 13 | 0 |

*Recursive local control flow, without descending calls; direct tail jumps are followed. Curve switch targets are explicitly expanded. No XMM instructions were found in these local traversals. “x87 all” includes load/store/compare/control as well as arithmetic; arithmetic is counted separately.

Zombie movement updates its float position at `+0x2C`:

```asm
0053B68D fld  dword ptr [esi + 0x2c]
0053B690 fadd qword ptr [0x6fdb68]
0053B696 fstp dword ptr [esi + 0x2c]
```

Reanimation checks frame count at `+0x1C` and dead flag at `+0x14`, then advances animation time `+4` from animation rate `+8` and frame count. Its actual arithmetic is:

```asm
004761F2 fld   dword ptr [esi + 8]
004761F5 fmul  qword ptr [0x6fdcd8]
004761FB fidiv dword ptr [esp + 0x14]
004761FF fadd  dword ptr [esi + 4]
00476202 fstp  dword ptr [esp + 0x14]
```

Particle system calls emitter update at `0x521A52 -> 0x521C00`; the emitter walks particle IDs (160-byte slots) and calls particle update at `0x521CF1 -> 0x5215B0`. Particle age normalization and position integration use:

```asm
00521614 fild  dword ptr [edi]
0052161B fidiv dword ptr [esp + 0x10]
00521621 fstp  dword ptr [esi + 0xc]
0052165B fld   dword ptr [esi + 0x20]
00521660 fadd  dword ptr [esi + 0x18]
0052166C fstp  dword ptr [esi + 0x20]
0052166F fld   dword ptr [esi + 0x1c]
00521672 fadd  dword ptr [esi + 0x24]
00521675 fstp  dword ptr [esi + 0x24]
```

Particle update calls FloatTrackEvaluate at `0x521685 -> 0x447400`. This walks 20-byte nodes and calls the curve evaluator at `0x447479`, `0x4474A9`, `0x4474CD`, and `0x4474F3 -> 0x51BC10`. Its time fraction uses `0x4474E0 FSUB [edi]`, `0x4474E4 FSUB [edi]`, and `0x4474E6 FDIVP ST(1)`.

Curve evaluator has a bounds-checked 14-entry switch: `0x51BC13 CMP EAX,0xD`, `0x51BC16 JA 0x51BD35`, `0x51BC1C JMP [EAX*4+0x51BD58]`. The table at `0x51BD58` is data and is never decoded as instructions. The quadratic branch has `0x51BC37 FMUL ST(0),ST(0)`; final interpolation is:

```asm
0051BD35 fld    dword ptr [esp + 0x14]
0051BD39 fld    dword ptr [esp + 0x10]
0051BD3D fld    st(0)
0051BD3F fsubp  st(2)
0051BD41 fld    dword ptr [esp + 4]
0051BD45 fmulp  st(2)
0051BD47 faddp  st(1)
```

## Conservative static census

`analyze.py` reads the PE and uses Capstone recursive descent starting at the PE entry point `0x69CC9A` and the business entries above. It follows direct calls, conditional edges and direct jumps. It stops at returns, unknown indirect jumps and traps. The one validated curve jump table is expanded. It never performs a linear section sweep, and reports zero overlapping-instruction conflicts.

The resulting 73,293 distinct reached instructions contain:

- 3,810 x87 instructions, of which 697 are arithmetic (loads/stores/comparisons excluded).
- 93 SSE floating-point arithmetic instructions, 6 SSE floating-point comparisons.
- 210 instructions mentioning XMM registers overall. The remaining 111 include moves, conversions, integer SIMD and bit manipulation; these are not counted as FP arithmetic.

Among the counted arithmetic instructions, 697/(697+93) = 88.2% are x87. This percentage characterizes this reached static subset only. All 93 reached SSE arithmetic instructions lie between `0x69EE3B` and `0x6AE14C`, outside the sampled gameplay routines. The region appears to contain runtime/math support, but exact symbol attribution was not established. Actual examples: `0x69EE3B ADDSD XMM1,XMM0`, `0x6AD02D MULSD XMM1,XMM0`, `0x6AD07D MULPD XMM2,XMM1`. Thus the SSE observations are not merely memcpy instructions. Conversely, `0x69ED90 MOVQ XMM0,[ESP+4]`, `0x69EDA6 PSRLQ XMM0,0x34`, and `0x69EDB7 PSUBD XMM2,XMM0` do not establish gameplay FP arithmetic and are excluded from its arithmetic count.

Limitations: this is not a complete code discovery pass. There are 29 unique reached indirect-jump sites (including the explicitly handled curve dispatch); other switch tables, virtual calls, callbacks, dynamically loaded DLLs and code reachable only through them can be missed. Direct-call fallthrough conservatively assumes a call can return. No game was launched, no profiler was attached, and no runtime-hotness claim is made. Instruction evidence establishes x87 arithmetic and float32 memory stores; it does not by itself prove the runtime x87 control-word precision or all intermediate rounding behavior.

Reproduce with Python plus `pefile` and `capstone`: `python analyze.py <exe> > report.json`. `report.json` contains compact counts and representative instructions, not a full binary dump. No production source was changed and no build or Git commit was performed.
