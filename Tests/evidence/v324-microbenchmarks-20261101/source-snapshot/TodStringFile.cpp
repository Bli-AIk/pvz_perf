#include "graphics/Graphics.h"
#include "TodDebug.h"
#include "TodCommon.h"
#include "TodStringFile.h"
#include "generated/EntityStrings.h"  // 动态键实体表（阶段 4）
#include "paklib/PakInterface.h"
#include "graphics/Font.h"
#include <string_view>

int gTodStringFormatCount;               //[0x69DE4C]
TodStringListFormat* gTodStringFormats;  //[0x69DA34]

const int gLawnStringFormatCount = 12;
TodStringListFormat gLawnStringFormats[12] = {    //0x6A5010 // GOTY @Patoke: 0x7248EC
	{ StringFormatId::Normal,          nullptr,    Color(40,   50,     90,     255),       0,      0U },
	{ StringFormatId::Flavor,          nullptr,    Color(143,  67,     27,     255),       0,      1U },
	{ StringFormatId::Keyword,         nullptr,    Color(143,  67,     27,     255),       0,      0U },
	{ StringFormatId::Nocturnal,       nullptr,    Color(136,  50,     170,    255),       0,      0U },
	{ StringFormatId::Aquatic,         nullptr,    Color(11,   161,    219,    255),       0,      0U },
	{ StringFormatId::Stat,            nullptr,    Color(204,  36,     29,     255),       0,      0U },
	{ StringFormatId::Metal,           nullptr,    Color(204,  36,     29,     255),       0,      2U },
	{ StringFormatId::KeyMetal,        nullptr,    Color(143,  67,     27,     255),       0,      2U },
	{ StringFormatId::ShortLine,       nullptr,    Color(0,    0,      0,      0),         -9,     0U },
	{ StringFormatId::ExtraShortLine,  nullptr,    Color(0,    0,      0,      0),         -14,    0U },
	{ StringFormatId::Credits1,        nullptr,    Color(0,    0,      0,      0),         3,      0U },
	{ StringFormatId::Credits2,        nullptr,    Color(0,    0,      0,      0),         2,      0U } // @Patoke: wrong size (2 duplicates)
};

TodStringListFormat::TodStringListFormat()
{
	mFormatId = StringFormatId::None;
	mNewFont = nullptr;
	mLineSpacingOffset = 0;
	mFormatFlags = 0U;
}

TodStringListFormat::TodStringListFormat(StringFormatId theFormatId, _Font** theFont, const Color& theColor, int theLineSpacingOffset, unsigned int theFormatFlags) :
	mFormatId(theFormatId), mNewFont(theFont), mNewColor(theColor), mLineSpacingOffset(theLineSpacingOffset), mFormatFlags(theFormatFlags)
{
}

//0x518E40

//0x518F60
[[maybe_unused]] static void TodStringRemoveReturnChars(std::string& theString)
{
	for (size_t i = 0; i < theString.size(); )
	{
		if (theString[i] == '\r')
			theString.replace(i, 1, "", 0);  // 原版中此处的“1”和“""”已内联至函数内部
		else
			i++;
	}
}

//0x518FB0

//0x519080

//0x519240

//0x519390

// 0x519410/0x519520/0x5195D0/0x5196C0（TodStringListFind/
// TodStringTranslate×2/TodStringListExists）已随 properties 字符串表退役
// 删除（删除验证阶段 4）：全库调用点改走生成实体表
// generated/EntityStrings.h 的 EntityStringFind/EntityStringExists/
// 保留为独立可用的 properties 装载器，但游戏启动不再调用。

//0x5197B0
// GOTY @Patoke: 0x523E20
static void TodWriteStringSetFormat(StringFormatId theFormat, TodStringListFormat& theCurrentFormat)
{
	if (theFormat == StringFormatId::None)
		return;
	for (int i = 0; i < gTodStringFormatCount; i++)
	{
		const TodStringListFormat& aFormat = gTodStringFormats[i];
		// Ordered textual prefix matching is resolved by the offline compiler.
		if (theFormat == aFormat.mFormatId)
		{
			if (aFormat.mNewFont != nullptr)
				theCurrentFormat.mNewFont = aFormat.mNewFont;
			if (aFormat.mNewColor != Color(0, 0, 0, 0))
				theCurrentFormat.mNewColor = aFormat.mNewColor;
			theCurrentFormat.mLineSpacingOffset = aFormat.mLineSpacingOffset;
			theCurrentFormat.mFormatFlags = aFormat.mFormatFlags;
			return;
		}
	}
}

