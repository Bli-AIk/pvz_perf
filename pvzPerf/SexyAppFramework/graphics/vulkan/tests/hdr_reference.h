#pragma once
#include "VulkanHdr.h"
#include <array>
namespace HdrReference {
using RGB=std::array<double,3>;
RGB Apply(RGB signal,const Sexy::VulkanHdrParams& params,bool linear=false);
}
