# 原始拼图布局调查

保留2026-10-23源素材盘点中的布局、单格尺寸和当时调用依据，供追溯原始sheet使用；计数不是当前发行资源数量。原版参考来自本地pvz-decomp-ruslan资源清单，并非逐项零售EXE验证。

维护规则：普通场景保留完整sheet，局部使用最终UV；既有粒子无损拆帧不在此规则中回退。当前接口和装箱规则以[场景DSL](../scene-atlas-dsl.md)与[资源格式](../resource-format.md)为准。本页不再保留已否决的拆图建议及实现计划。

## 历史审计基线中的帧图/图标集合

| 图片（省略 IMAGE_） | 审计布局与单格尺寸 | 当时调用依据 |
|---|---|---|
| FOG | 8×1，210×190 | Board::DrawFog；CreditScreen |
| WAVECENTER、WAVESIDE | 各 1×12，160×27 | Board::DrawUIBottom |
| CHALLENGE_THUMBNAILS | 22×1，80×65 | ChallengeScreen::Draw |
| SURVIVAL_THUMBNAILS | 11×1，80×65 | ChallengeScreen::Draw |
| SEEDS | 9×1，50×70 | SeedPacketDrawBackground 等 |
| FLAGMETER | 1×2，158×27 | Board 7906–7912 |
| FLAGMETERPARTS | 3×1，25×25 | Board 7970–7992 |
| SELECTORSCREEN_LEVELNUMBERS | 10×1，12×17 | GameSelector 834–842 |
| TOMBSTONES、TOMBSTONE_MOUNDS | 各 5×4，86×91 | GridItem::DrawGraveStone |
| CRATER、CRATER_FADING | 各 2×1，90×61 | GridItem::DrawCrater；Challenge |
| CRATER_ROOF_CENTER | 2×1，75×77 | GridItem::DrawCrater |
| CRATER_ROOF_LEFT | 2×1，75×87 | GridItem::DrawCrater |
| CRATER_WATER_DAY、CRATER_WATER_NIGHT | 各 2×1，85×53 | GridItem::DrawCrater |
| SUNFLOWER_TROPHY | 2×1，157×269 | Coin、AwardScreen、GameSelector |
| SCARY_POT | 3×2，80×101 | GridItem::DrawScaryPot；Coin |
| PACKET_PLANTS | 13×1，50×70 | SeedPacketDrawSeed |
| ZEN_NEED_ICONS | 3×1，30×30 | ZenGarden 2418 |
| CONVEYORBELT | 1×6，502×16 | SeedPacket 967 |
| ZOMBIEPOGO | 3×1，44×86 | Plant::DrawMagnetItems |
| ACHEESEMENTS_ICONS | 512×256，按 70×70、每行 7 格取图；当前使用 20 图标 | AchievementsWidget 137；AwardScreen 636 |


## 此轮决定前已有的拆分与重组

粒子原有的爆米花、大小泥土、碎石、豌豆碎屑/溅射、雪豌豆碎屑/
溅射/烟团、星星碎屑/溅射、雪花、土豆雷、瓜类碎屑、花瓶碎片、
坚果碎屑、各类 Boss 冰火碎屑等，已经是源级无损拆帧和 definition atlas。
完整 36 组见 `particle-frame-images.json`，无需再从发行图集切一次。
这些既有粒子输入及其历史证据保持原口径；新场景图集不继续对原有 sheet
实施这一拆分方式，也不为逻辑 view 新建拆帧源 PNG。

`assets/definitions/projectiles/Projectiles.projectile.dsl` 中的 day/night
阴影矩形分别为 `[0,0,21,9]` 和 `[21,0,21,9]`。
当前运行时使用离线生成的UV。

字体作者文件的 `RectList` / `LayerSetImageMap` 已描述字形区域。
此轮不拆字形页；字距、基线、图层共享和渲染批次需独立评估。

## 不能混同于帧图的切片

- `EDITBOX`：90×42，`LawnCommon:108 → Graphics::DrawImageBox`，
  按 3×3 切为 30×14 的九宫格。若后续合图，只能整张 sheet 装箱，
  绘制沿用局部矩形，不再采用九块分别装箱的旧建议。
- `OPTIONS_SLIDERSLOT`：135×10，Slider 按左右端和中段切成三段 45×10。
  合图后需保留中段重复及尾段局部裁剪。
- `SEEDBANK`：完整绘制后复用右尾扩展槽位，不是独立动画帧。
- Almanac 三张 1400×600 背景：当前宽屏逻辑保留左右 16px 边缘，
  伸展中间；这是完整背景的布局切片。实际 Almanac DSL 保留整张背景，
  装箱只改变整张源图的 origin，宽屏绘制继续使用其局部像素区域。
- ICE、草皮、草坪背景按长度/揭露进度裁剪，不应切成固定格子。
- Pool caustic compute 输入/输出、自定义水面 UV 与 wrap 语义保持独立，
  不能按普通 UI sprite 直接机械合图。