static bool CharIsSpaceInFormat(char theChar, const TodStringListFormat& theCurrentFormat)
{
	return theChar == ' ' || (TestBit(theCurrentFormat.mFormatFlags, TodStringFormatFlag::TOD_FORMAT_IGNORE_NEWLINES) && theChar == '\n');
}

//0x519870
int TodWriteString(Graphics* g, const SexyString& theString, int theX, int theY, TodStringListFormat& theCurrentFormat, int theWidth, DrawStringJustification theJustification, bool drawString, int theOffset, int theLength)
{
	_Font* aFont = *theCurrentFormat.mNewFont;
	if (drawString)  // 如果需要实际绘制
	{
		// Measuring inline tags must not change the format used for the
		// actual first run. Left alignment needs no preparatory width scan.
		const auto aMeasureSpareX = [&]()
		{
			TodStringListFormat aMeasureFormat = theCurrentFormat;
			return theWidth - TodWriteString(g, theString, theX, theY, aMeasureFormat,
				theWidth, DrawStringJustification::DS_ALIGN_LEFT, false, theOffset, theLength);
		};
		// B 形态 -> 除数表：6 枚举值三对（LEFT=0 不测量、RIGHT=1 全宽、CENTER=2 半宽）。
		// aMeasureSpareX 按引用捕获局部，保留在原位不动——数据表消分支，lambda 零改动。
		static constexpr uint8_t aJustifyDivisor[6] = {
		    0,  // DS_ALIGN_LEFT
		    1,  // DS_ALIGN_RIGHT
		    2,  // DS_ALIGN_CENTER
		    0,  // DS_ALIGN_LEFT_VERTICAL_MIDDLE
		    1,  // DS_ALIGN_RIGHT_VERTICAL_MIDDLE
		    2,  // DS_ALIGN_CENTER_VERTICAL_MIDDLE
		};
		const unsigned aJustifyIndex = static_cast<unsigned>(theJustification);
		if (aJustifyIndex < 6)
		{
			const int aDiv = aJustifyDivisor[aJustifyIndex];
			if (aDiv > 0)
			{
				theX += aMeasureSpareX() / aDiv;
			}
		}
	}

	if (theLength < 0 || theOffset + theLength > (int)theString.size())
		theLength = theString.size();
	else
		theLength = theOffset + theLength;  // 将 theLength 更改为子串结束位置

	SexyString aString;
	int aXOffset = 0;
	bool aPrevCharWasSpace = false;
	for (int i = theOffset; i < theLength; i++)
	{
		StringFormatId aFormatId = StringFormatId::None;
		const bool aNumericControl = TryReadUiTextControl(theString, i, aFormatId);
		if (aNumericControl || theString[i] == '{')
		{
			// User-provided complete brace tags remain zero-width no-ops. Named
			// format resolution exists only in the offline UI text compiler.
			const auto aBraceEnd = aNumericControl ? SexyString::npos : theString.find('}', i + 1);
			if (aNumericControl || aBraceEnd != SexyString::npos)
			{
				i += aNumericControl ? kUiTextControlSize - 1 : aBraceEnd - i;
				if (drawString)
					aFont->DrawString(g, theX + aXOffset, theY, aString, theCurrentFormat.mNewColor, g->mClipRect);

				aXOffset += aFont->StringWidth(aString);
				aString.assign("");
				TodWriteStringSetFormat(aFormatId, theCurrentFormat);
				aFont = *theCurrentFormat.mNewFont;
			}
		}
		else
		{
			if (TestBit(theCurrentFormat.mFormatFlags, TodStringFormatFlag::TOD_FORMAT_IGNORE_NEWLINES))  // 如果将换行符视作空格
			{
				if (CharIsSpaceInFormat(theString[i], theCurrentFormat))  // 如果当前字符是空格
				{
					if (!aPrevCharWasSpace)  // 如果前一个字符不是空格
					{
						aString.append(1, ' ');  // 积攒一个空格
						aPrevCharWasSpace = true;
					}
					continue;
				}
				else
					aPrevCharWasSpace = false;  // 确保字符串中至多只能连续出现 1 个空格字符
			}

			aString.append(1, theString[i]);
		}
	}

	if (drawString)  // 如果需要实际绘制
		aFont->DrawString(g, theX + aXOffset, theY, aString, theCurrentFormat.mNewColor, g->mClipRect);  // 将已经积攒的字符进行绘制
	return aXOffset + aFont->StringWidth(aString);
}

