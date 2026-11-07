#include "Sexy.TodLib/generated/EntityStrings.h"  // 动态键实体表（阶段 3）
#include "Sexy.TodLib/generated/ReanimVariantOptions.h"
#include "GameButton.h"
#include "CreditScreen.h"
#include "CreditsScroll.h"
#include "LawnApp.h"
#include "graphics/DDInterface.h"
#include "Resources.h"
#include "Lawn/System/Music.h"
#include "GameConstants.h"
#include "Lawn/System/PoolEffect.h"
#include "Sexy.TodLib/TodFoley.h"
#include "Sexy.TodLib/Attachment.h"
#include "Sexy.TodLib/Reanimator.h"
#include "Sexy.TodLib/TodParticle.h"
#include "widget/Dialog.h"
#include "graphics/Image.h"
#include "misc/SexyMatrix.h"
#include "widget/WidgetLifetimeHold.h"
#include "Sexy.TodLib/EffectSystem.h"
#include "Sexy.TodLib/TodStringFile.h"
#include <algorithm>
#include <cmath>

static CreditsTiming gCreditsTiming[] = {  //0x6A1AD8
    {  128.5f,      CreditWordType::WORD_AW ,         0,     CreditBrainType::BRAIN_OFF       },
    {  133.0f,      CreditWordType::WORD_OH ,         0,     CreditBrainType::BRAIN_OFF       },
    {  136.5f,      CreditWordType::WORD_EE ,         0,     CreditBrainType::BRAIN_OFF       },
    {  140.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_OFF       },
    {  141.0f,      CreditWordType::WORD_AW ,       214,     CreditBrainType::BRAIN_NEXT_WORD },
    {  143.0f,      CreditWordType::WORD_AW ,       297,     CreditBrainType::BRAIN_NEXT_WORD },
    {  145.0f,      CreditWordType::WORD_AW ,       348,     CreditBrainType::BRAIN_NEXT_WORD },
    {  149.0f,      CreditWordType::WORD_EE ,       400,     CreditBrainType::BRAIN_NEXT_WORD },
    {  153.0f,      CreditWordType::WORD_AW ,       455,     CreditBrainType::BRAIN_NEXT_WORD },
    {  155.0f,      CreditWordType::WORD_OH ,       523,     CreditBrainType::BRAIN_NEXT_WORD },
    {  159.0f,      CreditWordType::WORD_AW ,       593,     CreditBrainType::BRAIN_NEXT_WORD },
    {  163.0f,      CreditWordType::WORD_AW ,       619,     CreditBrainType::BRAIN_NEXT_WORD },
    {  171.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_FLY_OFF   },
    {  172.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_OFF       },
    {  173.0f,      CreditWordType::WORD_AW ,       214,     CreditBrainType::BRAIN_FAST_ON   },
    {  175.0f,      CreditWordType::WORD_AW ,       297,     CreditBrainType::BRAIN_NEXT_WORD },
    {  177.0f,      CreditWordType::WORD_AW ,       348,     CreditBrainType::BRAIN_NEXT_WORD },
    {  181.0f,      CreditWordType::WORD_EE ,       400,     CreditBrainType::BRAIN_NEXT_WORD },
    {  185.0f,      CreditWordType::WORD_AW ,       455,     CreditBrainType::BRAIN_NEXT_WORD },
    {  187.0f,      CreditWordType::WORD_OH ,       523,     CreditBrainType::BRAIN_NEXT_WORD },
    {  191.0f,      CreditWordType::WORD_AW ,       593,     CreditBrainType::BRAIN_NEXT_WORD },
    {  193.0f,      CreditWordType::WORD_AW ,       619,     CreditBrainType::BRAIN_NEXT_WORD },
    {  199.0f,      CreditWordType::WORD_AA ,         0,     CreditBrainType::BRAIN_FLY_OFF   },
    {  203.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_OFF       },
    {  205.0f,      CreditWordType::WORD_AW ,       214,     CreditBrainType::BRAIN_FLY_ON    },
    {  207.0f,      CreditWordType::WORD_AW ,       297,     CreditBrainType::BRAIN_NEXT_WORD },
    {  209.0f,      CreditWordType::WORD_AW ,       348,     CreditBrainType::BRAIN_NEXT_WORD },
    {  213.0f,      CreditWordType::WORD_EE ,       400,     CreditBrainType::BRAIN_NEXT_WORD },
    {  217.0f,      CreditWordType::WORD_AW ,       455,     CreditBrainType::BRAIN_NEXT_WORD },
    {  219.0f,      CreditWordType::WORD_OH ,       523,     CreditBrainType::BRAIN_NEXT_WORD },
    {  223.0f,      CreditWordType::WORD_AW ,       593,     CreditBrainType::BRAIN_NEXT_WORD },
    {  227.0f,      CreditWordType::WORD_AW ,       619,     CreditBrainType::BRAIN_NEXT_WORD },
    {  231.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_FLY_OFF   },
    {  234.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_OFF       },
    {  235.0f,      CreditWordType::WORD_EE ,       150,     CreditBrainType::BRAIN_FAST_ON   },
    {  237.0f,      CreditWordType::WORD_OH ,       220,     CreditBrainType::BRAIN_NEXT_WORD },
    {  239.0f,      CreditWordType::WORD_AW ,       307,     CreditBrainType::BRAIN_NEXT_WORD },
    {  241.0f,      CreditWordType::WORD_AW ,       390,     CreditBrainType::BRAIN_NEXT_WORD },
    {  245.0f,      CreditWordType::WORD_EE ,       452,     CreditBrainType::BRAIN_NEXT_WORD },
    {  249.0f,      CreditWordType::WORD_AW ,       512,     CreditBrainType::BRAIN_NEXT_WORD },
    {  251.0f,      CreditWordType::WORD_AW ,       573,     CreditBrainType::BRAIN_NEXT_WORD },
    {  255.0f,      CreditWordType::WORD_AW ,       630,     CreditBrainType::BRAIN_NEXT_WORD },
    {  257.0f,      CreditWordType::WORD_AW ,       656,     CreditBrainType::BRAIN_NEXT_WORD },
    {  261.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_FLY_OFF   },
    {  262.0f,      CreditWordType::WORD_AA ,         0,     CreditBrainType::BRAIN_OFF       },
    {  266.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_OFF       },
    {  266.5f,      CreditWordType::WORD_AW ,        96,     CreditBrainType::BRAIN_FAST_ON   },
    {  268.5f,      CreditWordType::WORD_OH ,       154,     CreditBrainType::BRAIN_NEXT_WORD },
    {  270.5f,      CreditWordType::WORD_OH ,       244,     CreditBrainType::BRAIN_NEXT_WORD },
    {  272.5f,      CreditWordType::WORD_AW ,       329,     CreditBrainType::BRAIN_NEXT_WORD },
    {  276.5f,      CreditWordType::WORD_AW ,       419,     CreditBrainType::BRAIN_NEXT_WORD },
    {  279.5f,      CreditWordType::WORD_AW ,       506,     CreditBrainType::BRAIN_NEXT_WORD },
    {  281.5f,      CreditWordType::WORD_AW ,       597,     CreditBrainType::BRAIN_NEXT_WORD },
    {  284.5f,      CreditWordType::WORD_AW ,       671,     CreditBrainType::BRAIN_NEXT_WORD },
    {  286.5f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_FLY_OFF   },
    {  287.0f,      CreditWordType::WORD_OH ,        48,     CreditBrainType::BRAIN_FAST_ON   },
    {  288.0f,      CreditWordType::WORD_AW ,       125,     CreditBrainType::BRAIN_NEXT_WORD },
    {  290.0f,      CreditWordType::WORD_OH ,       193,     CreditBrainType::BRAIN_NEXT_WORD },
    {  291.0f,      CreditWordType::WORD_EE ,       254,     CreditBrainType::BRAIN_NEXT_WORD },
    {  294.5f,      CreditWordType::WORD_AW ,       318,     CreditBrainType::BRAIN_NEXT_WORD },
    {  295.0f,      CreditWordType::WORD_AW ,       375,     CreditBrainType::BRAIN_NEXT_WORD },
    {  296.0f,      CreditWordType::WORD_AW ,       438,     CreditBrainType::BRAIN_NEXT_WORD },
    {  297.0f,      CreditWordType::WORD_AW ,       480,     CreditBrainType::BRAIN_NEXT_WORD },
    {  299.0f,      CreditWordType::WORD_AW ,       556,     CreditBrainType::BRAIN_NEXT_WORD },
    {  301.0f,      CreditWordType::WORD_AW ,       619,     CreditBrainType::BRAIN_NEXT_WORD },
    {  303.0f,      CreditWordType::WORD_AW ,       675,     CreditBrainType::BRAIN_NEXT_WORD },
    {  305.0f,      CreditWordType::WORD_AW ,       744,     CreditBrainType::BRAIN_NEXT_WORD },
    {  307.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_FLY_OFF   },
    {  309.5f,      CreditWordType::WORD_OFF,       207,     CreditBrainType::BRAIN_FLY_ON    },
    {  310.5f,      CreditWordType::WORD_OFF,       287,     CreditBrainType::BRAIN_NEXT_WORD },
    {  311.5f,      CreditWordType::WORD_OFF,       365,     CreditBrainType::BRAIN_NEXT_WORD },
    {  313.5f,      CreditWordType::WORD_OFF,       435,     CreditBrainType::BRAIN_NEXT_WORD },
    {  315.5f,      CreditWordType::WORD_OFF,       518,     CreditBrainType::BRAIN_NEXT_WORD },
    {  317.5f,      CreditWordType::WORD_OFF,       603,     CreditBrainType::BRAIN_NEXT_WORD },
    {  318.5f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_FAST_OFF  },
    {  319.5f,      CreditWordType::WORD_OFF,       198,     CreditBrainType::BRAIN_FAST_ON   },
    {  320.5f,      CreditWordType::WORD_OFF,       264,     CreditBrainType::BRAIN_NEXT_WORD },
    {  322.5f,      CreditWordType::WORD_OFF,       335,     CreditBrainType::BRAIN_NEXT_WORD },
    {  323.5f,      CreditWordType::WORD_OFF,       411,     CreditBrainType::BRAIN_NEXT_WORD },
    {  324.5f,      CreditWordType::WORD_OFF,       474,     CreditBrainType::BRAIN_NEXT_WORD },
    {  326.5f,      CreditWordType::WORD_OFF,       527,     CreditBrainType::BRAIN_NEXT_WORD },
    {  328.5f,      CreditWordType::WORD_OFF,       595,     CreditBrainType::BRAIN_NEXT_WORD },
    {  332.5f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_FLY_OFF   },
    {  337.5f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_OFF       },
    {  339.5f,      CreditWordType::WORD_AW ,       190,     CreditBrainType::BRAIN_FLY_ON    },
    {  340.5f,      CreditWordType::WORD_AW ,       260,     CreditBrainType::BRAIN_NEXT_WORD },
    {  342.5f,      CreditWordType::WORD_AA ,       314,     CreditBrainType::BRAIN_NEXT_WORD },
    {  344.5f,      CreditWordType::WORD_AW ,       364,     CreditBrainType::BRAIN_NEXT_WORD },
    {  347.5f,      CreditWordType::WORD_AW ,       426,     CreditBrainType::BRAIN_NEXT_WORD },
    {  349.5f,      CreditWordType::WORD_OH ,       474,     CreditBrainType::BRAIN_NEXT_WORD },
    {  350.5f,      CreditWordType::WORD_AW ,       538,     CreditBrainType::BRAIN_NEXT_WORD },
    {  352.5f,      CreditWordType::WORD_EE ,       606,     CreditBrainType::BRAIN_NEXT_WORD },
    {  353.5f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_FAST_OFF  },
    {  354.5f,      CreditWordType::WORD_EE ,       187,     CreditBrainType::BRAIN_FAST_ON   },
    {  356.5f,      CreditWordType::WORD_AW ,       242,     CreditBrainType::BRAIN_NEXT_WORD },
    {  358.5f,      CreditWordType::WORD_OH ,       280,     CreditBrainType::BRAIN_NEXT_WORD },
    {  359.5f,      CreditWordType::WORD_AW ,       340,     CreditBrainType::BRAIN_NEXT_WORD },
    {  360.5f,      CreditWordType::WORD_AW ,       394,     CreditBrainType::BRAIN_NEXT_WORD },
    {  361.5f,      CreditWordType::WORD_EE ,       439,     CreditBrainType::BRAIN_NEXT_WORD },
    {  363.5f,      CreditWordType::WORD_AW ,       500,     CreditBrainType::BRAIN_NEXT_WORD },
    {  364.5f,      CreditWordType::WORD_AW ,       550,     CreditBrainType::BRAIN_NEXT_WORD },
    {  366.5f,      CreditWordType::WORD_EE ,       606,     CreditBrainType::BRAIN_NEXT_WORD },
    {  369.5f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_FLY_OFF   },
    {  371.5f,      CreditWordType::WORD_OFF,       200,     CreditBrainType::BRAIN_FLY_ON    },
    {  372.5f,      CreditWordType::WORD_OH ,       258,     CreditBrainType::BRAIN_NEXT_WORD },
    {  374.5f,      CreditWordType::WORD_OFF,       332,     CreditBrainType::BRAIN_NEXT_WORD },
    {  376.5f,      CreditWordType::WORD_OFF,       416,     CreditBrainType::BRAIN_NEXT_WORD },
    {  378.5f,      CreditWordType::WORD_OFF,       494,     CreditBrainType::BRAIN_NEXT_WORD },
    {  380.5f,      CreditWordType::WORD_OFF,       576,     CreditBrainType::BRAIN_NEXT_WORD },
    {  381.5f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_FAST_OFF  },
    {  382.5f,      CreditWordType::WORD_OFF,       255,     CreditBrainType::BRAIN_FAST_ON   },
    {  384.5f,      CreditWordType::WORD_OFF,       322,     CreditBrainType::BRAIN_NEXT_WORD },
    {  386.5f,      CreditWordType::WORD_OFF,       400,     CreditBrainType::BRAIN_NEXT_WORD },
    {  388.5f,      CreditWordType::WORD_OFF,       474,     CreditBrainType::BRAIN_NEXT_WORD },
    {  390.5f,      CreditWordType::WORD_OFF,       533,     CreditBrainType::BRAIN_NEXT_WORD },
    {  394.5f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_FLY_OFF   },
    {  522.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_OFF       },
    {  523.0f,      CreditWordType::WORD_AW ,       214,     CreditBrainType::BRAIN_FAST_ON   },
    {  525.0f,      CreditWordType::WORD_AW ,       297,     CreditBrainType::BRAIN_NEXT_WORD },
    {  527.0f,      CreditWordType::WORD_AW ,       348,     CreditBrainType::BRAIN_NEXT_WORD },
    {  531.0f,      CreditWordType::WORD_EE ,       400,     CreditBrainType::BRAIN_NEXT_WORD },
    {  535.0f,      CreditWordType::WORD_AW ,       455,     CreditBrainType::BRAIN_NEXT_WORD },
    {  537.0f,      CreditWordType::WORD_OH ,       523,     CreditBrainType::BRAIN_NEXT_WORD },
    {  541.0f,      CreditWordType::WORD_AW ,       593,     CreditBrainType::BRAIN_NEXT_WORD },
    {  545.0f,      CreditWordType::WORD_AW ,       619,     CreditBrainType::BRAIN_NEXT_WORD },
    {  549.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_FLY_OFF   },
    {  554.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_OFF       },
    {  555.0f,      CreditWordType::WORD_AW ,       214,     CreditBrainType::BRAIN_FAST_ON   },
    {  557.0f,      CreditWordType::WORD_AW ,       297,     CreditBrainType::BRAIN_NEXT_WORD },
    {  559.0f,      CreditWordType::WORD_AW ,       348,     CreditBrainType::BRAIN_NEXT_WORD },
    {  563.0f,      CreditWordType::WORD_EE ,       400,     CreditBrainType::BRAIN_NEXT_WORD },
    {  567.0f,      CreditWordType::WORD_AW ,       455,     CreditBrainType::BRAIN_NEXT_WORD },
    {  569.0f,      CreditWordType::WORD_OH ,       523,     CreditBrainType::BRAIN_NEXT_WORD },
    {  573.0f,      CreditWordType::WORD_AW ,       593,     CreditBrainType::BRAIN_NEXT_WORD },
    {  575.0f,      CreditWordType::WORD_AW ,       619,     CreditBrainType::BRAIN_NEXT_WORD },
    {  581.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_FLY_OFF   },
    {  582.0f,      CreditWordType::WORD_AA ,         0,     CreditBrainType::BRAIN_OFF       },
    {  586.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_OFF       },
    {  587.0f,      CreditWordType::WORD_AW ,       214,     CreditBrainType::BRAIN_FAST_ON   },
    {  589.0f,      CreditWordType::WORD_AW ,       297,     CreditBrainType::BRAIN_NEXT_WORD },
    {  591.0f,      CreditWordType::WORD_AW ,       348,     CreditBrainType::BRAIN_NEXT_WORD },
    {  595.0f,      CreditWordType::WORD_EE ,       400,     CreditBrainType::BRAIN_NEXT_WORD },
    {  599.0f,      CreditWordType::WORD_AW ,       455,     CreditBrainType::BRAIN_NEXT_WORD },
    {  601.0f,      CreditWordType::WORD_OH ,       523,     CreditBrainType::BRAIN_NEXT_WORD },
    {  605.0f,      CreditWordType::WORD_AW ,       593,     CreditBrainType::BRAIN_NEXT_WORD },
    {  609.0f,      CreditWordType::WORD_AW ,       619,     CreditBrainType::BRAIN_NEXT_WORD },
    {  613.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_FLY_OFF   },
    {  616.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_OFF       },
    {  617.0f,      CreditWordType::WORD_EE ,       150,     CreditBrainType::BRAIN_FAST_ON   },
    {  619.0f,      CreditWordType::WORD_OH ,       220,     CreditBrainType::BRAIN_NEXT_WORD },
    {  621.0f,      CreditWordType::WORD_AW ,       307,     CreditBrainType::BRAIN_NEXT_WORD },
    {  623.0f,      CreditWordType::WORD_AW ,       390,     CreditBrainType::BRAIN_NEXT_WORD },
    {  627.0f,      CreditWordType::WORD_EE ,       452,     CreditBrainType::BRAIN_NEXT_WORD },
    {  631.0f,      CreditWordType::WORD_AW ,       512,     CreditBrainType::BRAIN_NEXT_WORD },
    {  633.0f,      CreditWordType::WORD_AW ,       573,     CreditBrainType::BRAIN_NEXT_WORD },
    {  637.0f,      CreditWordType::WORD_AW ,       630,     CreditBrainType::BRAIN_NEXT_WORD },
    {  639.0f,      CreditWordType::WORD_AW ,       656,     CreditBrainType::BRAIN_NEXT_WORD },
    {  643.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_FLY_OFF   },
    {  644.0f,      CreditWordType::WORD_AA ,         0,     CreditBrainType::BRAIN_OFF       },
    {  648.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_OFF       },
    {  649.0f,      CreditWordType::WORD_AA ,       196,     CreditBrainType::BRAIN_FAST_ON   },
    {  651.0f,      CreditWordType::WORD_EE ,       247,     CreditBrainType::BRAIN_NEXT_WORD },
    {  653.0f,      CreditWordType::WORD_AW ,       299,     CreditBrainType::BRAIN_NEXT_WORD },
    {  655.0f,      CreditWordType::WORD_AW ,       371,     CreditBrainType::BRAIN_NEXT_WORD },
    {  658.0f,      CreditWordType::WORD_OH ,       443,     CreditBrainType::BRAIN_NEXT_WORD },
    {  659.0f,      CreditWordType::WORD_EE ,       475,     CreditBrainType::BRAIN_NEXT_WORD },
    {  661.0f,      CreditWordType::WORD_AW ,       512,     CreditBrainType::BRAIN_NEXT_WORD },
    {  662.0f,      CreditWordType::WORD_AA ,       544,     CreditBrainType::BRAIN_NEXT_WORD },
    {  664.0f,      CreditWordType::WORD_OH ,       573,     CreditBrainType::BRAIN_NEXT_WORD },
    {  667.0f,      CreditWordType::WORD_AA ,       610,     CreditBrainType::BRAIN_NEXT_WORD },
    {  669.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_FAST_OFF  },
    {  670.0f,      CreditWordType::WORD_OFF,        48,     CreditBrainType::BRAIN_FAST_ON   },
    {  671.0f,      CreditWordType::WORD_OFF,       110,     CreditBrainType::BRAIN_NEXT_WORD },
    {  673.0f,      CreditWordType::WORD_OFF,       185,     CreditBrainType::BRAIN_NEXT_WORD },
    {  674.0f,      CreditWordType::WORD_OFF,       262,     CreditBrainType::BRAIN_NEXT_WORD },
    {  676.0f,      CreditWordType::WORD_OFF,       317,     CreditBrainType::BRAIN_NEXT_WORD },
    {  677.0f,      CreditWordType::WORD_OFF,       357,     CreditBrainType::BRAIN_NEXT_WORD },
    {  678.0f,      CreditWordType::WORD_OFF,       417,     CreditBrainType::BRAIN_NEXT_WORD },
    {  679.0f,      CreditWordType::WORD_OFF,       491,     CreditBrainType::BRAIN_NEXT_WORD },
    {  682.0f,      CreditWordType::WORD_OFF,       558,     CreditBrainType::BRAIN_NEXT_WORD },
    {  685.0f,      CreditWordType::WORD_OFF,       628,     CreditBrainType::BRAIN_NEXT_WORD },
    {  687.0f,      CreditWordType::WORD_OFF,       720,     CreditBrainType::BRAIN_NEXT_WORD },
    {  689.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_FLY_OFF   },
    {  690.0f,      CreditWordType::WORD_OFF,       172,     CreditBrainType::BRAIN_FAST_ON   },
    {  692.0f,      CreditWordType::WORD_OFF,       263,     CreditBrainType::BRAIN_NEXT_WORD },
    {  694.0f,      CreditWordType::WORD_OFF,       346,     CreditBrainType::BRAIN_NEXT_WORD },
    {  696.0f,      CreditWordType::WORD_OFF,       423,     CreditBrainType::BRAIN_NEXT_WORD },
    {  698.0f,      CreditWordType::WORD_OFF,       480,     CreditBrainType::BRAIN_NEXT_WORD },
    {  700.0f,      CreditWordType::WORD_OFF,       536,     CreditBrainType::BRAIN_NEXT_WORD },
    {  702.0f,      CreditWordType::WORD_OFF,       583,     CreditBrainType::BRAIN_NEXT_WORD },
    {  705.0f,      CreditWordType::WORD_OFF,       633,     CreditBrainType::BRAIN_NEXT_WORD },
    {  708.0f,      CreditWordType::WORD_OFF,       668,     CreditBrainType::BRAIN_NEXT_WORD },
    {  712.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_FLY_OFF   },
    {  719.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_OFF       },
    {  720.0f,      CreditWordType::WORD_OFF,       182,     CreditBrainType::BRAIN_FAST_ON   },
    {  722.0f,      CreditWordType::WORD_OFF,       267,     CreditBrainType::BRAIN_NEXT_WORD },
    {  724.0f,      CreditWordType::WORD_OFF,       331,     CreditBrainType::BRAIN_NEXT_WORD },
    {  726.0f,      CreditWordType::WORD_OFF,       371,     CreditBrainType::BRAIN_NEXT_WORD },
    {  729.0f,      CreditWordType::WORD_OFF,       434,     CreditBrainType::BRAIN_NEXT_WORD },
    {  731.0f,      CreditWordType::WORD_OFF,       486,     CreditBrainType::BRAIN_NEXT_WORD },
    {  732.0f,      CreditWordType::WORD_OFF,       562,     CreditBrainType::BRAIN_NEXT_WORD },
    {  734.0f,      CreditWordType::WORD_OFF,       617,     CreditBrainType::BRAIN_NEXT_WORD },
    {  735.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_FAST_OFF  },
    {  736.0f,      CreditWordType::WORD_AW ,       148,     CreditBrainType::BRAIN_FAST_ON   },
    {  738.0f,      CreditWordType::WORD_AW ,       211,     CreditBrainType::BRAIN_NEXT_WORD },
    {  740.0f,      CreditWordType::WORD_EE ,       298,     CreditBrainType::BRAIN_NEXT_WORD },
    {  742.0f,      CreditWordType::WORD_OH ,       367,     CreditBrainType::BRAIN_NEXT_WORD },
    {  744.0f,      CreditWordType::WORD_AW ,       440,     CreditBrainType::BRAIN_NEXT_WORD },
    {  746.0f,      CreditWordType::WORD_OH ,       506,     CreditBrainType::BRAIN_NEXT_WORD },
    {  747.0f,      CreditWordType::WORD_AW ,       533,     CreditBrainType::BRAIN_NEXT_WORD },
    {  748.0f,      CreditWordType::WORD_AW ,       601,     CreditBrainType::BRAIN_NEXT_WORD },
    {  749.0f,      CreditWordType::WORD_AW ,       645,     CreditBrainType::BRAIN_NEXT_WORD },
    {  750.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_FAST_OFF  },
    {  753.0f,      CreditWordType::WORD_OFF,       123,     CreditBrainType::BRAIN_FLY_ON    },
    {  755.0f,      CreditWordType::WORD_OFF,       195,     CreditBrainType::BRAIN_NEXT_WORD },
    {  757.0f,      CreditWordType::WORD_OFF,       255,     CreditBrainType::BRAIN_NEXT_WORD },
    {  759.0f,      CreditWordType::WORD_OFF,       312,     CreditBrainType::BRAIN_NEXT_WORD },
    {  761.0f,      CreditWordType::WORD_OFF,       378,     CreditBrainType::BRAIN_NEXT_WORD },
    {  763.0f,      CreditWordType::WORD_OFF,       443,     CreditBrainType::BRAIN_NEXT_WORD },
    {  765.0f,      CreditWordType::WORD_OFF,       516,     CreditBrainType::BRAIN_NEXT_WORD },
    {  767.0f,      CreditWordType::WORD_OFF,       563,     CreditBrainType::BRAIN_NEXT_WORD },
    {  770.0f,      CreditWordType::WORD_OFF,       588,     CreditBrainType::BRAIN_NEXT_WORD },
    {  773.0f,      CreditWordType::WORD_OFF,       657,     CreditBrainType::BRAIN_NEXT_WORD },
    {  777.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_FLY_OFF   },
    {  907.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_OFF       },
    {  908.0f,      CreditWordType::WORD_AW ,       214,     CreditBrainType::BRAIN_FAST_ON   },
    {  910.0f,      CreditWordType::WORD_AW ,       297,     CreditBrainType::BRAIN_NEXT_WORD },
    {  912.0f,      CreditWordType::WORD_AW ,       348,     CreditBrainType::BRAIN_NEXT_WORD },
    {  916.0f,      CreditWordType::WORD_EE ,       400,     CreditBrainType::BRAIN_NEXT_WORD },
    {  920.0f,      CreditWordType::WORD_AW ,       455,     CreditBrainType::BRAIN_NEXT_WORD },
    {  922.0f,      CreditWordType::WORD_OH ,       523,     CreditBrainType::BRAIN_NEXT_WORD },
    {  926.0f,      CreditWordType::WORD_AW ,       593,     CreditBrainType::BRAIN_NEXT_WORD },
    {  930.0f,      CreditWordType::WORD_AW ,       616,     CreditBrainType::BRAIN_NEXT_WORD },
    {  934.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_FLY_OFF   },
    {  939.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_OFF       },
    {  940.0f,      CreditWordType::WORD_AW ,       214,     CreditBrainType::BRAIN_FAST_ON   },
    {  942.0f,      CreditWordType::WORD_AW ,       297,     CreditBrainType::BRAIN_NEXT_WORD },
    {  944.0f,      CreditWordType::WORD_AW ,       348,     CreditBrainType::BRAIN_NEXT_WORD },
    {  948.0f,      CreditWordType::WORD_EE ,       400,     CreditBrainType::BRAIN_NEXT_WORD },
    {  952.0f,      CreditWordType::WORD_AW ,       455,     CreditBrainType::BRAIN_NEXT_WORD },
    {  954.0f,      CreditWordType::WORD_OH ,       523,     CreditBrainType::BRAIN_NEXT_WORD },
    {  958.0f,      CreditWordType::WORD_AW ,       593,     CreditBrainType::BRAIN_NEXT_WORD },
    {  960.0f,      CreditWordType::WORD_AW ,       616,     CreditBrainType::BRAIN_NEXT_WORD },
    {  966.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_FLY_OFF   },
    {  967.0f,      CreditWordType::WORD_AA ,         0,     CreditBrainType::BRAIN_OFF       },
    {  971.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_OFF       },
    {  972.0f,      CreditWordType::WORD_AW ,       214,     CreditBrainType::BRAIN_FAST_ON   },
    {  974.0f,      CreditWordType::WORD_AW ,       297,     CreditBrainType::BRAIN_NEXT_WORD },
    {  976.0f,      CreditWordType::WORD_AW ,       348,     CreditBrainType::BRAIN_NEXT_WORD },
    {  980.0f,      CreditWordType::WORD_EE ,       400,     CreditBrainType::BRAIN_NEXT_WORD },
    {  984.0f,      CreditWordType::WORD_AW ,       455,     CreditBrainType::BRAIN_NEXT_WORD },
    {  986.0f,      CreditWordType::WORD_OH ,       523,     CreditBrainType::BRAIN_NEXT_WORD },
    {  990.0f,      CreditWordType::WORD_AW ,       593,     CreditBrainType::BRAIN_NEXT_WORD },
    {  994.0f,      CreditWordType::WORD_AW ,       616,     CreditBrainType::BRAIN_NEXT_WORD },
    {  998.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_FLY_OFF   },
    { 1001.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_OFF       },
    { 1002.0f,      CreditWordType::WORD_EE ,       150,     CreditBrainType::BRAIN_FAST_ON   },
    { 1004.0f,      CreditWordType::WORD_OH ,       220,     CreditBrainType::BRAIN_NEXT_WORD },
    { 1006.0f,      CreditWordType::WORD_AW ,       307,     CreditBrainType::BRAIN_NEXT_WORD },
    { 1008.0f,      CreditWordType::WORD_AW ,       390,     CreditBrainType::BRAIN_NEXT_WORD },
    { 1012.0f,      CreditWordType::WORD_EE ,       452,     CreditBrainType::BRAIN_NEXT_WORD },
    { 1016.0f,      CreditWordType::WORD_AW ,       512,     CreditBrainType::BRAIN_NEXT_WORD },
    { 1018.0f,      CreditWordType::WORD_AW ,       573,     CreditBrainType::BRAIN_NEXT_WORD },
    { 1022.0f,      CreditWordType::WORD_AW ,       630,     CreditBrainType::BRAIN_NEXT_WORD },
    { 1024.0f,      CreditWordType::WORD_AW ,       656,     CreditBrainType::BRAIN_NEXT_WORD },
    { 1028.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_FLY_OFF   },
    { 1029.0f,      CreditWordType::WORD_AA ,         0,     CreditBrainType::BRAIN_OFF       },
    { 1033.0f,      CreditWordType::WORD_OFF,         0,     CreditBrainType::BRAIN_OFF       }
};
static int gCreditsTimingCount = LENGTH(gCreditsTiming);  //0x6A2B98

