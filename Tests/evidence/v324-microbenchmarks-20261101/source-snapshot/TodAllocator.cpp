#include "TodAllocator.h"
#include "TodDebug.h"
#include "TodCommon.h"
#include "misc/Debug.h"
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>

static int CheckedAllocatorBlockBytes(int theGrowCount, int theItemSize)
{
	if (theGrowCount <= 0 || theItemSize < static_cast<int>(sizeof(void*)))
		throw std::invalid_argument("TodAllocator: invalid growth count or item size");
	const int aLimit = std::numeric_limits<int>::max() - static_cast<int>(sizeof(void*));
	if (theItemSize > aLimit / theGrowCount)
		throw std::length_error("TodAllocator: block exceeds TodMalloc's signed size range");
	return theGrowCount * theItemSize + static_cast<int>(sizeof(void*));
}

static void* ReadAllocatorLink(const void* theAddress)
{
	void* aLink;
	std::memcpy(&aLink, theAddress, sizeof(aLink));
	return aLink;
}

static void WriteAllocatorLink(void* theAddress, void* theLink)
{
	// Do not require a byte-stride allocator's every item to be pointer-aligned.
	std::memcpy(theAddress, &theLink, sizeof(theLink));
}

void TodAllocator::Initialize(int theGrowCount, int theItemSize)
{
	CheckedAllocatorBlockBytes(theGrowCount, theItemSize);
	if (mBlockList || mFreeList || mTotalItems)
		throw std::logic_error("TodAllocator: release existing blocks before initialization");

	mFreeList = nullptr;
	mBlockList = nullptr;
	mGrowCount = theGrowCount;
	mTotalItems = 0;
	mItemSize = theItemSize;
}

void TodAllocator::Dispose()
{
	FreeAll();
}

//0x4438C0
void TodAllocator::Grow()
{
	const int aBytes = CheckedAllocatorBlockBytes(mGrowCount, mItemSize);
	void* aBlock = TodMalloc(aBytes);
	if (!aBlock)
		throw std::bad_alloc();
	WriteAllocatorLink(aBlock, mBlockList);

	void* aFreeList = mFreeList;
	auto* aItem = static_cast<unsigned char*>(aBlock) + sizeof(void*);
	for (int i = 0; i < mGrowCount; i++)
	{
		WriteAllocatorLink(aItem, aFreeList);
		aFreeList = aItem;
		aItem += mItemSize;
	}
	mBlockList = aBlock;
	mFreeList = aFreeList;
}

bool TodAllocator::IsPointerFromAllocator(void* theItem)
{
	if (!theItem || mGrowCount <= 0 || mItemSize < static_cast<int>(sizeof(void*)))
		return false;
	if (static_cast<size_t>(mGrowCount) > std::numeric_limits<size_t>::max() / static_cast<size_t>(mItemSize))
		return false;
	const size_t aBlockSize = static_cast<size_t>(mGrowCount) * mItemSize;
	for (void* aPtr = mBlockList; aPtr != nullptr; aPtr = ReadAllocatorLink(aPtr))
	{
		uintptr_t aItemPtr = (uintptr_t)theItem;
		// 区块的首个四字节为额外申请的、用于存储指向下一区块的指针的区域
		uintptr_t aBlockPtr = (uintptr_t)aPtr + sizeof(void*);
		// 判断 theItem 是否位于当前区块内且指向某一项的区域的起始地址
		if (aItemPtr >= aBlockPtr && aItemPtr - aBlockPtr < aBlockSize && (aItemPtr - aBlockPtr) % mItemSize == 0)
			return true;
	}
	return false;
}

bool TodAllocator::IsPointerOnFreeList(void* theItem)
{
	for (void* aPtr = mFreeList; aPtr != nullptr; aPtr = ReadAllocatorLink(aPtr))
		if (theItem == aPtr)
			return true;
	return false;
}

void* TodAllocator::Alloc(int theItemSize)
{
	if (theItemSize != mItemSize || theItemSize < static_cast<int>(sizeof(void*)))
		throw std::invalid_argument("TodAllocator: allocation size does not match the pool");
	if (mTotalItems < 0 || mTotalItems == std::numeric_limits<int>::max())
		throw std::length_error("TodAllocator: live item count overflow");
	if (mFreeList == nullptr)
		Grow();

	void* anItem = (void*)mFreeList;
	mFreeList = ReadAllocatorLink(anItem);
	++mTotalItems;
	return anItem;
}

void* TodAllocator::Calloc(int theItemSize)
{
	void* anItem = Alloc(theItemSize);
	memset(anItem, 0, theItemSize);
	return anItem;
}

void TodAllocator::Free(void* theItem, int theItemSize)
{
	if (theItemSize != mItemSize || theItemSize < static_cast<int>(sizeof(void*)))
		throw std::invalid_argument("TodAllocator: release size does not match the pool");
	if (!theItem || mTotalItems <= 0)
		throw std::logic_error("TodAllocator: invalid release or live count underflow");
	TOD_ASSERT(IsPointerFromAllocator(theItem));
	TOD_ASSERT(!IsPointerOnFreeList(theItem));
	WriteAllocatorLink(theItem, mFreeList);
	mFreeList = theItem;  // 将 theItem 设为新的可用区域头
	--mTotalItems;
}

void TodAllocator::FreeAll()
{
	for (void* aBlock = mBlockList; aBlock != nullptr; )
	{
		void* aNext = ReadAllocatorLink(aBlock);
		TodFree(aBlock);
		aBlock = aNext;
	}

	mBlockList = nullptr;
	mFreeList = nullptr;
	mTotalItems = 0;
}
