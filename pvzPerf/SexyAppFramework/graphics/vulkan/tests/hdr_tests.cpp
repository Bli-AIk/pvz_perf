#include "VulkanRenderer.h"
#include "TestWin32Window.h"
#include "VulkanSurfaceFormat.h"
#include "hdr_reference.h"
#include <atomic>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>
#include <filesystem>
#include <string>

namespace Sexy {
const char* GetLogFilePathC(const char* name) {
    static thread_local std::string path;
    std::filesystem::create_directories("hdr-test-output");
    path=std::string("hdr-test-output/")+name;
    return path.c_str();
}
}
namespace {
std::atomic_uint errors=0;
void Require(bool ok,const char* why) {if(!ok) throw std::runtime_error(why);}
void Check(VkResult r) {if(r!=VK_SUCCESS) throw std::runtime_error("Vulkan result="+std::to_string(r));}
VkBool32 VKAPI_CALL Validation(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT types,const VkDebugUtilsMessengerCallbackDataEXT* data,void* user) {
    if(severity>=VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ++errors;
    return VulkanInitConfig::DefaultDebugCallback(severity,types,data,user);
}
template<class F> void MustReject(F&& f) {
    bool rejected=false;
    try { f(); } catch(const std::runtime_error&) {rejected=true;} catch(const std::invalid_argument&) {rejected=true;}
    Require(rejected,"Invalid encoding/parameter was accepted");
}
uint32_t MemoryType(uint32_t mask,VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties props;
    vkGetPhysicalDeviceMemoryProperties(VulkanContext::physicalDevice,&props);
    for(uint32_t i=0;i<props.memoryTypeCount;++i)
        if((mask&(1u<<i)) && (props.memoryTypes[i].propertyFlags&flags)==flags) return i;
    throw std::runtime_error("No memory type");
}
struct Buffer {
    VkDevice d=VulkanContext::device;
    VkBuffer buffer{};
    VkDeviceMemory memory{};
    void* mapped{};
    Buffer(VkDeviceSize bytes,VkBufferUsageFlags usage) {
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size=bytes; info.usage=usage;
        Check(vkCreateBuffer(d,&info,nullptr,&buffer));
        VkMemoryRequirements requirements;
        vkGetBufferMemoryRequirements(d,buffer,&requirements);
        VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        alloc.allocationSize=requirements.size;
        alloc.memoryTypeIndex=MemoryType(requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        Check(vkAllocateMemory(d,&alloc,nullptr,&memory));
        Check(vkBindBufferMemory(d,buffer,memory,0));
        Check(vkMapMemory(d,memory,0,bytes,0,&mapped));
    }
    ~Buffer() {
        if(mapped) vkUnmapMemory(d,memory);
        if(buffer) vkDestroyBuffer(d,buffer,nullptr);
        if(memory) vkFreeMemory(d,memory,nullptr);
    }
};

struct Probe {
    VkDevice d=VulkanContext::device;
    VkDescriptorSetLayout setLayout{};
    VkPipelineLayout layout{}; VkPipeline pipeline{}; VkShaderModule shader{};
    VkDescriptorPool pool{}; VkCommandPool commands{}; VkFence fence{};
    ~Probe() {
        vkDeviceWaitIdle(d);
        if(fence) vkDestroyFence(d,fence,nullptr);
        if(commands) vkDestroyCommandPool(d,commands,nullptr);
        if(pool) vkDestroyDescriptorPool(d,pool,nullptr);
        if(pipeline) vkDestroyPipeline(d,pipeline,nullptr);
        if(layout) vkDestroyPipelineLayout(d,layout,nullptr);
        if(setLayout) vkDestroyDescriptorSetLayout(d,setLayout,nullptr);
        if(shader) vkDestroyShaderModule(d,shader,nullptr);
    }
};
void MathProbe(VulkanContext& window,bool linear=false) {
    using namespace Sexy;
    using RGBA=std::array<float,4>;
    std::vector<RGBA> input;
    for(int i=0;i<256;++i) {
        float c=i/255.0f;
        input.push_back({c,c,c,float(i%2)});
        input.push_back({c,0,0,1});input.push_back({0,c,0,1});input.push_back({0,0,c,1});
        input.push_back({c,1-c,.37f,.1f});
    }
    for(float c:{-.1f,0.f,.0404499f,.04045f,.0404501f,.5f,1.f,1.1f})
        input.push_back({c,c,c,1});
    for(float linear:{.2999999f,.3f,.3000001f,.5999999f,.6f,.6000001f,.8999999f,.9f,.9000001f}) {
        float c=1.055f*std::pow(linear,1.f/2.4f)-.055f;
        input.push_back({c,c,c,1});
    }
    const VkDeviceSize bytes=input.size()*sizeof(RGBA);
    Buffer source(bytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),dest(bytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    std::memcpy(source.mapped,input.data(),size_t(bytes));
    Probe r;
    VkDescriptorSetLayoutBinding bindings[]={{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},
        {1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}};
    VkDescriptorSetLayoutCreateInfo setInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    setInfo.bindingCount=2;setInfo.pBindings=bindings;
    Check(vkCreateDescriptorSetLayout(r.d,&setInfo,nullptr,&r.setLayout));
    VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT,0,32};
    VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layout.setLayoutCount=1;layout.pSetLayouts=&r.setLayout;layout.pushConstantRangeCount=1;layout.pPushConstantRanges=&push;
    Check(vkCreatePipelineLayout(r.d,&layout,nullptr,&r.layout));
    std::ifstream file(std::string(PVZ_NATIVE_SHADER_DIR)+(linear?"/hdr_linear_probe.spv":"/hdr_probe.spv"),std::ios::binary|std::ios::ate);
    Require(bool(file),"Missing HDR probe SPIR-V");
    const auto count=file.tellg();
    Require(count>0 && count%4==0,"Invalid HDR probe SPIR-V");
    std::vector<uint32_t> words(size_t(count)/4);
    file.seekg(0);Require(bool(file.read(reinterpret_cast<char*>(words.data()),count)),"Cannot read SPIR-V");
    VkShaderModuleCreateInfo shader{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    shader.codeSize=size_t(count);shader.pCode=words.data();
    Check(vkCreateShaderModule(r.d,&shader,nullptr,&r.shader));
    VkComputePipelineCreateInfo pipeline{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pipeline.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_COMPUTE_BIT,r.shader,"main"};
    pipeline.layout=r.layout;
    Check(vkCreateComputePipelines(r.d,VK_NULL_HANDLE,1,&pipeline,nullptr,&r.pipeline));
    VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,2};
    VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool.maxSets=1;pool.poolSizeCount=1;pool.pPoolSizes=&size;
    Check(vkCreateDescriptorPool(r.d,&pool,nullptr,&r.pool));
    VkDescriptorSetAllocateInfo alloc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    alloc.descriptorPool=r.pool;alloc.descriptorSetCount=1;alloc.pSetLayouts=&r.setLayout;
    VkDescriptorSet set{};Check(vkAllocateDescriptorSets(r.d,&alloc,&set));
    VkDescriptorBufferInfo infos[]={{source.buffer,0,bytes},{dest.buffer,0,bytes}};
    VkWriteDescriptorSet writes[2]{};
    for(uint32_t i=0;i<2;++i) {
        writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[i].dstSet=set;
        writes[i].dstBinding=i;writes[i].descriptorCount=1;
        writes[i].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;writes[i].pBufferInfo=&infos[i];
    }
    vkUpdateDescriptorSets(r.d,2,writes,0,nullptr);
    VkCommandPoolCreateInfo cmdPool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cmdPool.queueFamilyIndex=window.graphicsFamily;Check(vkCreateCommandPool(r.d,&cmdPool,nullptr,&r.commands));
    VkCommandBufferAllocateInfo cmdAlloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cmdAlloc.commandPool=r.commands;cmdAlloc.commandBufferCount=1;cmdAlloc.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    VkCommandBuffer cmd{};Check(vkAllocateCommandBuffers(r.d,&cmdAlloc,&cmd));
    VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};Check(vkCreateFence(r.d,&fence,nullptr,&r.fence));
    double maximumError=0;size_t values=0;
    for(float white:{80.f,200.f,480.f}) for(float strength:{0.f,.5f,1.f}) for(float peak:{0.f,250.f,1000.f,4000.f}) {
        const auto params=VulkanHdrDisplayInfo{true,white,std::max(white,peak),0,0}.Params(strength);
        Check(vkResetCommandPool(r.d,r.commands,0));Check(vkResetFences(r.d,1,&r.fence));
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        Check(vkBeginCommandBuffer(cmd,&begin));
        vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,r.pipeline);
        vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,r.layout,0,1,&set,0,nullptr);
        const float pushData[]={0,0,0,0,params.paperWhiteNits,params.peakNits,params.reinhardPaperNits,params.strength};
        vkCmdPushConstants(cmd,r.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(pushData),pushData);
        vkCmdDispatch(cmd,uint32_t(input.size()),1,1);
        VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        host.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);
        Check(vkEndCommandBuffer(cmd));
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&cmd;
        Check(vkQueueSubmit(window.graphicsQueue,1,&submit,r.fence));
        Check(vkWaitForFences(r.d,1,&r.fence,VK_TRUE,UINT64_MAX));
        const auto* actual=static_cast<const RGBA*>(dest.mapped);
        for(size_t i=0;i<input.size();++i) {
            const auto expected=HdrReference::Apply({input[i][0],input[i][1],input[i][2]},params,linear);
            for(int j=0;j<3;++j) {
                const double error=std::abs(double(actual[i][j])-expected[j]);
                maximumError=std::max(maximumError,error);++values;
                if(!std::isfinite(actual[i][j]) || error>(linear?.001:.0001)) {
                    std::cerr<<"HDR mismatch sample="<<i<<" channel="<<j<<" actual="<<actual[i][j]<<" expected="<<expected[j]<<" white="<<white<<" strength="<<strength<<'\n';
                    throw std::runtime_error("HDR GPU/reference mismatch");
                }
            }
            Require(actual[i][3]==1,"HDR output alpha must be opaque");
        }
    }
    std::cout<<"PASS HDR equations "<<(linear?"linear scRGB":"PQ")<<": "<<values<<" channels, maximum error="<<maximumError<<"\n";
}
std::vector<uint32_t> DrawSolid(Sexy::VulkanRenderer& renderer) {
    Require(renderer.BeginScene(),"Hidden WSI acquire failed");
    const Sexy::VulkanVertex quad[]={{0,0,1,.5f,0,1,0,0},{800,0,1,.5f,0,1,1,0},{800,600,1,.5f,0,1,1,1},
        {0,0,1,.5f,0,1,0,0},{800,600,1,.5f,0,1,1,1},{0,600,1,.5f,0,1,0,1}};
    renderer.SubmitTriangles(quad,renderer.whiteTexture,false,false,Sexy::VulkanBlend::Opaque);
    renderer.EndSceneAndPresent();
    return renderer.ReadScenePixels();
}
void Contracts() {
    const VkSurfaceFormatKHR sdr[]={{VK_FORMAT_B8G8R8A8_UNORM,VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
    MustReject([&]{ChooseVulkanSurfaceFormat(sdr,VK_FORMAT_A2B10G10R10_UNORM_PACK32,VK_COLOR_SPACE_HDR10_ST2084_EXT);});
    MustReject([&]{ChooseVulkanSurfaceFormat(sdr,VK_FORMAT_B8G8R8A8_SRGB,VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);});
    MustReject([&]{ChooseVulkanSurfaceFormat({},VK_FORMAT_B8G8R8A8_UNORM,VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);});
    Require(ChooseVulkanSurfaceFormat(sdr,VK_FORMAT_R8G8B8A8_UNORM,VK_COLOR_SPACE_SRGB_NONLINEAR_KHR).format==sdr[0].format,"RGBA memory order substitution");
    const Sexy::VulkanHdrDisplayInfo hdr{true,200,1000,0,0};
    Require(hdr.Params(1).reinhardPaperNits==255,"Legacy white-level mapping");
    MustReject([&]{auto p=hdr.Params(1);p.strength=std::numeric_limits<float>::quiet_NaN();p.Validate();});
    MustReject([&]{auto p=hdr.Params(1);p.reinhardPaperNits=1000;p.Validate();});
    MustReject([&]{Sexy::VulkanHdrDisplayInfo{false,200,1000,0,0}.Params(1);});
    std::cout<<"PASS strict color-space/format, finite HDR parameters and legacy SDR-white mapping\n";
}
}
int main() {
    try {
        Contracts();
        VulkanInitConfig init;
        init.features.shaderFloat64=VK_TRUE;
        init.enableValidation=true;init.debugCallback=Validation;
        init.extraExtensions.push_back(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
        VulkanTest::Window native(320,240);
        auto config=VulkanTest::SwapchainConfig();
        // Reject unsupported WSI format, release the partial window/device, and
        // successfully initialize again in the same process.
        auto bad=config;bad.imageFormat=VK_FORMAT_R32G32B32A32_SFLOAT;
        MustReject([&]{VulkanContext invalid(init,native.hwnd,bad);});
        Require(VulkanContext::device==VK_NULL_HANDLE,"Failed context leaked its device");
        Sexy::VulkanHdrDisplayInfo display{};
        std::vector<uint32_t> sdrScene;
        {
            VulkanContext window(init,native.hwnd,config);
            MathProbe(window);
            MathProbe(window,true);
            Sexy::VulkanRenderer renderer(window,800,600,std::string(PVZ_NATIVE_SHADER_DIR)+"/game_vs.spv",
                std::string(PVZ_NATIVE_SHADER_DIR)+"/game_ps.spv");
            renderer.PrepareLoadingPipelines();
            bool rejected=false;
            try {renderer.SetHdrParams({200,1000,255,1});}catch(const std::logic_error&) {rejected=true;}
            Require(rejected,"SDR renderer silently accepted HDR metadata");
            sdrScene=DrawSolid(renderer);
            display=Sexy::QueryVulkanHdrDisplay(MonitorFromWindow(native.hwnd,MONITOR_DEFAULTTONEAREST));
            std::cout<<"OS monitor HDR="<<display.hdr<<" white="<<display.paperWhiteNits<<" peak="<<display.peakNits
                <<" min="<<display.minimumNits<<" full-frame="<<display.fullFrameNits<<'\n';
        }
        if(display.hdr) {
            config.imageFormat=VK_FORMAT_R16G16B16A16_SFLOAT;
            config.imageColorSpace=VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
            VulkanContext window(init,native.hwnd,config);
            const auto params=display.Params(1);
            Sexy::VulkanRenderer renderer(window,800,600,std::string(PVZ_NATIVE_SHADER_DIR)+"/game_vs.spv",
                std::string(PVZ_NATIVE_SHADER_DIR)+"/game_ps.spv",&params);
            renderer.PrepareLoadingPipelines();
            const auto identity=window.hwnd;
            auto generation=window.swapchainGeneration;
            const auto* white=renderer.whiteTexture;
            for(int frame=0;frame<8;++frame) {
                if(frame==4) {native.Resize(641,361);renderer.ResizePresent();generation=window.swapchainGeneration;}
                auto changed=params;
                changed.paperWhiteNits=frame%2?160.f:240.f;
                changed.peakNits=1000.f;
                renderer.SetHdrParams(changed);
                Require(renderer.HdrParams()==changed,"Renderer retained stale HDR monitor parameters");
                auto bad=changed;bad.paperWhiteNits=std::numeric_limits<float>::quiet_NaN();
                MustReject([&]{renderer.SetHdrParams(bad);});
                Require(renderer.HdrParams()==changed,"Invalid metadata partially overwrote active HDR state");
                // Compare with the same UNORM draw on an SDR WSI, not a guessed
                // .5 -> 128 quantization (interpolation can land below the tie).
                Require(DrawSolid(renderer)==sdrScene,"HDR changed the SDR game scene");
                Require(window.hwnd==identity && window.swapchainGeneration==generation && renderer.whiteTexture==white,
                    "HDR metadata update rebuilt window/swapchain/textures");
            }
            Require(renderer.BeginScene(),"Cannot test HDR update during recording");
            bool rejected=false;
            try {renderer.SetHdrParams(params);}catch(const std::logic_error&) {rejected=true;}
            Require(rejected,"HDR state mutated during recording");
            renderer.EndSceneAndPresent();
            std::cout<<"PASS live HDR constants: 8 metadata changes, invalid/in-frame rejection, no resource rebuild\n";
            std::cout<<"PASS hidden linear scRGB WSI/fragment shader, 8 frames, resize, SDR scene unchanged\n";
        } else std::cout<<"SKIP real HDR WSI: desktop HDR is off; GPU math/SDR rejection tests still ran\n";
        Require(errors.load()==0,"Vulkan validation errors");
        std::cout<<"PASS HDR tests; zero validation errors\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