CreditsOverlay::CreditsOverlay(CreditScreen* theCreditScreen)
{
	mParent = theCreditScreen;
	mMouseVisible = false;
	mHasAlpha = true;
    mClip = false;
}

void CreditsOverlay::Draw(Graphics* g)
{
	mParent->DrawOverlay(g);
}

//0x433A70
// GOTY @Patoke: 0x4367F0
CreditScreen::CreditScreen(LawnApp* theApp)
{
	mApp = theApp;
	mClip = false;
	mCreditsReanimID = ReanimationID::REANIMATIONID_NULL;
	mFogParticleID = ParticleSystemID::PARTICLESYSTEMID_NULL;
	mUpdateCount = 0;
	mDrawCount = 0;
    mLastDrawCount = 0;
	mBlinkCountdown = 700;
	mDontSync = false;
	mCreditsPaused = false;
	mPreloaded = false;

	mApp->mEffectSystem->EffectSystemFreeAll();
	mApp->mMusic->StopAllMusic();
	TodLoadResources(ResourceGroupId::DelayLoad_Credits);

    mBrainPosX = 0.0f;
    mBrainPosY = 0.0f;
    mCreditsPhase = CreditsPhase::CREDITS_MAIN1;
    mCreditsPhaseCounter = 0;
    mDrawBrain = false;

	mMainMenuButton = MakeButton(CreditScreen::Credits_Button_MainMenu, this, gStr.CREDITS_MAIN_MENU_BUTTON);
	mMainMenuButton->Resize(298, 554, 209, 46);
	mMainMenuButton->SetVisible(false);

	mReplayButton = MakeNewButton(CreditScreen::Credits_Button_Replay, this, gStr.CREDITS_REPLAY_BUTTON, FONT_HOUSEOFTERROR16, IMAGE_CREDITS_PLAYBUTTON, nullptr, nullptr);
	mReplayButton->mTextDownOffsetX = 1;
	mReplayButton->mTextDownOffsetY = 1;
	mReplayButton->mColors[ButtonWidget::COLOR_LABEL] = Color(255, 255, 255);
	mReplayButton->mColors[ButtonWidget::COLOR_LABEL_HILITE] = Color(213, 159, 43);
	mReplayButton->Resize(mApp->mScreenBounds.mX - mX + 10, 530, 125, 65);
    mReplayButton->SetVisible(false);
	mReplayButton->mTextOffsetX = 33;
	mReplayButton->mTextOffsetY = -5;

	mOverlayWidget = new CreditsOverlay(this);
	mOverlayWidget->Resize(0, 0, BOARD_WIDTH, BOARD_HEIGHT);
	
	mOriginalMusicVolume = mApp->mMusicVolume;
	if (mApp->mMusicVolume < 0.1)
	{
		mApp->SetMusicVolume(0.85);
	}

	// GOTY 1073 004367F0 reads numbered role/name pairs, not the two
	// static columns used by 1051. '^' denotes a heading/spacer.
	// CREDITS_ROLES/NAMES 是完整 1..11 连续族：直接按索引取生成数组，
	// 不再逐节拼键探测存在性。
	for (int aSection = 1; aSection <= gCreditsRoleCount; ++aSection)
	{
		CreditSection aEntry{
			SexyString(gCreditsRoles[aSection]),
			SexyString(gCreditsNames[aSection])};
		if (aEntry.mRoles == "-") aEntry.mRoles = " ";
		if (aEntry.mNames == "-") aEntry.mNames = " ";
		mCreditSections.push_back(std::move(aEntry));
	}
	mCreditsContentHeight = DrawCreditsContent(nullptr, 0, false);
	mCreditsScrollPosition = static_cast<float>(mMainMenuButton->mY - 50);
}