int TodWriteWordWrappedHelper(Graphics* g, const SexyString& theString, int theX, int theY, TodStringListFormat& theCurrentFormat, int theWidth, DrawStringJustification theJustification, bool drawString, int theOffset, int theLength, int theMaxChars)
{
	if (theOffset + theLength > theMaxChars)  // 如果指定子串超出了字符串的最大长度
	{
		theLength = theMaxChars - theOffset;  // 修正子串长度
		if (theLength <= 0)
			return -1;
	}
	return TodWriteString(g, theString, theX, theY, theCurrentFormat, theWidth, theJustification, drawString, theOffset, theLength);
}

//0x519B50
// GOTY @Patoke: 0x5241C0
int TodDrawStringWrappedHelper(Graphics* g, const SexyString& theText, const Rect& theRect, _Font* theFont, const Color& theColor, DrawStringJustification theJustification, bool drawString)
{
	int theMaxChars = theText.size();
	TodStringListFormat aCurrentFormat;
	aCurrentFormat.mNewFont = &theFont;
	aCurrentFormat.mNewColor = theColor;
	aCurrentFormat.mFormatId = StringFormatId::None;
	aCurrentFormat.mLineSpacingOffset = 0;
	aCurrentFormat.mFormatFlags = 0U;

	int aYOffset = theFont->mAscent - theFont->mAscentPadding;
	int aLineSpacing = theFont->GetLineSpacing() + aCurrentFormat.mLineSpacingOffset;
	SexyString aCurString;
	size_t aLineFeedPos = 0;
	size_t aCurPos = 0;
	int aCurWidth = 0;
	SexyChar aCurChar = '\0';
	SexyChar aPrevChar = '\0';
	int aSpacePos = -1;
	int aMaxWidth = 0;
	while (aCurPos < theText.size())
	{
		aCurChar = theText[aCurPos];
		StringFormatId aFormatId = StringFormatId::None;
		const bool aNumericControl = TryReadUiTextControl(theText, aCurPos, aFormatId);
		if (aNumericControl || aCurChar == '{')
		{
			const auto aBraceEnd = aNumericControl ? SexyString::npos : theText.find('}', aCurPos + 1);
			if (aNumericControl || aBraceEnd != SexyString::npos)
			{
				aCurPos = aNumericControl ? aCurPos + kUiTextControlSize : aBraceEnd + 1;
				int aOldAscentOffset = (*aCurrentFormat.mNewFont)->mAscent - (*aCurrentFormat.mNewFont)->mAscentPadding;
				Color aExistingColor = aCurrentFormat.mNewColor;
				TodWriteStringSetFormat(aFormatId, aCurrentFormat);
				aCurrentFormat.mNewColor = aExistingColor;
				int aNewAscentOffset = (*aCurrentFormat.mNewFont)->mAscent - (*aCurrentFormat.mNewFont)->mAscentPadding;
				aLineSpacing = (*aCurrentFormat.mNewFont)->GetLineSpacing() + aCurrentFormat.mLineSpacingOffset;
				aYOffset += aNewAscentOffset - aOldAscentOffset;
				continue;
			}
		}
		else if (CharIsSpaceInFormat(aCurChar, aCurrentFormat))
		{
			aSpacePos = aCurPos;
			aCurChar = ' ';
		}
		else if (aCurChar == '\n')
		{
			aSpacePos = aCurPos;
			aCurWidth = theRect.mWidth + 1;
			aCurPos++;
		}

		aCurWidth += (*aCurrentFormat.mNewFont)->CharWidthKern(aCurChar, aPrevChar);  // 当前宽度加上当前字符的宽度
		aPrevChar = aCurChar;
		if (aCurWidth > theRect.mWidth)  // 如果当前宽度超出了限制区域的宽度，则进行换行的处理
		{
			int aLineWidth;
			if (aSpacePos != -1)  // 如果本行前面的字符中存在空格字符
			{
				if (drawString && (int)g->mTransY + theRect.mY + aYOffset >= g->mClipRect.mY &&
					(int)g->mTransY + theRect.mY + aYOffset <= g->mClipRect.mY + g->mClipRect.mHeight + aLineSpacing)
				{
					TodWriteWordWrappedHelper(
						g,
						theText,
						theRect.mX,
						theRect.mY + aYOffset,
						aCurrentFormat,
						theRect.mWidth,
						theJustification,
						drawString,
						aLineFeedPos, // 上次换行的位置即为新行开始的位置
						aSpacePos - aLineFeedPos, // 绘制部分为从上次换行的位置开始至本行空格字符之前的文本
						theMaxChars
					);  // 绘制新一行的文本（若需要）
				}

				aLineWidth = aCurWidth;
				if (aLineWidth < 0)  // 如果本行字符总宽度小于 0
					break;

				aCurPos = aSpacePos + 1;  // 将 aCurPos 移至下一行的开始处
				if (aCurChar != '\n')
					while (aCurPos < theText.size() && CharIsSpaceInFormat(theText[aCurPos], aCurrentFormat))
						aCurPos++;  // aCurPos 跳过所有连续的空白字符
			}
			else
			{
				if (aCurPos < aLineFeedPos + 1)
					aCurPos++;  // 确保每行至少有 1 个字符

				aLineWidth = TodWriteWordWrappedHelper(
					g,
					theText,
					theRect.mX,
					theRect.mY + aYOffset,
					aCurrentFormat,
					theRect.mWidth,
					theJustification,
					drawString,
					aLineFeedPos, // 上次换行的位置即为新行开始的位置
					aCurPos - aLineFeedPos, // 绘制部分为从上次换行的位置开始至当前位置的文本
					theMaxChars
				);  // 绘制新一行的文本（若需要）
				if (aLineWidth < 0)  // 如果本行字符总宽度小于 0
					break;
			}

			if (aLineWidth > aMaxWidth)
				aMaxWidth = aLineWidth;  // 更新最大行宽度
			aYOffset += aLineSpacing;
			aLineFeedPos = aCurPos;  // 记录当前位置为“上次换行的位置”
			aSpacePos = -1;
			aCurWidth = 0;
			aPrevChar = '\0';
		}
		else  // 当前宽度未超过限制区域宽度时
		{
			aCurPos++;  // 继续下一个字符
		}
	}

	if (aLineFeedPos < theText.size())
	{
		int aLastLineLength = TodWriteWordWrappedHelper(
			g,
			theText,
			theRect.mX,
			theRect.mY + aYOffset,
			aCurrentFormat,
			theRect.mWidth,
			theJustification,
			drawString,
			aLineFeedPos, // 上次换行的位置即为最后一行开始的位置
			theText.size() - aLineFeedPos, // 绘制部分为从上次换行的位置开始的所有剩余文本
			theMaxChars
		);  // 绘制最后一行的文本
		if (aLastLineLength >= 0)
			aYOffset += aLineSpacing;
	}
	else
		aYOffset += aLineSpacing;

	return (*aCurrentFormat.mNewFont)->GetDescent() + aYOffset - aLineSpacing;
}

//0x51A040
// GOTY @Patoke: 0x5246A0
void TodDrawStringWrapped(Graphics* g, const SexyString& theText, const Rect& theRect, _Font* theFont, const Color& theColor, DrawStringJustification theJustification)
{
	const SexyString& aTextFinal = theText;
	Rect aRectTodUse = theRect;
	if (theJustification == DrawStringJustification::DS_ALIGN_LEFT_VERTICAL_MIDDLE ||
		theJustification == DrawStringJustification::DS_ALIGN_RIGHT_VERTICAL_MIDDLE ||
		theJustification == DrawStringJustification::DS_ALIGN_CENTER_VERTICAL_MIDDLE)  // 如果纵向需要居中
	{
		aRectTodUse.mY += (aRectTodUse.mHeight - TodDrawStringWrappedHelper(g, aTextFinal, aRectTodUse, theFont, theColor, theJustification, false)) / 2;
	}
	TodDrawStringWrappedHelper(g, aTextFinal, aRectTodUse, theFont, theColor, theJustification, true);
}
