#include "VulkanLegacy.h"
#include "VulkanContext.h"
#include <array>
#include <stdexcept>

namespace {
VkPipelineStageFlags Stage(VkPipelineStageFlags2 flags, bool source) {
    if (!flags) return source ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    VkPipelineStageFlags result = static_cast<VkPipelineStageFlags>(flags);
    if (flags & (VK_PIPELINE_STAGE_2_COPY_BIT | VK_PIPELINE_STAGE_2_BLIT_BIT |
                 VK_PIPELINE_STAGE_2_RESOLVE_BIT | VK_PIPELINE_STAGE_2_CLEAR_BIT))
        result |= VK_PIPELINE_STAGE_TRANSFER_BIT;
    if (flags & (VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT | VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT))
        result |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    return result;
}

VkAccessFlags Access(VkAccessFlags2 flags) {
    VkAccessFlags result = static_cast<VkAccessFlags>(flags);
    if (flags & (VK_ACCESS_2_SHADER_SAMPLED_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT))
        result |= VK_ACCESS_SHADER_READ_BIT;
    if (flags & VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT) result |= VK_ACCESS_SHADER_WRITE_BIT;
    return result;
}

void VKAPI_CALL Barrier(VkCommandBuffer cmd, const VkDependencyInfo* info) {
    for (unsigned int i=0;i<info->memoryBarrierCount;++i) {
        const auto& in=info->pMemoryBarriers[i];
        const VkMemoryBarrier out{VK_STRUCTURE_TYPE_MEMORY_BARRIER,nullptr,Access(in.srcAccessMask),Access(in.dstAccessMask)};
        vkCmdPipelineBarrier(cmd,Stage(in.srcStageMask,true),Stage(in.dstStageMask,false),
            info->dependencyFlags,1,&out,0,nullptr,0,nullptr);
    }
    for (unsigned int i=0;i<info->bufferMemoryBarrierCount;++i) {
        const auto& in=info->pBufferMemoryBarriers[i];
        const VkBufferMemoryBarrier out{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,nullptr,
            Access(in.srcAccessMask),Access(in.dstAccessMask),in.srcQueueFamilyIndex,in.dstQueueFamilyIndex,
            in.buffer,in.offset,in.size};
        vkCmdPipelineBarrier(cmd,Stage(in.srcStageMask,true),Stage(in.dstStageMask,false),
            info->dependencyFlags,0,nullptr,1,&out,0,nullptr);
    }
    for (unsigned int i=0;i<info->imageMemoryBarrierCount;++i) {
        const auto& in=info->pImageMemoryBarriers[i];
        const VkImageMemoryBarrier out{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,nullptr,
            Access(in.srcAccessMask),Access(in.dstAccessMask),in.oldLayout,in.newLayout,
            in.srcQueueFamilyIndex,in.dstQueueFamilyIndex,in.image,in.subresourceRange};
        vkCmdPipelineBarrier(cmd,Stage(in.srcStageMask,true),Stage(in.dstStageMask,false),
            info->dependencyFlags,0,nullptr,0,nullptr,1,&out);
    }
}

VkResult VKAPI_CALL Submit(VkQueue queue, unsigned int count, const VkSubmitInfo2* submits, VkFence fence) {
    if (!count) return vkQueueSubmit(queue,0,nullptr,fence);
    for (unsigned int n=0;n<count;++n) {
        const auto& in=submits[n];
        std::array<VkSemaphore,16> wait{},signal{};
        std::array<VkPipelineStageFlags,16> stages{};
        std::array<VkCommandBuffer,16> commands{};
        if (in.waitSemaphoreInfoCount>wait.size() || in.signalSemaphoreInfoCount>signal.size() ||
            in.commandBufferInfoCount>commands.size())
            return VK_ERROR_INITIALIZATION_FAILED;
        for (unsigned int i=0;i<in.waitSemaphoreInfoCount;++i) {
            if (in.pWaitSemaphoreInfos[i].value) return VK_ERROR_FEATURE_NOT_PRESENT;
            wait[i]=in.pWaitSemaphoreInfos[i].semaphore;
            stages[i]=Stage(in.pWaitSemaphoreInfos[i].stageMask,false);
        }
        for (unsigned int i=0;i<in.signalSemaphoreInfoCount;++i) {
            if (in.pSignalSemaphoreInfos[i].value) return VK_ERROR_FEATURE_NOT_PRESENT;
            signal[i]=in.pSignalSemaphoreInfos[i].semaphore;
        }
        for (unsigned int i=0;i<in.commandBufferInfoCount;++i)
            commands[i]=in.pCommandBufferInfos[i].commandBuffer;
        const VkSubmitInfo out{VK_STRUCTURE_TYPE_SUBMIT_INFO,nullptr,in.waitSemaphoreInfoCount,
            wait.data(),stages.data(),in.commandBufferInfoCount,commands.data(),in.signalSemaphoreInfoCount,signal.data()};
        const auto result=vkQueueSubmit(queue,1,&out,n+1==count?fence:VK_NULL_HANDLE);
        if (result!=VK_SUCCESS) return result;
    }
    return VK_SUCCESS;
}
}

void InstallVulkan10Commands() {
    VulkanContext::queueSubmit2=Submit;
    VulkanContext::pipelineBarrier2=Barrier;
}