//0x433E20、0x433E40
CreditScreen::~CreditScreen()
{
	mApp->SetMusicVolume(mOriginalMusicVolume);
	delete mReplayButton;
	delete mMainMenuButton;
	delete mOverlayWidget;
}

//0x433EE0
void CreditScreen::AddedToManager(WidgetManager* theWidgetManager)
{
	Widget::AddedToManager(theWidgetManager);
	AddWidget(mMainMenuButton);
	AddWidget(mReplayButton);
	AddWidget(mOverlayWidget);
}

//0x433F30
void CreditScreen::RemovedFromManager(WidgetManager* theWidgetManager)
{
	Widget::RemovedFromManager(theWidgetManager);
	RemoveWidget(mMainMenuButton);
	RemoveWidget(mReplayButton);
	RemoveWidget(mOverlayWidget);
}

//0x433F80
void CreditScreen::PreLoadCredits()
{
    mPreloaded = true;
	TodLoadResources(ResourceGroupId::DelayLoad_Background1);
	TodLoadResources(ResourceGroupId::DelayLoad_Background2);
	TodLoadResources(ResourceGroupId::DelayLoad_Background3);
	TodLoadResources(ResourceGroupId::DelayLoad_Background4);
	TodLoadResources(ResourceGroupId::DelayLoad_Background5);
	TodLoadResources(ResourceGroupId::DelayLoad_Background6);

    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_CREDITS_MAIN, true);
    ReanimationPreload(ReanimationType::REANIM_CREDITS_MAIN);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_CREDITS_MAIN2, true);
    ReanimationPreload(ReanimationType::REANIM_CREDITS_MAIN2);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_CREDITS_MAIN3, true);
    ReanimationPreload(ReanimationType::REANIM_CREDITS_MAIN3);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZOMBIE_CREDITS_DANCE, true);
    ReanimationPreload(ReanimationType::REANIM_ZOMBIE_CREDITS_DANCE);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_CREDITS_BIGBRAIN, true);
    ReanimationPreload(ReanimationType::REANIM_CREDITS_BIGBRAIN);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_CREDITS_FLOWER_PETALS, true);
    ReanimationPreload(ReanimationType::REANIM_CREDITS_FLOWER_PETALS);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_CREDITS_INFANTRY, true);
    ReanimationPreload(ReanimationType::REANIM_CREDITS_INFANTRY);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_CREDITS_STAGE, true);
    ReanimationPreload(ReanimationType::REANIM_CREDITS_STAGE);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_CREDITS_THROAT, true);
    ReanimationPreload(ReanimationType::REANIM_CREDITS_THROAT);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_SUNFLOWER, true);
    ReanimationPreload(ReanimationType::REANIM_SUNFLOWER);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_SUN, true);
    ReanimationPreload(ReanimationType::REANIM_SUN);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_CREDITS_CRAZYDAVE, true);
    ReanimationPreload(ReanimationType::REANIM_CREDITS_CRAZYDAVE);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZOMBIE, true);
    ReanimationPreload(ReanimationType::REANIM_ZOMBIE);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_DIGGER, true);
    ReanimationPreload(ReanimationType::REANIM_DIGGER);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZOMBIE_FOOTBALL, true);
    ReanimationPreload(ReanimationType::REANIM_ZOMBIE_FOOTBALL);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZOMBIE_CREDITS_CONEHEAD, true);
    ReanimationPreload(ReanimationType::REANIM_ZOMBIE_CREDITS_CONEHEAD);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZOMBIE_CREDITS_SCREEN_DOOR, true);
    ReanimationPreload(ReanimationType::REANIM_ZOMBIE_CREDITS_SCREEN_DOOR);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_CREDITS_ZOMBIEARMY1, true);
    ReanimationPreload(ReanimationType::REANIM_CREDITS_ZOMBIEARMY1);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_CREDITS_ZOMBIEARMY2, true);
    ReanimationPreload(ReanimationType::REANIM_CREDITS_ZOMBIEARMY2);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_CREDITS_TOMBSTONES, true);
    ReanimationPreload(ReanimationType::REANIM_CREDITS_TOMBSTONES);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_CREDITS_SOLARPOWER, true);
    ReanimationPreload(ReanimationType::REANIM_CREDITS_SOLARPOWER);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_CREDITS_ANYHOUR, true);
    ReanimationPreload(ReanimationType::REANIM_CREDITS_ANYHOUR);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_CREDITS_WEARETHEUNDEAD, true);
    ReanimationPreload(ReanimationType::REANIM_CREDITS_WEARETHEUNDEAD);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_CREDITS_DISCOLIGHTS, true);
    ReanimationPreload(ReanimationType::REANIM_CREDITS_DISCOLIGHTS);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_CREDITS_BOSSDANCE, true);
    ReanimationPreload(ReanimationType::REANIM_CREDITS_BOSSDANCE);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_TALLNUT, true);
    ReanimationPreload(ReanimationType::REANIM_TALLNUT);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_SNOWPEA, true);
    ReanimationPreload(ReanimationType::REANIM_SNOWPEA);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_PEASHOOTER, true);
    ReanimationPreload(ReanimationType::REANIM_PEASHOOTER);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_REPEATER, true);
    ReanimationPreload(ReanimationType::REANIM_REPEATER);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_KERNELPULT, true);
    ReanimationPreload(ReanimationType::REANIM_KERNELPULT);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_WALLNUT, true);
    ReanimationPreload(ReanimationType::REANIM_WALLNUT);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_FUMESHROOM, true);
    ReanimationPreload(ReanimationType::REANIM_FUMESHROOM);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_PUFFSHROOM, true);
    ReanimationPreload(ReanimationType::REANIM_PUFFSHROOM);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_SUNSHROOM, true);
    ReanimationPreload(ReanimationType::REANIM_SUNSHROOM);

    ReanimatorDefinition* aMain2Def = &gReanimatorDefArray[(int)ReanimationType::REANIM_CREDITS_MAIN2];
    for (int aTrackIndex = 0; aTrackIndex < aMain2Def->mTracks.count; aTrackIndex++)
    {
        ReanimatorTrack* aTrack = &aMain2Def->mTracks.tracks[aTrackIndex];
        if (!aTrack->mTransforms.mTransforms) continue; // GPU-only image tracks have no CPU font data.
        for (int aTransIndex = 0; aTransIndex < aTrack->mTransforms.count; aTransIndex++)
        {
            ReanimatorSourceTransform& aTrans = aTrack->mTransforms.mTransforms[aTransIndex];
            if (aTransIndex < 124 && (ReanimatorTrackIdAt(ReanimationType::REANIM_CREDITS_MAIN2, aTrackIndex) == static_cast<std::uint32_t>(ReanimTrackId::Words) ||
                                      ReanimatorTrackIdAt(ReanimationType::REANIM_CREDITS_MAIN2, aTrackIndex) == static_cast<std::uint32_t>(ReanimTrackId::Words2)))
            {
                aTrans.mFont = FONT_BRIANNETOD32BLACK;
            }
        }
    }
    ReanimatorDefinition* aMain3Def = &gReanimatorDefArray[(int)ReanimationType::REANIM_CREDITS_MAIN3];
    for (int aTrackIndex = 0; aTrackIndex < aMain3Def->mTracks.count; aTrackIndex++)
    {
        ReanimatorTrack* aTrack = &aMain3Def->mTracks.tracks[aTrackIndex];
        if (!aTrack->mTransforms.mTransforms) continue;
        for (int aTransIndex = 0; aTransIndex < aTrack->mTransforms.count; aTransIndex++)
        {
            ReanimatorSourceTransform& aTrans = aTrack->mTransforms.mTransforms[aTransIndex];
            // 1073 00437CE6..00437D83 tests MAIN3's own Words/Words2 tracks.
            // MAIN2 has only 25 tracks; MAIN3's text is at indices 60/61.
            if (aTransIndex < 124 && (ReanimatorTrackIdAt(ReanimationType::REANIM_CREDITS_MAIN3, aTrackIndex) == static_cast<std::uint32_t>(ReanimTrackId::Words) ||
                                      ReanimatorTrackIdAt(ReanimationType::REANIM_CREDITS_MAIN3, aTrackIndex) == static_cast<std::uint32_t>(ReanimTrackId::Words2)))
            {
                aTrans.mFont = FONT_BRIANNETOD32BLACK;
            }
        }
    }
    ReanimatorDefinition* aUndeadDef = &gReanimatorDefArray[(int)ReanimationType::REANIM_CREDITS_WEARETHEUNDEAD];
    for (int aTrackIndex = 0; aTrackIndex < aUndeadDef->mTracks.count; aTrackIndex++)
    {
        ReanimatorTrack* aTrack = &aUndeadDef->mTracks.tracks[aTrackIndex];
        if (!aTrack->mTransforms.mTransforms) continue;
        for (int aTransIndex = 0; aTransIndex < aTrack->mTransforms.count; aTransIndex++)
        {
            ReanimatorSourceTransform& aTrans = aTrack->mTransforms.mTransforms[aTransIndex];
            if (aTrans.mFont)
            {
                aTrans.mFont = FONT_BRIANNETOD32BLACK;
            }
        }
    }
}

