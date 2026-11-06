#include "WebGPUShaders.h"
namespace Sexy::WebGPUShaders
{
std::string_view Scene()
{
    return R"WGSL(
// Matches Vulkan game_ps.frag specialization: no per-fragment target-state branch.
struct Target { extent: vec2f };
override premultipliedOutput: bool = false;
@group(0) @binding(0) var<uniform> outputTarget: Target;
@group(1) @binding(0) var image: texture_2d<f32>;
@group(1) @binding(1) var imageSampler: sampler;
struct Vertex {
    @builtin(position) position: vec4f,
    @location(0) color: vec4f,
    @location(1) uv: vec2f,
    @location(2) @interpolate(flat) flags: u32,
    @location(3) @interpolate(flat) sampling: u32,
};
@vertex fn vertex(@location(0) p: vec2f, @location(1) color: vec4f,
    @location(2) uv: vec2f, @location(3) flags: u32, @location(4) sampling: u32) -> Vertex {
    var out: Vertex;
    out.position = vec4f(p.x / outputTarget.extent.x * 2.0 - 1.0, 1.0 - p.y / outputTarget.extent.y * 2.0, 0.0, 1.0);
    out.color = color; out.uv = uv; out.flags = flags; out.sampling = sampling;
    return out;
}
@fragment fn sprite(in: Vertex) -> @location(0) vec4f {
    var value = textureSample(image, imageSampler, in.uv);
    if ((in.flags & 0x04000000u) != 0u) { value.a = 1.0; }
    return value * in.color;
}
// Same unchanged-hue closed form as Vulkan game_ps.frag; avoid HSL sextant reconstruction.
fn filterRgb(rgb: vec3f, kind: u32) -> vec3f {
    if (kind == 0u) { return rgb; } if (kind == 1u) { return vec3f(1.0); }
    let hi=max(max(rgb.r,rgb.g),rgb.b);let lo=min(min(rgb.r,rgb.g),rgb.b);
    let oldL=(lo+hi)*0.5;if(oldL<=0.0){return vec3f(0);}
    let newL=oldL*select(1.2,1.8,kind==2u);let saturation=select(0.3,0.2,kind==2u);
    let denominator=select(1.0-oldL,oldL,oldL<=0.5);
    let numerator=select(1.0-newL,newL,newL<=0.5);
    var ratio=0.0;if(denominator!=0.0){ratio=saturation*numerator/denominator;}
    let value=newL+(hi-lo)*ratio*0.5;if(value<=0.0){return vec3f(0);}
    let result=(rgb-vec3f(oldL))*ratio+vec3f(newL);
    return floor(clamp(result*255.0,vec3f(0),vec3f(255)))/255.0;
}
fn addressed(p: vec2i, sampling: u32) -> vec2i {
    let size = vec2i(textureDimensions(image));
    if ((sampling & 2u) != 0u) { return ((p % size) + size) % size; }
    return clamp(p, vec2i(0), size - 1);
}
fn filtered(value: vec4f, flags: u32, straight: bool) -> vec4f {
    var out = value; let generated = (flags & 0x80000000u) != 0u;
    let kind = (flags >> 28u) & 3u;
    if (kind != 0u) {
        var rgb = out.rgb;
        if (generated) { rgb = vec3f(0); if (out.a > 0.0) { rgb = out.rgb / out.a; } }
        out = vec4f(filterRgb(rgb, kind) * select(out.a, 1.0, straight), out.a);
    } else if (generated && straight) {
        var rgb = vec3f(0); if (out.a > 0.0) { rgb = out.rgb / out.a; } out = vec4f(rgb, out.a);
    } else if (!generated && !straight) { out = vec4f(out.rgb * out.a, out.a); }
    return out;
}
fn texel(p: vec2i, flags: u32, sampling: u32, straight: bool) -> vec4f {
    let pos = addressed(p, sampling); var value = textureLoad(image, pos, 0);
    if ((flags & 0x04000000u) != 0u) { value.a = 1.0; }
    if ((flags & 0x88000000u) == 0x08000000u && value.a == 0.0) {
        var sum = vec3u(0); var count = 0u;
        for (var dy = -1; dy <= 1; dy += 2) { for (var dx = -1; dx <= 1; dx++) {
            let n = pos + vec2i(dx,dy);
            if (all(n >= vec2i(0)) && all(n < vec2i(textureDimensions(image)))) {
                let next = textureLoad(image,n,0);
                if (next.a != 0.0) { sum += vec3u(round(clamp(next.rgb,vec3f(0),vec3f(1))*255.0)); count++; }
            }
        }}
        value = vec4f(vec3f(sum / max(count,1u)) / 255.0, value.a);
    }
    return filtered(value, flags, straight);
}
fn sampleFiltered(in: Vertex, straight: bool) -> vec4f {
    let size = vec2f(textureDimensions(image));
    if ((in.sampling & 1u) == 0u) { return texel(vec2i(floor(in.uv*size)),in.flags,in.sampling,straight); }
    let p = in.uv*size-0.5; let base = vec2i(floor(p)); let f = fract(p);
    let a = mix(texel(base,in.flags,in.sampling,straight),texel(base+vec2i(1,0),in.flags,in.sampling,straight),f.x);
    let b = mix(texel(base+vec2i(0,1),in.flags,in.sampling,straight),texel(base+vec2i(1,1),in.flags,in.sampling,straight),f.x);
    return mix(a,b,f.y);
}
@fragment fn coverage(in: Vertex) -> @location(0) vec4f {
    let source=textureSample(image,imageSampler,in.uv);
    let cover=u32(round(clamp(source.r,0.0,1.0)*255.0));
    let alpha=u32(round(clamp(in.color.a,0.0,1.0)*255.0));
    let a=f32(((cover+1u)*alpha)>>8u)/255.0;
    if(premultipliedOutput){return vec4f(in.color.rgb*a,a);}
    return vec4f(in.color.rgb,a);
}
@fragment fn scene(in: Vertex) -> @location(0) vec4f {
    // Vulkan samples ordinary/generated images through the hardware unit once.
    // Manual taps are reserved for operations which transform each texel before interpolation.
    var source=textureSample(image,imageSampler,in.uv);
    if((in.flags&0x04000000u)!=0u){source.a=1.0;}
    let generated=(in.flags&0x80000000u)!=0u;
    let independent=(in.flags&0x40000000u)!=0u;
    let linear=(in.sampling&1u)!=0u;
    let kind=(in.flags>>28u)&3u;
    let sand=(in.flags&0x88000000u)==0x08000000u;
    if(premultipliedOutput){
        if(kind!=0u||sand){
            if(linear||sand){source=sampleFiltered(in,independent);}
            else{source=filtered(source,in.flags,independent);}
            if(independent){source=vec4f(source.rgb*source.a,source.a);}
        }else if(independent){
            if(generated){
                if(linear){source=sampleFiltered(in,true);}
                else{var rgb=vec3f(0);if(source.a>0.0){rgb=source.rgb/source.a;}source=vec4f(rgb,source.a);}
            }
            source=vec4f(source.rgb*source.a,source.a);
        }else if(!generated&&linear){source=sampleFiltered(in,false);}
        else if(!generated){source=vec4f(source.rgb*source.a,source.a);}
        return vec4f(source.rgb*in.color.rgb*in.color.a,source.a*in.color.a);
    }
    let filteredPremult=!generated&&kind!=0u&&!independent;
    if(kind!=0u||sand){
        if(linear||sand){source=sampleFiltered(in,!generated&&!filteredPremult);}
        else{source=filtered(source,in.flags,!generated&&!filteredPremult);}
    }
    if(generated||filteredPremult){
        var rgb=vec3f(0);if(source.a>0.0){rgb=source.rgb/source.a;}source=vec4f(rgb,source.a);
    }
    return source*in.color;
}

)WGSL";
}
std::string_view Mixed()
{
    return R"WGSL(
struct MixedColor { @location(0) @blend_src(0) color: vec4f, @location(0) @blend_src(1) control: vec4f };
@fragment fn mixedSprite(in: Vertex) -> MixedColor {
    var source = textureSample(image,imageSampler,in.uv);
    if ((in.flags & 0x04000000u) != 0u) { source.a = 1.0; }
    let c = source * in.color;
    var result: MixedColor;
    result.color = vec4f(c.rgb*c.a,c.a);
    result.control = vec4f(0.0,0.0,0.0,select(c.a,0.0,(in.flags & 0x02000000u)!=0u));
    return result;
}
)WGSL";
}
std::string_view Image()
{
    return R"WGSL(
struct Params {
    origin: vec2i, operation: u32, color: u32,
    tint: vec4u, flags: u32, colorOnly: u32, u0: u32, u1: u32,
    v1: u32, pad0: u32, pad1: u32, pad2: u32,
};
@group(0) @binding(0) var<uniform> p: Params;
@group(0) @binding(1) var image: texture_2d<f32>;
@group(0) @binding(2) var alphaImage: texture_2d<f32>;
@vertex fn vertex(@builtin(vertex_index) id: u32) -> @builtin(position) vec4f {
    let uv = vec2f(f32((id << 1u)&2u),f32(id&2u));
    return vec4f(uv*2.0-1.0,0.0,1.0);
}
fn blue(u: u32, v: u32) -> u32 { return u32(round(textureLoad(image,vec2i(i32(u&255u),i32(v&255u)),0).b*255.0)); }
fn lookup(u: u32, v: u32) -> u32 {
    let f1 = ((u & 65535u) & 65534u)+1u; let g1 = ((v & 65535u) & 65534u)+1u;
    let f0 = 65536u-f1; let g0 = 65536u-g1; let x = u>>16u; let y = v>>16u;
    return ((f0*g1/65536u)*blue(x,y+1u)/65536u)+((f1*g1/65536u)*blue(x+1u,y+1u)/65536u)+
        ((f0*g0/65536u)*blue(x,y)/65536u)+((f1*g0/65536u)*blue(x+1u,y)/65536u);
}
@fragment fn fragment(@builtin(position) position: vec4f) -> @location(0) vec4f {
    let xy = vec2i(position.xy);
    if (p.operation == 3u) {
        let x = u32(xy.x)<<17u; let y = u32(xy.y);
        let a1 = lookup(x+p.u1,((256u-y)<<17u)+p.v1)&255u;
        let a0 = lookup(x+p.u0,y<<17u)&255u; let a = (a0+a1)/2u;
        var alpha = 0u;
        if (a >= 160u) { alpha = 255u-2u*(a-160u); } else if (a >= 128u) { alpha = 5u*(a-128u); }
        return vec4f(1.0,1.0,1.0,f32(alpha/3u)/255.0);
    }
    if (p.operation == 2u) {
        var value = textureLoad(image,xy,0); let a = u32(round(textureLoad(alphaImage,xy,0).b*255.0));
        if (p.colorOnly != 0u) {
            let color = p.color | (a<<24u);
            return vec4f(f32((color>>16u)&255u),f32((color>>8u)&255u),f32(color&255u),f32(color>>24u))/255.0;
        }
        value.a = f32(a)/255.0; return value;
    }
    let pos = xy+p.origin;
    if (any(pos < vec2i(0)) || any(pos >= vec2i(textureDimensions(image)))) { return vec4f(0); }
    var value = textureLoad(image,pos,0);
    if ((p.flags & 0x80000000u) != 0u) {
        var rgb = vec3f(0); if (value.a > 0.0) { rgb = value.rgb/value.a; } value = vec4f(rgb,value.a);
    }
    let bytes = vec4u(round(clamp(value,vec4f(0),vec4f(1))*255.0));
    if (p.operation == 0u) { return vec4f(bytes)/255.0; }
    var result = min(bytes*p.tint/255u,vec4u(255));
    if (all(p.tint <= vec4u(255))) { result = ((bytes*p.tint)>>vec4u(8))&vec4u(255); }
    return vec4f(result)/255.0;
}
)WGSL";
}
std::string_view Font()
{
    return R"WGSL(
struct Params { src: vec4i, dst: vec4i, flags: u32, sampling: u32, forceWhite: u32, pad: u32, offsets: vec4u };
@group(0) @binding(0) var<uniform> p: Params;
@group(0) @binding(1) var image: texture_2d<f32>;
@group(0) @binding(2) var<storage,read> weights: array<u32>;
@vertex fn vertex(@builtin(vertex_index) id: u32) -> @builtin(position) vec4f {
    let uv=vec2f(f32((id<<1u)&2u),f32(id&2u)); return vec4f(uv*2.0-1.0,0.0,1.0);
}
fn raw(pos: vec2i) -> vec4u {
    var v=textureLoad(image,clamp(pos,vec2i(0),vec2i(textureDimensions(image))-1),0);
    if ((p.flags&0x04000000u)!=0u) { v.a=1.0; }
    if ((p.flags&0x80000000u)!=0u) { var rgb=vec3f(0); if(v.a>0.0){rgb=v.rgb/v.a;} v=vec4f(rgb,v.a); }
    return vec4u(round(clamp(v,vec4f(0),vec4f(1))*255.0));
}
fn pixel(pos: vec2i) -> vec4u {
    var v=raw(pos);
    if ((p.sampling&0x08000000u)!=0u && v.a==0u) {
        var sum=vec3u(0); var count=0u;
        for(var dy=-1;dy<=1;dy+=2){for(var dx=-1;dx<=1;dx++){
            let n=pos+vec2i(dx,dy);
            if(all(n>=vec2i(0))&&all(n<vec2i(textureDimensions(image)))){
                let next=raw(n); if(next.a!=0u){sum+=next.rgb;count++;}
            }
        }}
        v=vec4u(sum/max(count,1u),v.a);
    }
    return v;
}
@fragment fn fragment(@builtin(position) pos: vec4f) -> @location(0) vec4f {
    let xy=vec2u(vec2i(pos.xy)-p.dst.xy);
    let xfirst=weights[p.offsets.x+xy.x];let xend=weights[p.offsets.x+xy.x+1u];
    let yfirst=weights[p.offsets.y+xy.y];let yend=weights[p.offsets.y+xy.y+1u];
    var total=vec4u(0);
    for(var y=yfirst;y<yend;y++){
        let yi=bitcast<i32>(weights[p.offsets.w+2u*y]);let yw=weights[p.offsets.w+2u*y+1u];
        var horizontal=vec4u(0);
        for(var x=xfirst;x<xend;x++){
            let xi=bitcast<i32>(weights[p.offsets.z+2u*x]);let xw=weights[p.offsets.z+2u*x+1u];
            horizontal+=pixel(p.src.xy+vec2i(xi,yi))*xw;
        }
        total+=horizontal*yw;
    }
    var result=total>>vec4u(16); var rgb=(result.rgb*result.a)>>vec3u(8);
    if(result.a==0u){rgb=vec3u(0);}
    let alpha=f32(result.a)/255.0;
    return vec4f(select(vec3f(rgb)/255.0,vec3f(1),p.forceWhite!=0u)*alpha,alpha);
}
)WGSL";
}
std::string_view Present()
{
    return R"WGSL(
@group(0) @binding(0) var image: texture_2d<f32>;
@group(0) @binding(1) var imageSampler: sampler;
struct Output { @builtin(position) position: vec4f, @location(0) uv: vec2f };
@vertex fn vertex(@builtin(vertex_index) index: u32) -> Output {
    let uv=vec2f(f32((index<<1u)&2u),f32(index&2u));
    var out: Output;
    out.position=vec4f(uv.x*2.0-1.0,1.0-uv.y*2.0,0.0,1.0);out.uv=uv;
    return out;
}
@fragment fn fragment(in: Output) -> @location(0) vec4f {
    return textureSample(image,imageSampler,in.uv);
}
)WGSL";
}
} // namespace Sexy::WebGPUShaders