//0x434C70
void CreditScreen::GetTiming(CreditsTiming** theBeforeTiming, CreditsTiming** theAfterTiming, float* theFraction)
{
    Reanimation* aCreditsReanim = mApp->ReanimationGet(mCreditsReanimID);
    float aFrameCount = aCreditsReanim->mDefinition->mTracks.tracks->mTransforms.count * (static_cast<double>(aCreditsReanim->SamplePlayback().phaseNs) / aCreditsReanim->mPeriodNs) - 1.0f;
    if (mCreditsPhase == CreditsPhase::CREDITS_MAIN1)
    {
        aFrameCount += 2.0f;
    }
    else if (mCreditsPhase == CreditsPhase::CREDITS_MAIN2)
    {
        aFrameCount += 400.0f;
    }
    else if (mCreditsPhase == CreditsPhase::CREDITS_MAIN3)
    {
        aFrameCount += 785.0f;
    }
    else
    {
        *theBeforeTiming = nullptr;
        *theAfterTiming = nullptr;
        *theFraction = 0.0f;
        return;
    }

    if (aFrameCount < gCreditsTiming[0].mFrame)
    {
        *theBeforeTiming = nullptr;
        *theAfterTiming = &gCreditsTiming[0];
        *theFraction = aFrameCount / gCreditsTiming[0].mFrame;
    }
    else
    {
        for (int i = 0; i < gCreditsTimingCount - 1; i++)
        {
            CreditsTiming* aTiming1 = &gCreditsTiming[i];
            CreditsTiming* aTiming2 = &gCreditsTiming[i + 1];
            TOD_ASSERT(aTiming1->mFrame < aTiming2->mFrame);

            if (aTiming2->mFrame > aFrameCount)
            {
                *theBeforeTiming = aTiming1;
                *theAfterTiming = aTiming2;
                *theFraction = (aFrameCount - aTiming1->mFrame) / (aTiming2->mFrame - aTiming1->mFrame);
                return;
            }
        }

        *theBeforeTiming = &gCreditsTiming[gCreditsTimingCount - 1];
        *theAfterTiming = nullptr;
        *theFraction = 0.0f;
    }
}

//0x434D60
Reanimation* CreditScreen::PlayReanim(int aIndex)
{
    Reanimation* aCreditsReanim = mApp->ReanimationTryToGet(mCreditsReanimID);
    if (aCreditsReanim)
    {
        aCreditsReanim->ReanimationDie();
    }

    if (aIndex == 1)
    {
        aCreditsReanim = mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_CREDITS_MAIN, mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, ReanimTimeDomain::Visual);
        aCreditsReanim->AssignRenderGroupToRange(ReanimTrackRanges::Background, 1);
        aCreditsReanim->AssignRenderGroupToRange(ReanimTrackRanges::attacher__Zombie, 2);
        aCreditsReanim->AssignRenderGroupToRange(ReanimTrackRanges::Words, 3);
        aCreditsReanim->AssignRenderGroupToRange(ReanimTrackRanges::SpotFront, 3);
    }
    else if (aIndex == 2)
    {
        aCreditsReanim = mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_CREDITS_MAIN2, mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, ReanimTimeDomain::Visual);
        aCreditsReanim->AssignRenderGroupToRange(ReanimTrackRanges::Background, 1);
        aCreditsReanim->AssignRenderGroupToRange(ReanimTrackRanges::attacher__Zombie, 2);
        aCreditsReanim->AssignRenderGroupToRange(ReanimTrackRanges::Words, 3);
        aCreditsReanim->AssignRenderGroupToRange(ReanimTrackRanges::SpotFront, 3);
        aCreditsReanim->AssignRenderGroupToRange(ReanimTrackRanges::attacher__undead, 2);
    }
    else if (aIndex == 3)
    {
        aCreditsReanim = mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_CREDITS_MAIN3, mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, ReanimTimeDomain::Visual);
        aCreditsReanim->AssignRenderGroupToRange(ReanimTrackRanges::Background, 1);
        aCreditsReanim->AssignRenderGroupToRange(ReanimTrackRanges::attacher__Zombie, 2);
        aCreditsReanim->AssignRenderGroupToRange(ReanimTrackRanges::attacher__DiscoLights, 2);
        aCreditsReanim->AssignRenderGroupToRange(ReanimTrackRanges::Words, 3);
        aCreditsReanim->AssignRenderGroupToRange(ReanimTrackRanges::attacher__cattail, 3);
        aCreditsReanim->AssignRenderGroupToRange(ReanimTrackRanges::SpotFront, 3);
        aCreditsReanim->AssignRenderGroupToRange(ReanimTrackRanges::attacher__undead, 2);
    }
    else
    {
        TOD_ASSERT(false);
        return nullptr;
    }

    aCreditsReanim->mIsAttachment = true;
    aCreditsReanim->ChangeLoopType(ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD);
    mCreditsReanimID = mApp->ReanimationGetID(aCreditsReanim);
	return aCreditsReanim;
}

//0x434F20
static void DrawDisco(Graphics* g, float aCenterX, float aCenterY, float theTime)
{
    float x1 = cos(theTime) * 600.0f;
    float y1 = sin(theTime) * 200.0f;
    float x2 = cos(theTime + PI / 2) * 600.0f;
    float y2 = sin(theTime + PI / 2) * 200.0f;
    float x3 = cos(theTime + PI) * 600.0f;
    float y3 = sin(theTime + PI) * 200.0f;
    float x4 = cos(theTime + PI + PI / 2) * 600.0f;
    float y4 = sin(theTime + PI + PI / 2) * 200.0f;

    TriVertex aVerts[2][3];
    int aColor = Color(255, 255, 255, 128).ToInt();
    aVerts[0][0].x = x1 + aCenterX;
    aVerts[0][0].y = y1 + aCenterY;
    aVerts[0][0].u = 0.0;
    aVerts[0][0].v = 0.0;
    aVerts[0][0].color = aColor;
    aVerts[0][1].x = x2 + aCenterX;
    aVerts[0][1].y = y2 + aCenterY;
    aVerts[0][1].u = 1.0;
    aVerts[0][1].v = 0.0;
    aVerts[0][1].color = aColor;
    aVerts[0][2].x = x4 + aCenterX;
    aVerts[0][2].y = y4 + aCenterY;
    aVerts[0][2].u = 0.0;
    aVerts[0][2].v = 1.0;
    aVerts[0][2].color = aColor;
    aVerts[1][0].x = x4 + aCenterX;
    aVerts[1][0].y = y4 + aCenterY;
    aVerts[1][0].u = 0.0;
    aVerts[1][0].v = 1.0;
    aVerts[1][0].color = aColor;
    aVerts[1][1].x = x2 + aCenterX;
    aVerts[1][1].y = y2 + aCenterY;
    aVerts[1][1].u = 1.0;
    aVerts[1][1].v = 0.0;
    aVerts[1][1].color = aColor;
    aVerts[1][2].x = x3 + aCenterX;
    aVerts[1][2].y = y3 + aCenterY;
    aVerts[1][2].u = 1.0;
    aVerts[1][2].v = 1.0;
    aVerts[1][2].color = aColor;

    // Use the same destination translation and clip as the movie's sprites.
    g->DrawTrianglesTex(IMAGE_REANIM_CREDITS_DISCOLIGHTS, aVerts, 2);
}

// The authored iris has opaque black edges, but its image is only 801x801.
// Extend those edges instead of stretching the transparent circular opening.
static void DrawIrisBorder(Graphics* g, Reanimation* movie)
{
    const int index=movie->FindTrackIndex(ReanimTrackId::attacher__Zombie1);
    ReanimatorTransform transform;
    movie->GetCurrentTransform(index,&transform);
    if(transform.mFrame<0 || transform.mImageSlot==0 || transform.mAlpha<=0) return;
    const auto& region=movie->mDefinition->mImageRects[transform.mImageSlot];
    SexyTransform2D matrix;
    movie->GetTrackMatrix(index,matrix);
    const float halfWidth=(std::abs(matrix.m00)*region.mWidth+std::abs(matrix.m01)*region.mHeight)*0.5f;
    const float halfHeight=(std::abs(matrix.m10)*region.mWidth+std::abs(matrix.m11)*region.mHeight)*0.5f;
    const auto& clip=g->mClipRect;
    const int left=std::clamp(int(std::ceil(matrix.m02+g->mTransX-halfWidth)),clip.mX,clip.mX+clip.mWidth);
    const int right=std::clamp(int(std::floor(matrix.m02+g->mTransX+halfWidth)),left,clip.mX+clip.mWidth);
    const int top=std::clamp(int(std::ceil(matrix.m12+g->mTransY-halfHeight)),clip.mY,clip.mY+clip.mHeight);
    const int bottom=std::clamp(int(std::floor(matrix.m12+g->mTransY+halfHeight)),top,clip.mY+clip.mHeight);
    Graphics border(*g);
    border.mTransX=border.mTransY=0;
    border.mColor=Color(0,0,0,ClampInt(FloatRoundToInt(transform.mAlpha*movie->mColorOverride.mAlpha),0,255));
    border.FillRect(clip.mX,clip.mY,left-clip.mX,clip.mHeight);
    border.FillRect(right,clip.mY,clip.mX+clip.mWidth-right,clip.mHeight);
    border.FillRect(left,clip.mY,right-left,top-clip.mY);
    border.FillRect(left,bottom,right-left,clip.mY+clip.mHeight-bottom);
}

//0x4351E0
void CreditScreen::DrawFogEffect(Graphics* g, float theTime)
{
    Reanimation* aCreditsReanim = mApp->ReanimationGet(mCreditsReanimID);
    Image* aFogImage = IMAGE_FOG;
    const Rect aFogFrame(0, 0, 210, 190);
    int aFadeAmount = theTime * 255.0f;

    for (int x = 0; x < 14; x++)
    {
        for (int y = 0; y < 7; y++)
        {
            // 取得格子内的雾的形状
            int aCelLook = x + (x + 17) * y;
            int aCelCol = aCelLook % 8;
            // 本格浓雾横坐标 = 列 * 80 + 浓雾偏移 - 15
            float aPosX = x * 80 - 15.0f;
            // 本格浓雾纵坐标 = 行 * 85 + 200
            float aPosY = y * 85 + 200.0f;
            // 开始计算周期变化的颜色，aAnimTime 为 MV 动画播放至当前时刻需要的时间（秒数）
            float aAnimTime = aCreditsReanim->mDefinition->mTracks.tracks->mTransforms.count * (static_cast<double>(aCreditsReanim->SamplePlayback().phaseNs) / aCreditsReanim->mPeriodNs) / (aCreditsReanim->mAnimRate * SECONDS_PER_UPDATE);
            float aTime = aAnimTime * PI * 2;
            // 与行、列有关的初始相位
            float aPhaseX = 6 * PI * x / 14;
            float aPhaseY = 6 * PI * y / 7;
            // 根据初相和时间计算当前相位
            float aMotion = 13 + 4 * sin(aTime / 900 + aPhaseY) + 8 * sin(aTime / 500 + aPhaseX);

            int aColorVariant = 255 - (aCelLook % 20) * 1.5f - aMotion * 1.5f;
            int aLightnessVariant = 255 - (aCelLook % 20) - aMotion;
            g->mColorizeImages = true;
            g->mColor = Color(aColorVariant, aColorVariant, aLightnessVariant, aFadeAmount);
            const Rect aFogRect(aCelCol * aFogFrame.mWidth, 0, aFogFrame.mWidth, aFogFrame.mHeight);
            g->DrawImage(aFogImage, aPosX, aPosY, aFogRect);
            g->mColorizeImages = false;
        }
    }
}

//0x4354E0
void CreditScreen::DrawOverlay(Graphics* g)
{
    if (mCreditsPhase == CreditsPhase::CREDITS_END)
    {
        int aFadeAlpha = TodAnimateCurveNs(500'000'000, 1'000'000'000, mCreditsPhaseCounter.mNanoseconds, 255, 0, Curves::Linear);
        if (aFadeAlpha > 0)
        {
            g->mColor = Color(0, 0, 0, aFadeAlpha);
            Rect screen = mApp->mScreenBounds;
            screen.Offset(-mX, -mY);
            g->FillRect(screen);
        }
    }
}

// GOTY 1073: 0043B410/0043B4D0/0043B590; the same layout measures and draws.
int CreditScreen::DrawCreditsContent(Graphics* g, int theY, bool theDraw)
{
	int aY = theY;
	for (const CreditSection& aSection : mCreditSections)
	{
		if (!aSection.mRoles.empty() && aSection.mRoles.front() == '^')
		{
			if (theDraw)
				TodDrawString(g, aSection.mRoles.substr(1), 380, aY + 20,
					FONT_HOUSEOFTERROR16, Color::White, DS_ALIGN_CENTER);
			aY += 29;
		}
		else
		{
			const int aRoleHeight = TodDrawStringWrappedHelper(g, aSection.mRoles,
				Rect(0, aY, 350, 768), FONT_HOUSEOFTERROR16, Color::White, DS_ALIGN_RIGHT, theDraw);
			const int aNameHeight = TodDrawStringWrappedHelper(g, aSection.mNames,
				Rect(380, aY, 400, 768), FONT_HOUSEOFTERROR16, Color::White, DS_ALIGN_LEFT, theDraw);
			aY += std::max(aRoleHeight, aNameHeight);
		}
		aY -= 8;
	}
	return aY;
}

// GOTY 1073 00438700: clip the roll above the replay button.
void CreditScreen::DrawFinalCredits(Graphics* g)
{
	Graphics roll(*g);
	roll.ClipRect(0, 0, BOARD_WIDTH, mReplayButton->mY - 10);
	DrawCreditsContent(&roll, static_cast<int>(mCreditsScrollPosition), true);
}

//0x435A90
static void DrawReanimToPreload(Graphics* g, ReanimationType theReanimType)
{
    Reanimation aReanim;
    aReanim.ApplyRate(0.3f);
    aReanim.ReanimationInitializeType(0.0f, 0.0f, theReanimType, 0);
    aReanim.Draw(g);
}

//0x435B60
void CreditScreen::Draw(Graphics* destination)
{
    Graphics movie(*destination);
    Graphics* g = &movie;
    // Authored movie positions remain relative to the centered widget. Let
    // backgrounds and effects extend into the rest of the actual viewport.
    Rect screen=mApp->mScreenBounds;
    screen.Offset(-mX,-mY);
    g->mLinearBlend = true;

    if (!mPreloaded)
    {
        g->mColor = Color::Black;
        g->FillRect(screen);
        mDrawCount = 1;
        return;
    }

    mDrawCount++;
    if (mDrawCount == 2)
    {
        g->DrawImage(IMAGE_BACKGROUND1, 0, 0);
        g->DrawImage(IMAGE_BACKGROUND2, 0, 0);
        g->DrawImage(IMAGE_BACKGROUND3, 0, 0);
        g->DrawImage(IMAGE_BACKGROUND4, 0, 0);
        g->DrawImage(IMAGE_BACKGROUND5, 0, 0);
        DrawReanimToPreload(g, ReanimationType::REANIM_CREDITS_BOSSDANCE);
        DrawReanimToPreload(g, ReanimationType::REANIM_CREDITS_CRAZYDAVE);
        DrawReanimToPreload(g, ReanimationType::REANIM_ZOMBIE_CREDITS_DANCE);
        DrawReanimToPreload(g, ReanimationType::REANIM_CREDITS_STAGE);
        DrawReanimToPreload(g, ReanimationType::REANIM_SUNFLOWER);
    }
    g->mColor = Color::Black;
    g->FillRect(screen);

    Reanimation* aCreditsReanim = mApp->ReanimationGet(mCreditsReanimID);
    const double aPlaybackPhase = static_cast<double>(aCreditsReanim->SamplePlayback().phaseNs) /
        aCreditsReanim->mPeriodNs;
    int aFrameCount = aCreditsReanim->mDefinition->mTracks.tracks->mTransforms.count - 1;
    float aFrameFactor = 1.0f / aFrameCount;
    int aBackground1Index = aCreditsReanim->FindTrackIndex(ReanimTrackId::Background);
    ReanimatorTransform aTransformBackground1;
    aCreditsReanim->GetCurrentTransform(aBackground1Index, &aTransformBackground1);
    int aBackground2Index = aCreditsReanim->FindTrackIndex(ReanimTrackId::Background2);
    ReanimatorTransform aTransformBackground2;
    aCreditsReanim->GetCurrentTransform(aBackground2Index, &aTransformBackground2);
    const Rect movieClip=g->mClipRect;
    Graphics aBackground2G(*g);

    // These crops compose independent shots inside the animated white frames.
    // They are scene masks, not the removed 800x600 viewport boundary.
    bool aDrawClippedBackgrounds1 = false;
    bool aDrawClippedBackgrounds2 = false;
    if (mCreditsPhase == CreditsPhase::CREDITS_MAIN2 && aFrameFactor * 125.0f > aPlaybackPhase)
    {
        aDrawClippedBackgrounds1 = true;
    }
    if (mCreditsPhase == CreditsPhase::CREDITS_MAIN3 && aFrameFactor * 125.0f > aPlaybackPhase)
    {
        aDrawClippedBackgrounds2 = true;
    }

    if (mCreditsPhase == CreditsPhase::CREDITS_END)
    {
        DrawFinalCredits(g);
    }
    if (aDrawClippedBackgrounds1)
    {
        int aBackground3Index = aCreditsReanim->FindTrackIndex(ReanimTrackId::Background3);
        ReanimatorTransform aTransformBackground3;
        aCreditsReanim->GetCurrentTransform(aBackground3Index, &aTransformBackground3);
        int aBackground4Index = aCreditsReanim->FindTrackIndex(ReanimTrackId::Background4);
        ReanimatorTransform aTransformBackground4;
        aCreditsReanim->GetCurrentTransform(aBackground4Index, &aTransformBackground4);
        
        if (aTransformBackground2.mFrame != -1.0f)
        {
            const TextureRegion& aRegion = aCreditsReanim->mDefinition->mImageRects[aTransformBackground2.mImageSlot];
            aBackground2G.ClipRect(aTransformBackground2.mTransX, aTransformBackground2.mTransY, aRegion.mWidth - 1, aRegion.mHeight - 1);
            aBackground2G.DrawImageF(IMAGE_BACKGROUND1, aTransformBackground2.mTransX - BOARD_WIDTH / 2, aTransformBackground2.mTransY - BOARD_HEIGHT / 2);
            aBackground2G.mClipRect = movieClip;
        }
        if (aTransformBackground3.mFrame != -1.0f)
        {
            Graphics aBackground3G(*g);
            const TextureRegion& aRegion = aCreditsReanim->mDefinition->mImageRects[aTransformBackground3.mImageSlot];
            aBackground3G.ClipRect(aTransformBackground3.mTransX, aTransformBackground3.mTransY, aRegion.mWidth - 1, aRegion.mHeight - 1);
            // 1073 00438CE8: this window crops a different part of the same yard.
            aBackground3G.DrawImageF(IMAGE_BACKGROUND1, aTransformBackground3.mTransX - 600.0f, aTransformBackground3.mTransY - 300.0f);
        }
        if (aTransformBackground4.mFrame != -1.0f)
        {
            Graphics aBackground4G(*g);
            const TextureRegion& aRegion = aCreditsReanim->mDefinition->mImageRects[aTransformBackground4.mImageSlot];
            aBackground4G.ClipRect(aTransformBackground4.mTransX, aTransformBackground4.mTransY, aRegion.mWidth - 1, aRegion.mHeight - 1);
            aBackground4G.DrawImageF(IMAGE_BACKGROUND2, aTransformBackground4.mTransX - BOARD_WIDTH / 2, aTransformBackground4.mTransY - BOARD_HEIGHT / 2);
        }
    }
    if (aDrawClippedBackgrounds2)
    {
        int aBackground1Index = aCreditsReanim->FindTrackIndex(ReanimTrackId::Background);
        ReanimatorTransform aTransformBackground1;
        aCreditsReanim->GetCurrentTransform(aBackground1Index, &aTransformBackground1);
        int aBackground3Index = aCreditsReanim->FindTrackIndex(ReanimTrackId::Background3);
        ReanimatorTransform aTransformBackground3;
        aCreditsReanim->GetCurrentTransform(aBackground3Index, &aTransformBackground3);
        int aBackground4Index = aCreditsReanim->FindTrackIndex(ReanimTrackId::Background4);
        ReanimatorTransform aTransformBackground4;
        aCreditsReanim->GetCurrentTransform(aBackground4Index, &aTransformBackground4);

        if (aTransformBackground1.mFrame != -1.0f)
        {
            Graphics aBackground1G(*g);
            const TextureRegion& aRegion = aCreditsReanim->mDefinition->mImageRects[aTransformBackground1.mImageSlot];
            aBackground1G.ClipRect(aTransformBackground1.mTransX, aTransformBackground1.mTransY, aRegion.mWidth - 1, aRegion.mHeight - 1);
            // 1073 00438F42/00438F61: the tricycle scene uses (-80, -360).
            aBackground1G.DrawImageF(IMAGE_BACKGROUND1, aTransformBackground1.mTransX - 80.0f, aTransformBackground1.mTransY - 360.0f);
        }
        if (aTransformBackground3.mFrame != -1.0f)
        {
            Graphics aBackground3G(*g);
            aBackground3G.mTransX += aTransformBackground3.mTransX - 20.0f;
            aBackground3G.mTransY += aTransformBackground3.mTransY - 260.0f;
            const TextureRegion& aRegion = aCreditsReanim->mDefinition->mImageRects[aTransformBackground3.mImageSlot];
            aBackground3G.ClipRect(20, 260, aRegion.mWidth - 1, aRegion.mHeight - 1);
            aBackground3G.DrawImageF(IMAGE_BACKGROUND3, -220.0f, 0.0f);
            aBackground3G.DrawImageF(IMAGE_POOL, 34.0f, 278.0f);
        }
        if (aTransformBackground4.mFrame != -1.0f)
        {
            Graphics aBackground4G(*g);
            const TextureRegion& aRegion = aCreditsReanim->mDefinition->mImageRects[aTransformBackground4.mImageSlot];
            aBackground4G.ClipRect(aTransformBackground4.mTransX, aTransformBackground4.mTransY, aRegion.mWidth - 1, aRegion.mHeight - 1);
            aBackground4G.DrawImageF(IMAGE_BACKGROUND2, aTransformBackground4.mTransX - BOARD_WIDTH / 2, aTransformBackground4.mTransY - BOARD_HEIGHT / 2);
        }
    }
    aCreditsReanim->DrawRenderGroup(g, 1);

    bool aDrawPool = false;
    bool aDrawNightPool = false;
    bool aDrawDoorBottom = false;
    bool aDrawChimney = false;
    bool aDrawDiscoLights = false;
    bool aDrawFog = false;
    if (mCreditsPhase == CreditsPhase::CREDITS_MAIN1)
    {
        if (aPlaybackPhase > aFrameFactor * 203.0f && aPlaybackPhase < aFrameFactor * 268.0f)
        {
            aDrawPool = true;
        }
        if (aPlaybackPhase > aFrameFactor * 305.0f && aPlaybackPhase < aFrameFactor * 339.0f)
        {
            aDrawPool = true;
            aDrawDoorBottom = true;
        }
    }
    if (mCreditsPhase == CreditsPhase::CREDITS_MAIN2)
    {
        if (aPlaybackPhase > aFrameFactor * 187.0f && aPlaybackPhase < aFrameFactor * 249.0f)
        {
            aDrawPool = true;
            aDrawNightPool = true;
            if (aTransformBackground1.mFrame != -1.0f || aTransformBackground2.mFrame != -1.0f)
            {
                aDrawDiscoLights = true;
            }
        }
        if (aPlaybackPhase > aFrameFactor * 123.0f && aPlaybackPhase < aFrameFactor * 189.0f)
        {
            if (aTransformBackground1.mFrame != -1.0f)
            {
                aDrawDiscoLights = true;
            }
        }
        if (aPlaybackPhase > aFrameFactor * 189.0f && aPlaybackPhase < aFrameFactor * 249.0f)
        {
            if (aTransformBackground2.mFrame != -1.0f)
            {
                aDrawDiscoLights = true;
                aDrawFog = true;
            }
        }
    }
    if (mCreditsPhase == CreditsPhase::CREDITS_MAIN3)
    {
        if (aPlaybackPhase > aFrameFactor * 123.0f && aPlaybackPhase < aFrameFactor * 218.0f)
        {
            aDrawChimney = true;
        }
        if (aPlaybackPhase > aFrameFactor * 217.0f)
        {
            if (aTransformBackground1.mFrame != -1.0f)
            {
                aDrawDiscoLights = true;
            }
        }
    }
    if (aTransformBackground2.mFrame == -1.0f)
    {
        //aDrawPool = false;
        //aDrawNightPool = false;
        aDrawDoorBottom = false;
        aDrawChimney = false;
    }
    else
    {
        aBackground2G.mTransX += aTransformBackground2.mTransX + 220.0f;
        if (aDrawPool || aDrawNightPool)
        {
            mApp->mPoolEffect->PoolEffectDraw(&aBackground2G, aDrawNightPool);
        }
    }
    
    if (aDrawDoorBottom)
    {
        aBackground2G.DrawImage(IMAGE_BACKGROUND3_GAMEOVER_INTERIOR_OVERLAY, -171, 241);
    }
    aCreditsReanim->DrawRenderGroup(g, 0);

    if (aDrawDoorBottom)
        g->ClipRect(48, 0, BOARD_WIDTH, BOARD_HEIGHT);
    if (aDrawDiscoLights)
    {
        float aDiscoTime = aCreditsReanim->mDefinition->mTracks.tracks->mTransforms.count * aPlaybackPhase / (aCreditsReanim->mAnimRate);
        DrawDisco(g, 600.0f, 450.0f, aDiscoTime);
        DrawDisco(g, 200.0f, 450.0f, aDiscoTime);
    }
    if (aDrawFog)
    {
        aBackground2G.DrawImage(IMAGE_REANIM_CREDITS_FOGMACHINE, 600, 200);
    }
    aCreditsReanim->DrawRenderGroup(g, 2);

    if (aDrawDoorBottom)
    {
        g->mClipRect=movieClip;
        aBackground2G.DrawImage(IMAGE_BACKGROUND3_GAMEOVER_MASK, -172, 234);
    }
    if (aDrawChimney)
    {
        aBackground2G.DrawImage(IMAGE_BACKGROUND5_GAMEOVER_MASK, -220, 81);
    }
    if (mCreditsPhase==CreditsPhase::CREDITS_MAIN3 &&
        aPlaybackPhase>=aFrameFactor*240.0f && aPlaybackPhase<aFrameFactor*300.0f)
        DrawIrisBorder(g,aCreditsReanim);
    aCreditsReanim->DrawRenderGroup(g, 3);

    TodParticleSystem* aParticle = nullptr;
    while (mApp->mEffectSystem->mParticleHolder->mParticleSystems.IterateNext(aParticle))
    {
        if (!aParticle->mIsAttachment && !aParticle->mDead)
        {
            aParticle->Draw(g);
        }
    }

    if (aDrawFog)
    {
        float aPercent = TodAnimateCurveFloat(aFrameFactor * 189.0f, aFrameFactor * 249.0f, aPlaybackPhase, 0.0f, 1.0f, Curves::Linear);
        DrawFogEffect(&aBackground2G, aPercent);
    }
    aCreditsReanim->DrawRenderGroup(g, 3);

    if (mDrawBrain)
    {
        g->DrawImageF(IMAGE_BRAIN, mBrainPosX, mBrainPosY);
    }
}

Reanimation* CreditScreen::FindSubReanim(Reanimation* theReanim, ReanimationType theReanimType)
{
    if (theReanim->mReanimationType == theReanimType)
        return theReanim;

    for (int i = 0; i < theReanim->mDefinition->mTracks.count; i++)
    {
        Reanimation* aReanimation = FindReanimAttachment(theReanim->mTrackInstances[i].mAttachmentID);
        if (aReanimation != nullptr)
        {
            Reanimation* aSubReanim = FindSubReanim(aReanimation, theReanimType);
            if (aSubReanim != nullptr)
            {
                return aSubReanim;
            }
        }
    }

    return nullptr;
}

//0x436940
void CreditScreen::UpdateBlink()
{
    mBlinkCountdown--;
    if (mBlinkCountdown > 0)
    {
        return;
    }

    mBlinkCountdown = 700;
    Reanimation* aCreditsReanim = mApp->ReanimationGet(mCreditsReanimID);
    Reanimation* aSunFlowerReanim = FindSubReanim(aCreditsReanim, ReanimationType::REANIM_SUNFLOWER);
    if (aSunFlowerReanim)
    {
        if (mCreditsPhase == CreditsPhase::CREDITS_MAIN3)
        {
            int aFrameCount = aCreditsReanim->mDefinition->mTracks.tracks->mTransforms.count - 1;
            float aFrameFactor = 1.0f / aFrameCount;
            if ((static_cast<double>(aCreditsReanim->SamplePlayback().phaseNs) / aCreditsReanim->mPeriodNs) > aFrameFactor * 200.0f)
            {
                return;
            }
        }

        Reanimation* aBlinkReanim = mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_SUNFLOWER, mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, ReanimTimeDomain::Visual);
        aBlinkReanim->SetFramesForLayer(ReanimTrackId::anim_blink);
        aBlinkReanim->ApplyRate(15.0f);
        aBlinkReanim->ChangeLoopType(ReanimLoopType::REANIM_PLAY_ONCE_FULL_LAST_FRAME);
        aBlinkReanim->AttachToAnotherReanimation(aSunFlowerReanim, ReanimTrackId::anim_idle);
    }
}

//0x436A30
void CreditScreen::Update()
{
    const int replayX = mApp->mScreenBounds.mX - mX + 10;
    if (mReplayButton->mX != replayX)
        mReplayButton->Move(replayX, mReplayButton->mY);
    Widget::Update();
    if (!mCreditsPaused && !mMainMenuButton->mIsOver && !mReplayButton->mIsOver)
    {
        mApp->SetCursor(CURSOR_POINTER);
    }
    if (mDrawCount == 0 || mCreditsPaused)
    {
        return;
    }

    mUpdateCount++;
    if (mUpdateCount == 1)
    {
        PreLoadCredits();
        PlayReanim(1);
        mApp->mMusic->MusicCreditScreenInit();
        mApp->mMusic->MakeSureMusicIsPlaying(MusicTune::MUSIC_TUNE_CREDITS_ZOMBIES_ON_YOUR_LAWN);
        mApp->ClearUpdateBacklog(false);
        mTimerSinceStart.Start();
    }
    else if (mDontSync)
    {
        UpdateMovie();
    }
    else if (mCreditsPhase == CreditsPhase::CREDITS_END)
    {
        UpdateMovie();
        // 1073 004396F0: 0.25 logical pixels per 100 Hz update, then wrap
        // after the whole roll and a further 120 pixels have passed.
        mCreditsScrollPosition = AdvanceCreditsScroll(mCreditsScrollPhaseNs,
            mMainMenuButton->mY - 50, mCreditsContentHeight, Sexy::gUpdateIntervalNs);
    }
    else if (mUpdateCount > 1)
    {
        Reanimation* aCreditsReanim = mApp->ReanimationGet(mCreditsReanimID);
        int aDurationSinceStart = mTimerSinceStart.GetDuration();
        int aDurationReanimation = (aCreditsReanim->mDefinition->mTracks.tracks->mTransforms.count * (static_cast<double>(aCreditsReanim->SamplePlayback().phaseNs) / aCreditsReanim->mPeriodNs) / (aCreditsReanim->mAnimRate)) * 1000.0f;
        if (mCreditsPhase == CreditsPhase::CREDITS_MAIN2)
        {
            aDurationReanimation += 57142;
        }
        else if (mCreditsPhase == CreditsPhase::CREDITS_MAIN3)
        {
            aDurationReanimation += 112000;
        }

        int aUnsyncedDuration = aDurationSinceStart - aDurationReanimation;
        const double aFrameMs = mApp->mFrameTiming.mIntervalNs / 1'000'000.0;
        int aUnsyncedFrames = static_cast<int>((aUnsyncedDuration + aFrameMs * 0.5) / aFrameMs);
        if (aUnsyncedDuration > 10000)
        {
            JumpToFrame((CreditsPhase)((int)mCreditsPhase + 1), 0.0f);
            aUnsyncedFrames = 0;
        }
        while (aUnsyncedFrames > 0)
        {
            UpdateMovie();
            aUnsyncedFrames--;
        }
    }

    mLastDrawCount = mDrawCount;
    MarkDirty();
}

namespace
{
    // CreditScreen::UpdateMovie 向日葵唱歌头贴表（B 形态手工迁移）：aSunflowerFrame
    // 1..5 -> 头部贴图覆盖；0（初值）与越界 = 原 default 行（nullptr，无覆盖）。
    // 逐槽对照原 switch 0 差异（int 下标，非枚举驱动）。
    constexpr std::uint32_t kSunflowerHeadAppearance[6] = {
        ReanimOptions::SunFlower::anim_idle::Default,
        ReanimOptions::SunFlower::anim_idle::SUNFLOWER_HEAD_SING1,
        ReanimOptions::SunFlower::anim_idle::SUNFLOWER_HEAD_SING2,
        ReanimOptions::SunFlower::anim_idle::SUNFLOWER_HEAD_SING3,
        ReanimOptions::SunFlower::anim_idle::SUNFLOWER_HEAD_SING4,
        ReanimOptions::SunFlower::anim_idle::SUNFLOWER_HEAD_SING5,
    };
    static_assert(sizeof(kSunflowerHeadAppearance) / sizeof(kSunflowerHeadAppearance[0]) == 6,
        "kSunflowerHeadOverride 槽位须与 aSunflowerFrame 值域(0..5)同步");

    // CreditScreen::UpdateMovie 相位推进表：表内匿名 lambda = 原 case 体逐句
    // 迁移（原支 mLoopCount > 0 前置保留在 lambda 内）；aCreditsReanim 经引用
    // 传出（MAIN1/MAIN2 支 PlayReanim 重指向）。
    using CreditsPhaseFn = void (*)(CreditScreen&, Reanimation*&);

    // 匿名 lambda 函数指针表，行序 = CreditsPhase 枚举值序（0..3）。
    constexpr CreditsPhaseFn kCreditsPhaseCases[(int)CreditsPhase::CREDITS_END + 1] = {
        // 原 case CREDITS_MAIN1 && mLoopCount > 0
        [](CreditScreen& s, Reanimation*& aCreditsReanim)
        {
            if (aCreditsReanim->mLoopCount > 0)
            {
                aCreditsReanim = s.PlayReanim(2);
                s.mCreditsPhase = CreditsPhase::CREDITS_MAIN2;
            }
        },  // 0 CREDITS_MAIN1
        // 原 case CREDITS_MAIN2 && mLoopCount > 0
        [](CreditScreen& s, Reanimation*& aCreditsReanim)
        {
            if (aCreditsReanim->mLoopCount > 0)
            {
                aCreditsReanim = s.PlayReanim(3);
                s.mCreditsPhase = CreditsPhase::CREDITS_MAIN3;
            }
        },  // 1 CREDITS_MAIN2
        // 原 case CREDITS_MAIN3 && mLoopCount > 0
        [](CreditScreen& s, Reanimation*& aCreditsReanim)
        {
            if (aCreditsReanim->mLoopCount > 0)
            {
                s.mCreditsPhase = CreditsPhase::CREDITS_END;
            }
        },  // 2 CREDITS_MAIN3
        // 原 case CREDITS_END
        [](CreditScreen& s, Reanimation*& aCreditsReanim)
        {
            (void)aCreditsReanim;
            s.mCreditsPhaseCounter++;
            if (s.mCreditsPhaseCounter == 50)
            {
                s.mMainMenuButton->SetVisible(true);
                s.mReplayButton->SetVisible(true);
            }
        },  // 3 CREDITS_END
    };
    static_assert(sizeof(kCreditsPhaseCases) / sizeof(kCreditsPhaseCases[0]) ==
        (int)CreditsPhase::CREDITS_END + 1, "kCreditsPhaseCases 槽位数须与 CreditsPhase 枚举同步");
}  // namespace

namespace
{
    // UpdateMovie 脑子移动相位表（C 形态 -> 匿名 lambda 函数指针表）：行序 =
    // CreditBrainType 枚举值序 0..5（原链书写序 FLY_ON/FAST_ON/FLY_OFF/FAST_OFF
    // 与值序不同，勿照抄链序）。nullptr = 该 brain 类型无 after 支（NEXT_WORD/
    // OFF），未命中时回落函数内保留的 aBeforeTiming 支（原第 5 支，判别式
    // 切换到 aBeforeTiming->mBrainType 属混排，表外保留）。lambda 体 = 原 case
    // 体逐句迁移。
    using BrainMoveFn = void (*)(CreditScreen&, const CreditsTiming*, const CreditsTiming*, float);

    constexpr BrainMoveFn kBrainMoveCases[(int)CreditBrainType::BRAIN_OFF + 1] = {
        // 原 aAfterTiming->mBrainType == BRAIN_FLY_ON 支
        [](CreditScreen& s, const CreditsTiming* aBeforeTiming, const CreditsTiming* aAfterTiming, float aFraction)
        {
            (void)aBeforeTiming;
            s.mBrainPosX = TodAnimateCurveFloat(0.0f, 1.0f, aFraction, -50.0f, aAfterTiming->mWordX - 15.0f, Curves::EaseInOut);
            s.mBrainPosY = TodAnimateCurveFloat(0.0f, 1.0f, aFraction, 505.0f, 485.0f, Curves::BounceFastMiddle);
            s.mDrawBrain = true;
        },  // 0 BRAIN_FLY_ON
        // 原 aAfterTiming->mBrainType == BRAIN_FAST_ON 支
        [](CreditScreen& s, const CreditsTiming* aBeforeTiming, const CreditsTiming* aAfterTiming, float aFraction)
        {
            (void)aBeforeTiming;
            s.mBrainPosX = TodAnimateCurveFloat(0.0f, 1.0f, aFraction, aAfterTiming->mWordX - 50.0f, aAfterTiming->mWordX - 15.0f, Curves::EaseInOut);
            s.mBrainPosY = TodAnimateCurveFloat(0.0f, 1.0f, aFraction, 485.0f, 505.0f, Curves::EaseInOut);
            s.mDrawBrain = true;
        },  // 1 BRAIN_FAST_ON
        nullptr,  // 2 BRAIN_NEXT_WORD（无 after 支；走表外 aBeforeTiming 回落支）
        // 原 aAfterTiming->mBrainType == BRAIN_FAST_OFF 支
        [](CreditScreen& s, const CreditsTiming* aBeforeTiming, const CreditsTiming* aAfterTiming, float aFraction)
        {
            (void)aAfterTiming;
            s.mBrainPosX = TodAnimateCurveFloat(0.0f, 1.0f, aFraction, aBeforeTiming->mWordX - 15.0f, aBeforeTiming->mWordX + 25.0f, Curves::EaseInOut);
            s.mBrainPosY = TodAnimateCurveFloat(0.0f, 1.0f, aFraction, 505.0f, 485.0f, Curves::EaseInOut);
            s.mDrawBrain = true;
        },  // 3 BRAIN_FAST_OFF
        // 原 aAfterTiming->mBrainType == BRAIN_FLY_OFF 支
        [](CreditScreen& s, const CreditsTiming* aBeforeTiming, const CreditsTiming* aAfterTiming, float aFraction)
        {
            (void)aAfterTiming;
            s.mBrainPosX = TodAnimateCurveFloat(0.0f, 1.0f, aFraction, aBeforeTiming->mWordX - 15.0f, 850.0f, Curves::EaseInOut);
            s.mBrainPosY = TodAnimateCurveFloat(0.0f, 1.0f, aFraction, 505.0f, 485.0f, Curves::BounceFastMiddle);
            s.mDrawBrain = true;
        },  // 4 BRAIN_FLY_OFF
        nullptr,  // 5 BRAIN_OFF（无 after 支；走表外 aBeforeTiming 回落支）
    };
    static_assert(sizeof(kBrainMoveCases) / sizeof(kBrainMoveCases[0]) ==
        (int)CreditBrainType::BRAIN_OFF + 1, "kBrainMoveCases 槽位须与 CreditBrainType 枚举同步");
    static_assert(kBrainMoveCases[(int)CreditBrainType::BRAIN_NEXT_WORD] == nullptr &&
        kBrainMoveCases[(int)CreditBrainType::BRAIN_OFF] == nullptr &&
        kBrainMoveCases[(int)CreditBrainType::BRAIN_FLY_OFF] != nullptr &&
        kBrainMoveCases[(int)CreditBrainType::BRAIN_FAST_OFF] != nullptr,
        "kBrainMoveCases 关键槽位锚定（NEXT_WORD/OFF 空槽，FLY_OFF/FAST_OFF 有支）");
}  // namespace

//0x436BE0
void CreditScreen::UpdateMovie()
{
    UpdateBlink();

    // Credits owns this clock: each catch-up iteration is a real movie step.
    // LawnApp leaves it frozen while Credits is paused or awaiting its first draw.
    if (Sexy::gUpdateIntervalNs > INT64_MAX)
        throw std::overflow_error("credits movie update exceeds clock range");
    auto& visualNowNs = mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
    visualNowNs = ReanimClock::Add(visualNowNs, static_cast<std::int64_t>(Sexy::gUpdateIntervalNs));

    Reanimation* aCreditsReanim = mApp->ReanimationGet(mCreditsReanimID);
    const int aFrameCount = aCreditsReanim->mDefinition->mTracks.tracks->mTransforms.count - 1;
    float aFrameFactor = 1.0f / aFrameCount;

    aCreditsReanim->UpdateAt(visualNowNs);
    mApp->mEffectSystem->UpdateAt(visualNowNs, visualNowNs);
    mApp->mPoolEffect->PoolEffectUpdate(visualNowNs, Sexy::gUpdateIntervalNs);
    TurnOffTongues(aCreditsReanim, 0);

    // C 形态 -> 匿名 lambda 表（上方 kCreditsPhaseCases）：行序 = CreditsPhase
    // 枚举序（0..3，无 NUM 哨兵=末值 CREDITS_END 锚定）；全槽覆盖无 default。
    const unsigned aPhaseIndex = static_cast<unsigned>(mCreditsPhase);
    if (aPhaseIndex <= static_cast<unsigned>(CreditsPhase::CREDITS_END))
    {
        kCreditsPhaseCases[aPhaseIndex](*this, aCreditsReanim);
    }

    if (mCreditsPhase == CreditsPhase::CREDITS_MAIN1)
    {
        if ((aCreditsReanim->TimedEventCount(128, aFrameCount) != 0) ||
            (aCreditsReanim->TimedEventCount(130, aFrameCount) != 0) ||
            (aCreditsReanim->TimedEventCount(132, aFrameCount) != 0) ||
            (aCreditsReanim->TimedEventCount(134, aFrameCount) != 0) ||
            (aCreditsReanim->TimedEventCount(136, aFrameCount) != 0) ||
            (aCreditsReanim->TimedEventCount(138, aFrameCount) != 0) ||
            (aCreditsReanim->TimedEventCount(140, aFrameCount) != 0) ||
            (aCreditsReanim->TimedEventCount(142, aFrameCount) != 0))
        {
            mApp->AddTodParticle(BOARD_WIDTH / 2, BOARD_HEIGHT / 2, (int)RenderLayer::RENDER_LAYER_TOP, ParticleEffect::PARTICLE_CREDIT_STROBE, mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs);
        }
        if (aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 136.5f))
        {
            mApp->AddTodParticle(BOARD_WIDTH / 2, BOARD_HEIGHT / 2, (int)RenderLayer::RENDER_LAYER_TOP, ParticleEffect::PARTICLE_CREDITS_RAYSWIPE, mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs);
        }
        if ((aCreditsReanim->TimedEventCount(330, aFrameCount) != 0))
        {
            mApp->PlayFoley(FoleyType::FOLEY_SCREAM);
        }
        if ((aCreditsReanim->TimedEventCount(336, aFrameCount) != 0))
        {
            mApp->mSoundSystem->StopFoley(FoleyType::FOLEY_SCREAM);
        }
    }
    if (mCreditsPhase == CreditsPhase::CREDITS_MAIN2 || mCreditsPhase == CreditsPhase::CREDITS_MAIN3)
    {
        ReanimatorTrackInstance* aTrackWords1 = aCreditsReanim->GetTrackInstanceByName(ReanimTrackId::Words);
        ReanimatorTrackInstance* aTrackWords2 = aCreditsReanim->GetTrackInstanceByName(ReanimTrackId::Words2);
        if ((static_cast<double>(aCreditsReanim->SamplePlayback().phaseNs) / aCreditsReanim->mPeriodNs) < aFrameFactor * 124.0f)
        {
            aTrackWords1->mTrackColor = Color::Black;
            aTrackWords2->mTrackColor = Color::Black;
        }
        else
        {
            aTrackWords1->mTrackColor = Color::White;
            aTrackWords2->mTrackColor = Color::White;
        }

        Reanimation* aUndeadReanim = FindSubReanim(aCreditsReanim, ReanimationType::REANIM_CREDITS_WEARETHEUNDEAD);
        if (aUndeadReanim)
        {
            ReanimatorTrackInstance* aTrackWordsUndead1 = aUndeadReanim->GetTrackInstanceByName(ReanimTrackId::bubbletext1);
            ReanimatorTrackInstance* aTrackWordsUndead2 = aUndeadReanim->GetTrackInstanceByName(ReanimTrackId::bubbletext2);
            ReanimatorTrackInstance* aTrackWordsUndead3 = aUndeadReanim->GetTrackInstanceByName(ReanimTrackId::bubbletext3);
            aTrackWordsUndead1->mTrackColor = Color::Black;
            aTrackWordsUndead2->mTrackColor = Color::Black;
            aTrackWordsUndead3->mTrackColor = Color::Black;

            ReanimatorTrackInstance* aTrackWordsShake = aUndeadReanim->GetTrackInstanceByName(ReanimTrackId::ShakyText);
            if ((static_cast<double>(aCreditsReanim->SamplePlayback().phaseNs) / aCreditsReanim->mPeriodNs) > aFrameFactor * 112.0f)
            {
                aTrackWordsShake->mShakeOverride = 2.0f;
            }
            else
            {
                aTrackWordsShake->mShakeOverride = 0.0f;
            }
        }

        if ((aCreditsReanim->TimedEventCount(120, aFrameCount) != 0))
        {
            mApp->AddTodParticle(BOARD_WIDTH / 2, BOARD_HEIGHT / 2, (int)RenderLayer::RENDER_LAYER_TOP, ParticleEffect::PARTICLE_CREDITS_ZOMBIEHEADWIPE, mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs);
        }
    }
    if (mCreditsPhase == CreditsPhase::CREDITS_MAIN2)
    {
        if (aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 111.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 115.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 119.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 121.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 123.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 125.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 127.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 131.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 135.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 139.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 143.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 147.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 151.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 155.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 159.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 163.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 167.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 171.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 175.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 179.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 183.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 187.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 191.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 195.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 199.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 203.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 207.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 211.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 215.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 219.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 223.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 227.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 231.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 235.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 239.5f) ||
            aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 243.5f))
        {
            mApp->AddTodParticle(BOARD_WIDTH / 2, BOARD_HEIGHT / 2, (int)RenderLayer::RENDER_LAYER_TOP, ParticleEffect::PARTICLE_CREDIT_STROBE, mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs);
        }
        if (aCreditsReanim->ShouldTriggerTimedEvent(aFrameFactor * 332.75f))
        {
            mApp->AddTodParticle(678.0f, 352.0f, (int)RenderLayer::RENDER_LAYER_TOP, ParticleEffect::PARTICLE_MELONSPLASH, mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs);
        }
        if ((aCreditsReanim->TimedEventCount(336, aFrameCount) != 0))
        {
            TodParticleSystem* aParticle = nullptr;
            while (mApp->mEffectSystem->mParticleHolder->mParticleSystems.IterateNext(aParticle))
            {
                if (aParticle->mEffectType == ParticleEffect::PARTICLE_MELONSPLASH)
                {
                    aParticle->ParticleSystemDie();
                }
            }
        }
        if ((aCreditsReanim->TimedEventCount(342, aFrameCount) != 0))
        {
            mApp->AddTodParticle(BOARD_WIDTH / 2, BOARD_HEIGHT / 2, (int)RenderLayer::RENDER_LAYER_TOP, ParticleEffect::PARTICLE_CREDIT_STROBE, mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs);
        }

        int aBackground2Index = aCreditsReanim->FindTrackIndex(ReanimTrackId::Background2);
        ReanimatorTransform aTransformBackground2;
        aCreditsReanim->GetCurrentTransform(aBackground2Index, &aTransformBackground2);

        float aFogPosX = aTransformBackground2.mTransX + 856.0f;
        if ((aCreditsReanim->TimedEventCount(188, aFrameCount) != 0))
        {
            TodParticleSystem* aFogParticle = mApp->AddTodParticle(aFogPosX, 230.0f, (int)RenderLayer::RENDER_LAYER_TOP, ParticleEffect::PARTICLE_CREDITS_FOG, mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs);
            mFogParticleID = mApp->ParticleGetID(aFogParticle);
        }
        TodParticleSystem* aFogParticle = mApp->ParticleTryToGet(mFogParticleID);
        if (aFogParticle)
        {
            if ((aCreditsReanim->TimedEventCount(248, aFrameCount) != 0))
            {
                aFogParticle->ParticleSystemDie();
                mFogParticleID = ParticleSystemID::PARTICLESYSTEMID_NULL;
            }
            else
            {
                aFogParticle->SystemMove(aFogPosX, 230.0f);
            }
        }
    }
    if (mCreditsPhase == CreditsPhase::CREDITS_MAIN3)
    {
        if ((aCreditsReanim->TimedEventCount(65, aFrameCount) != 0))
        {
            mApp->PlayFoley(FoleyType::FOLEY_DOLPHIN_APPEARS);
        }
        if ((aCreditsReanim->TimedEventCount(111, aFrameCount) != 0) ||
            (aCreditsReanim->TimedEventCount(115, aFrameCount) != 0) ||
            (aCreditsReanim->TimedEventCount(119, aFrameCount) != 0) ||
            (aCreditsReanim->TimedEventCount(121, aFrameCount) != 0) ||
            (aCreditsReanim->TimedEventCount(123, aFrameCount) != 0) ||
            (aCreditsReanim->TimedEventCount(219, aFrameCount) != 0) ||
            (aCreditsReanim->TimedEventCount(223, aFrameCount) != 0) ||
            (aCreditsReanim->TimedEventCount(227, aFrameCount) != 0) ||
            (aCreditsReanim->TimedEventCount(231, aFrameCount) != 0) ||
            (aCreditsReanim->TimedEventCount(235, aFrameCount) != 0) ||
            (aCreditsReanim->TimedEventCount(239, aFrameCount) != 0) ||
            (aCreditsReanim->TimedEventCount(243, aFrameCount) != 0) ||
            (aCreditsReanim->TimedEventCount(247, aFrameCount) != 0))
        {
            mApp->AddTodParticle(BOARD_WIDTH / 2, BOARD_HEIGHT / 2, (int)RenderLayer::RENDER_LAYER_TOP, ParticleEffect::PARTICLE_CREDIT_STROBE, mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs);
        }
    }

    float aFraction;
    CreditsTiming* aBeforeTiming;
    CreditsTiming* aAfterTiming;
    GetTiming(&aBeforeTiming, &aAfterTiming, &aFraction);

    int aSunflowerFrame = 0;
    mDrawBrain = false;
    if (aBeforeTiming && aAfterTiming)
    {
        // C 形态 -> lambda 表 kBrainMoveCases（上方）：aAfterTiming->mBrainType
        // 键控（枚举值序 0..5，越界守卫；NEXT_WORD/OFF 空槽）。
        BrainMoveFn aBrainMove = nullptr;
        const unsigned anAfterBrainIndex = (unsigned)aAfterTiming->mBrainType;
        if (anAfterBrainIndex < sizeof(kBrainMoveCases) / sizeof(kBrainMoveCases[0]))
        {
            aBrainMove = kBrainMoveCases[anAfterBrainIndex];
        }
        if (aBrainMove != nullptr)
        {
            aBrainMove(*this, aBeforeTiming, aAfterTiming, aFraction);
        }
        // 表外特例：原第 5 支判别式切换到 aBeforeTiming->mBrainType（混排），
        // 仅在 after 支未命中时判定，整支保留。
        else if (aBeforeTiming->mBrainType == CreditBrainType::BRAIN_FLY_ON ||
            aBeforeTiming->mBrainType == CreditBrainType::BRAIN_FAST_ON ||
            aBeforeTiming->mBrainType == CreditBrainType::BRAIN_NEXT_WORD)
        {
            mBrainPosX = TodAnimateCurveFloat(0.0f, 1.0f, aFraction, aBeforeTiming->mWordX - 15.0f, aAfterTiming->mWordX - 15.0f, Curves::EaseInOut);
            mBrainPosY = TodAnimateCurveFloat(0.0f, 1.0f, aFraction, 505.0f, 485.0f, Curves::BounceFastMiddle);
            mDrawBrain = true;
        }

        float aFramesForWord = aAfterTiming->mFrame - aBeforeTiming->mFrame;
        float aFramesTillEnd = (1.0f - aFraction) * aFramesForWord;

        // B 形态 -> 常量表 kWordPhonemeFrames（原 6 支链中 4 支音素映射合并；
        // 行序 = CreditWordType 枚举序 0..4，count_last=末值 WORD_OFF 锚定，
        // OFF 槽 0 = 原默认不动）。前两支时序闭口（!= WORD_OFF 且起始/收尾
        // 窗口，混排判别式）表外保留。
        static constexpr uint8_t kWordPhonemeFrames[(int)CreditWordType::WORD_OFF + 1] = {
            2,  // 0 WORD_AA
            3,  // 1 WORD_EE
            4,  // 2 WORD_AW
            5,  // 3 WORD_OH
            0,  // 4 WORD_OFF（原默认）
        };
        static_assert(sizeof(kWordPhonemeFrames) / sizeof(kWordPhonemeFrames[0]) ==
            (int)CreditWordType::WORD_OFF + 1, "kWordPhonemeFrames 槽位须与 CreditWordType 枚举同步");

        if (aBeforeTiming->mWordType != CreditWordType::WORD_OFF && aFramesForWord * aFraction < 0.2f)
        {
            aSunflowerFrame = 1;
        }
        else if (aBeforeTiming->mWordType != CreditWordType::WORD_OFF && aFramesTillEnd < 0.4f)
        {
            aSunflowerFrame = 1;
        }
        else if ((unsigned)aBeforeTiming->mWordType <= (unsigned)CreditWordType::WORD_OFF)
        {
            aSunflowerFrame = kWordPhonemeFrames[(int)aBeforeTiming->mWordType];
        }
    }

    Reanimation* aSunFlowerReanim = FindSubReanim(aCreditsReanim, ReanimationType::REANIM_SUNFLOWER);
    if (aSunFlowerReanim)
    {
        if (mCreditsPhase == CreditsPhase::CREDITS_MAIN3 && (static_cast<double>(aCreditsReanim->SamplePlayback().phaseNs) / aCreditsReanim->mPeriodNs) > aFrameFactor * 255.0f)
        {
            aSunFlowerReanim->SetVariantOption(ReanimTrackId::anim_idle, ReanimOptions::SunFlower::anim_idle::SUNFLOWER_HEAD_WINK);
            Reanimation* aStageReanim = FindSubReanim(aCreditsReanim, ReanimationType::REANIM_CREDITS_STAGE);
            if (aSunFlowerReanim->mAnimRate != 0.0f)
            {
                aSunFlowerReanim->Seek(aSunFlowerReanim->mLastFrameNs, aSunFlowerReanim->mPeriodNs);
                aSunFlowerReanim->ApplyRate(0.0f);
            }
            if (aStageReanim && aStageReanim->mAnimRate != 0.0f)
            {
                aStageReanim->Seek(aStageReanim->mLastFrameNs, aStageReanim->mPeriodNs);
                aStageReanim->ApplyRate(0.0f);
            }
        }
        else
        {
            const auto aHeadAppearance = static_cast<unsigned>(aSunflowerFrame) < 6u
                ? kSunflowerHeadAppearance[aSunflowerFrame] : 0u;
            aSunFlowerReanim->SetVariantOption(ReanimTrackId::anim_idle, aHeadAppearance);
        }
    }
}

//0x437F20
void CreditScreen::TurnOffTongues(Reanimation* theReanim, int aParentTrack)
{
    for (int aTrackIndex = 0; aTrackIndex < theReanim->mDefinition->mTracks.count; aTrackIndex++)
    {
        ReanimatorTrackInstance* aTrackInstance = &theReanim->mTrackInstances[aTrackIndex];
        if (theReanim->mReanimationType == ReanimationType::REANIM_ZOMBIE_CREDITS_DANCE &&
            aParentTrack % 4 != 1 && theReanim->GetTrackId(aTrackIndex) == ReanimTrackId::anim_tongue)
        {
            aTrackInstance->mRenderGroup = RENDER_GROUP_HIDDEN;
        }

        Reanimation* aAttachedReanim = FindReanimAttachment(aTrackInstance->mAttachmentID);
        if (aAttachedReanim)
        {
            TurnOffTongues(aAttachedReanim, aTrackIndex);
        }
    }
}

//0x438010
void CreditScreen::JumpToFrame(CreditsPhase thePhase, float theFrame)
{
    mMainMenuButton->SetVisible(false);
    mReplayButton->SetVisible(false);
    mCreditsPhaseCounter = 0;
    mApp->mEffectSystem->EffectSystemFreeAll();

    Reanimation* aReanim;
    if (thePhase == CreditsPhase::CREDITS_END)
    {
        aReanim = PlayReanim(3);
    }
    else
    {
        aReanim = PlayReanim((int)thePhase + 1);
    }

    float aFrameFactor = 1.0f / (aReanim->mDefinition->mTracks.tracks->mTransforms.count - 1);
    int aMusicOffset = theFrame * 12142.0f;
    int aJumpMilliseconds = theFrame * 1000.0f / 7.0f;

    // 原版降阈值梯保留（对拍 2026-10-03：kMusicOffsetLadder 线性配对查表
    // -31~-37% 回退；字幕影片结构固定、改版不会加新梯级，不适用"可扩展
    // 键空间保留表形态"豁免，按 >10% 回退规则恢复原链）
    if (thePhase == CreditsPhase::CREDITS_MAIN1)
    {
        if (theFrame >= 368.0f)
        {
            aMusicOffset = 12142.0f * (theFrame - 368.0f) + 4634474.0f;
        }
        else if (theFrame >= 340.0f)
        {
            aMusicOffset = 12142.0f * (theFrame - 340.0f) + 4280738.0f;
        }
        else if (theFrame >= 304.0f)
        {
            aMusicOffset = 12142.0f * (theFrame - 304.0f) + 3825710.0f;
        }
        else if (theFrame >= 272.0f)
        {
            aMusicOffset = 12142.0f * (theFrame - 272.0f) + 3421764.0f;
        }
        else if (theFrame >= 144.0f)
        {
            aMusicOffset = 12142.0f * (theFrame - 144.0f) + 1805688.0f;
        }
        else if (theFrame >= 128.0)
        {
            aMusicOffset = 12142.0f * (theFrame - 128.0f) + 1603662.0f;
        }
    }
    else if (thePhase == CreditsPhase::CREDITS_MAIN2)
    {
        if (theFrame >= 320.0f)
        {
            aMusicOffset = 12142.0f * (theFrame - 320.0f) + 9069118.0f;
            aJumpMilliseconds += 57142;
        }
        else if (theFrame >= 248.0f)
        {
            aMusicOffset = 12142.0f * (theFrame - 248.0f) + 8159850.0f;
            aJumpMilliseconds += 57142;
        }
        else if (theFrame >= 188.0f)
        {
            aMusicOffset = 12142.0f * (theFrame - 188.0f) + 7401454.0f;
            aJumpMilliseconds += 57142;
        }
        else if (theFrame >= 124.0f)
        {
            aMusicOffset = 12142.0f * (theFrame - 124.0f) + 6593548.0f;
            aJumpMilliseconds += 57142;
        }
        else
        {
            aMusicOffset = 12142.0f * theFrame + 5026370.0f;
            aJumpMilliseconds += 57142;
        }
    }
    else if (thePhase == CreditsPhase::CREDITS_MAIN3)
    {
        if (theFrame >= 240.0f)
        {
            aMusicOffset = 12142.0f * (theFrame - 240.0f) + 12897822.0f;
            aJumpMilliseconds += 112000;
        }
        else if (theFrame >= 216.0f)
        {
            aMusicOffset = 12142.0f * (theFrame - 216.0f) + 12594510.0f;
            aJumpMilliseconds += 112000;
        }
        else if (theFrame >= 124.0f)
        {
            aMusicOffset = 12142.0f * (theFrame - 124.0f) + 11434414.0f;
            aJumpMilliseconds += 112000;
        }
        else
        {
            aMusicOffset = 12142.0f * theFrame + 9864866.0f;
            aJumpMilliseconds += 112000;
        }
    }
    else if (thePhase == CreditsPhase::CREDITS_END)
    {
        aMusicOffset = 14047138;
        aJumpMilliseconds += 159142;
    }

    mApp->mMusic->PlayFromOffset(MusicFile::MUSIC_FILE_CREDITS_ZOMBIES_ON_YOUR_LAWN, aMusicOffset - 900, 1.0f);
    if (thePhase == CreditsPhase::CREDITS_END)
    {
        aReanim->Seek(1000);
    }
    else
    {
        aReanim->Seek(theFrame, aReanim->mDefinition->mTracks.tracks->mTransforms.count - 1);
    }

    mCreditsPhase = thePhase;
    mTimerSinceStart.SetStartTime(aJumpMilliseconds);
}

void CreditScreen::KeyChar(SexyChar theChar)
{
    if (mCreditsPaused || !mApp->mDebugKeysEnabled)
        return;

    // 原版跳帧键链保留（对拍 2026-10-03：kKeyJumpRows 配对查表 -22~-25%
    // 回退；字幕影片按键集固定、改版不会扩，按 >10% 回退规则恢复原链）
    if (theChar == ('1'))
    {
        CreditScreen::JumpToFrame(CreditsPhase::CREDITS_MAIN1, 0.0f);
    }
    else if (theChar == ('2'))
    {
        CreditScreen::JumpToFrame(CreditsPhase::CREDITS_MAIN1, 128.0f);
    }
    else if (theChar == ('3'))
    {
        CreditScreen::JumpToFrame(CreditsPhase::CREDITS_MAIN1, 144.0f);
    }
    else if (theChar == ('4'))
    {
        CreditScreen::JumpToFrame(CreditsPhase::CREDITS_MAIN1, 272.0f);
    }
    else if (theChar == ('5'))
    {
        CreditScreen::JumpToFrame(CreditsPhase::CREDITS_MAIN1, 304.0f);
    }
    else if (theChar == ('6'))
    {
        CreditScreen::JumpToFrame(CreditsPhase::CREDITS_MAIN1, 340.0f);
    }
    else if (theChar == ('7'))
    {
        CreditScreen::JumpToFrame(CreditsPhase::CREDITS_MAIN1, 368.0f);
    }
    else if (theChar == ('q'))
    {
        CreditScreen::JumpToFrame(CreditsPhase::CREDITS_MAIN2, 0.0f);
    }
    else if (theChar == ('w'))
    {
        CreditScreen::JumpToFrame(CreditsPhase::CREDITS_MAIN2, 124.0f);
    }
    else if (theChar == ('e'))
    {
        CreditScreen::JumpToFrame(CreditsPhase::CREDITS_MAIN2, 188.0f);
    }
    else if (theChar == ('r'))
    {
        CreditScreen::JumpToFrame(CreditsPhase::CREDITS_MAIN2, 248.0f);
    }
    else if (theChar == ('t'))
    {
        CreditScreen::JumpToFrame(CreditsPhase::CREDITS_MAIN2, 320.0f);
    }
    else if (theChar == ('a'))
    {
        CreditScreen::JumpToFrame(CreditsPhase::CREDITS_MAIN3, 0.0f);
    }
    else if (theChar == ('s'))
    {
        CreditScreen::JumpToFrame(CreditsPhase::CREDITS_MAIN3, 124.0f);
    }
    else if (theChar == ('d'))
    {
        CreditScreen::JumpToFrame(CreditsPhase::CREDITS_MAIN3, 216.0f);
    }
    else if (theChar == ('f'))
    {
        CreditScreen::JumpToFrame(CreditsPhase::CREDITS_MAIN3, 240.0f);
    }
    else if (theChar == ('g'))
    {
        CreditScreen::JumpToFrame(CreditsPhase::CREDITS_MAIN3, 324.0f);
    }
    else if (theChar == ('n'))
    {
        mDontSync = !mDontSync;
    }
}

//0x438430
void CreditScreen::PauseCredits()
{
    if (mCreditsPaused || mApp->mCreditScreen != this)
        return;

    WidgetLifetimeHold aPauseLifetime(*this);
    mApp->mSoundSystem->StopFoley(FoleyType::FOLEY_SCREAM);
    mApp->PlaySample(SOUND_PAUSE);
    mCreditsPaused = true;
    int aDurationOnPause = mTimerSinceStart.GetDuration();
    mApp->mMusic->GameMusicPause(true);

    const int aResult = mApp->LawnMessageBox(
        Dialogs::DIALOG_MESSAGE, 
        gStr.CREDITS_PAUSE_HEADER.c_str(), 
        gStr.CREDITS_PAUSE_BODY.c_str(), 
        gStr.CREDITS_RESUME_BUTTON.c_str(), 
        gStr.MAIN_MENU_BUTTON.c_str(), 
        Dialog::BUTTONS_YES_NO);

    // The modal wait pumps updates. Keep this caller alive, but never let its
    // result close a replacement credits screen or resume a new scene's music.
    if (mApp->mCreditScreen != this)
        return;

    if (aResult == Dialog::ID_NO)
    {
        mApp->KillCreditScreen();
        mApp->DoBackToMain();
        return; // DoBackToMain starts the menu's own music state.
    }

    mCreditsPaused = false;
    mApp->mMusic->GameMusicPause(false);
    mTimerSinceStart.SetStartTime(aDurationOnPause);
}

//0x438530
void CreditScreen::KeyDown(KeyCode theKey)
{
    if (mCreditsPaused)
        return;

    if (theKey == KeyCode::KEYCODE_SPACE || theKey == KeyCode::KEYCODE_RETURN || theKey == KeyCode::KEYCODE_ESCAPE)
    {
        PauseCredits();
    }
}

//0x438560
void CreditScreen::ButtonPress(int theId)
{
    if (theId == CreditScreen::Credits_Button_MainMenu)
    {
        mApp->PlaySample(SOUND_GRAVEBUTTON);
    }
    else if (theId == CreditScreen::Credits_Button_Replay)
    {
        mApp->PlaySample(SOUND_TAP);
    }
}

//0x4385A0
void CreditScreen::ButtonDepress(int theId)
{
    if (theId == CreditScreen::Credits_Button_MainMenu)
    {
        mApp->KillCreditScreen();
        mApp->DoBackToMain();
    }
    else if (theId == CreditScreen::Credits_Button_Replay)
    {
        mApp->KillCreditScreen();
        mApp->ShowCreditScreen();
    }
}

void CreditScreen::MouseUp(int, int, int){}
