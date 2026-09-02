#include "Render/NativePostProcess.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <d3dcompiler.h>
#include <mutex>

extern void Log(const char* fmt, ...);

namespace {
using Microsoft::WRL::ComPtr;

std::mutex g_settingsMutex;
NativePostSettings g_settings{};
std::once_flag g_loadOnce;
char g_configPath[MAX_PATH]{};

float Clamp(float v, float lo, float hi) { return std::max(lo, std::min(v, hi)); }

void Normalize(NativePostSettings& s) {
    s.enabled = s.enabled ? 1 : 0;
    s.mix = Clamp(s.mix, 0.0f, 1.0f);
    s.exposure = Clamp(s.exposure, -2.0f, 2.0f);
    s.gamma = Clamp(s.gamma, 0.10f, 2.50f);
    s.saturation = Clamp(s.saturation, -1.0f, 1.0f);
    s.bleach = Clamp(s.bleach, 0.0f, 1.0f);
    s.defog = Clamp(s.defog, 0.0f, 1.0f);
    for (float& v : s.fogColor) v = Clamp(v, 0.0f, 1.0f);
    for (float& v : s.lift) v = Clamp(v, 0.0f, 2.0f);
    for (float& v : s.rgbGamma) v = Clamp(v, 0.10f, 2.0f);
    for (float& v : s.gain) v = Clamp(v, 0.0f, 2.0f);
}

void ResolveConfigPath() {
    if (g_configPath[0]) return;
    GetModuleFileNameA(nullptr, g_configPath, MAX_PATH);
    if (char* slash = std::strrchr(g_configPath, '\\')) {
        slash[1] = '\0';
        strcat_s(g_configPath, "native-post.ini");
    } else {
        strcpy_s(g_configPath, "native-post.ini");
    }
}

float ReadFloat(const char* key, float fallback) {
    char def[64]{}, text[64]{};
    sprintf_s(def, "%.6g", fallback);
    GetPrivateProfileStringA("ReShadeNative", key, def, text, sizeof(text), g_configPath);
    char* end = nullptr;
    const float value = std::strtof(text, &end);
    return end != text && std::isfinite(value) ? value : fallback;
}

void WriteFloat(const char* key, float value) {
    char text[64]{};
    sprintf_s(text, "%.6g", value);
    WritePrivateProfileStringA("ReShadeNative", key, text, g_configPath);
}

void LoadOnce() {
    ResolveConfigPath();
    NativePostSettings s{};
    s.enabled = GetPrivateProfileIntA("ReShadeNative", "Enabled", s.enabled, g_configPath) ? 1 : 0;
    s.mix = ReadFloat("Mix", s.mix);
    s.exposure = ReadFloat("Exposure", s.exposure);
    s.gamma = ReadFloat("Gamma", s.gamma);
    s.saturation = ReadFloat("Saturation", s.saturation);
    s.bleach = ReadFloat("Bleach", s.bleach);
    s.defog = ReadFloat("Defog", s.defog);
    s.fogColor[0] = ReadFloat("FogColorR", s.fogColor[0]);
    s.fogColor[1] = ReadFloat("FogColorG", s.fogColor[1]);
    s.fogColor[2] = ReadFloat("FogColorB", s.fogColor[2]);
    s.lift[0] = ReadFloat("LiftR", s.lift[0]);
    s.lift[1] = ReadFloat("LiftG", s.lift[1]);
    s.lift[2] = ReadFloat("LiftB", s.lift[2]);
    s.rgbGamma[0] = ReadFloat("GammaR", s.rgbGamma[0]);
    s.rgbGamma[1] = ReadFloat("GammaG", s.rgbGamma[1]);
    s.rgbGamma[2] = ReadFloat("GammaB", s.rgbGamma[2]);
    s.gain[0] = ReadFloat("GainR", s.gain[0]);
    s.gain[1] = ReadFloat("GainG", s.gain[1]);
    s.gain[2] = ReadFloat("GainB", s.gain[2]);
    Normalize(s);
    {
        std::lock_guard<std::mutex> lock(g_settingsMutex);
        g_settings = s;
    }
    Log("[native-post] loaded enabled=%d mix=%.3f exposure=%.3f gamma=%.3f saturation=%.3f path=%s\n",
        s.enabled, s.mix, s.exposure, s.gamma, s.saturation, g_configPath);
}

DXGI_FORMAT ViewFormat(DXGI_FORMAT f) {
    switch (f) {
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case DXGI_FORMAT_B8G8R8A8_TYPELESS:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
            return DXGI_FORMAT_B8G8R8A8_UNORM;
        default:
            return f;
    }
}

constexpr char kShader[] = R"(
cbuffer GradeCB : register(b0) {
    float mixAmount;
    float exposure;
    float tonemapGamma;
    float saturation;
    float bleach;
    float defog;
    float pad0;
    float pad1;
    float3 fogColor;
    float pad2;
    float3 rgbLift;
    float pad3;
    float3 rgbGamma;
    float pad4;
    float3 rgbGain;
    float enabled;
};
Texture2D<float4> sourceTex : register(t0);
SamplerState linearClamp : register(s0);

struct VSOut { float4 position : SV_Position; float2 uv : TEXCOORD0; };

VSOut VSMain(uint id : SV_VertexID) {
    static const float2 p[3] = { float2(-1, 1), float2(3, 1), float2(-1, -3) };
    static const float2 u[3] = { float2(0, 0), float2(2, 0), float2(0, 2) };
    VSOut o; o.position = float4(p[id], 0, 1); o.uv = u[id]; return o;
}

float3 LiftGammaGain(float3 color) {
    color = color * (1.5 - 0.5 * rgbLift) + 0.5 * rgbLift - 0.5;
    color = saturate(color);
    color *= rgbGain;
    color = pow(max(abs(color), 1e-6), 1.0 / max(rgbGamma, 0.1));
    return saturate(color);
}

// Arithmetic ported from the installed SweetFX Tonemap.fx. Saturation=0, gamma=1,
// exposure/bleach/defog=0 is exactly neutral.
float3 Tonemap(float3 color) {
    color = saturate(color - defog * fogColor * 2.55);
    color *= exp2(exposure);
    color = pow(max(color, 0.0), max(tonemapGamma, 0.1));

    const float3 lumaCoef = float3(0.2126, 0.7152, 0.0722);
    float lum = dot(lumaCoef, color);
    float L = saturate(10.0 * (lum - 0.45));
    float3 A2 = bleach * color;
    float3 result1 = 2.0 * color * lum;
    float3 result2 = 1.0 - 2.0 * (1.0 - lum) * (1.0 - color);
    float3 newColor = lerp(result1, result2, L);
    color += (1.0 - A2) * (A2 * newColor);

    float middleGray = dot(color, float3(1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0));
    float3 diff = color - middleGray;
    float3 denom = max(1.0 + diff * saturation, 1e-4);
    return saturate((color + diff * saturation) / denom);
}

float4 PSMain(VSOut i) : SV_Target {
    float4 raw = sourceTex.SampleLevel(linearClamp, i.uv, 0);
    if (enabled < 0.5 || mixAmount <= 0.0) return float4(raw.rgb, 1.0);
    float3 graded = Tonemap(LiftGammaGain(raw.rgb));
    return float4(lerp(raw.rgb, graded, saturate(mixAmount)), 1.0);
}
)";

bool Compile(const char* entry, const char* target, ComPtr<ID3DBlob>& blob) {
    ComPtr<ID3DBlob> errors;
    const HRESULT hr = D3DCompile(kShader, std::strlen(kShader), nullptr, nullptr, nullptr,
                                  entry, target, D3DCOMPILE_ENABLE_STRICTNESS, 0, &blob, &errors);
    if (FAILED(hr)) {
        Log("[native-post] shader compile failed %s/%s hr=0x%08X %.*s\n", entry, target,
            static_cast<unsigned>(hr), errors ? static_cast<int>(errors->GetBufferSize()) : 0,
            errors ? static_cast<const char*>(errors->GetBufferPointer()) : "");
        return false;
    }
    return true;
}
} // namespace

void NativePostLoadSettings() { std::call_once(g_loadOnce, LoadOnce); }

NativePostSettings NativePostGetSettings() {
    NativePostLoadSettings();
    std::lock_guard<std::mutex> lock(g_settingsMutex);
    return g_settings;
}

void NativePostSetSettings(const NativePostSettings& settings) {
    NativePostLoadSettings();
    NativePostSettings s = settings;
    Normalize(s);
    std::lock_guard<std::mutex> lock(g_settingsMutex);
    g_settings = s;
}

void NativePostResetNeutral(bool keepEnabled) {
    NativePostSettings s{};
    if (keepEnabled) s.enabled = NativePostGetSettings().enabled;
    NativePostSetSettings(s);
}

const char* NativePostConfigPath() {
    NativePostLoadSettings();
    return g_configPath;
}

void NativePostSaveSettings() {
    const NativePostSettings s = NativePostGetSettings();
    ResolveConfigPath();
    WritePrivateProfileStringA("ReShadeNative", "Enabled", s.enabled ? "1" : "0", g_configPath);
    WriteFloat("Mix", s.mix);
    WriteFloat("Exposure", s.exposure);
    WriteFloat("Gamma", s.gamma);
    WriteFloat("Saturation", s.saturation);
    WriteFloat("Bleach", s.bleach);
    WriteFloat("Defog", s.defog);
    WriteFloat("FogColorR", s.fogColor[0]);
    WriteFloat("FogColorG", s.fogColor[1]);
    WriteFloat("FogColorB", s.fogColor[2]);
    WriteFloat("LiftR", s.lift[0]); WriteFloat("LiftG", s.lift[1]); WriteFloat("LiftB", s.lift[2]);
    WriteFloat("GammaR", s.rgbGamma[0]); WriteFloat("GammaG", s.rgbGamma[1]); WriteFloat("GammaB", s.rgbGamma[2]);
    WriteFloat("GainR", s.gain[0]); WriteFloat("GainG", s.gain[1]); WriteFloat("GainB", s.gain[2]);
}

NativePostProcess::~NativePostProcess() { Shutdown(); }

void NativePostProcess::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pipeline.Reset();
    m_rootSignature.Reset();
    m_srvHeap.Reset();
    m_rtvHeap.Reset();
    m_device.Reset();
    m_outputFormat = DXGI_FORMAT_UNKNOWN;
    m_width = m_height = m_slot = 0;
}

bool NativePostProcess::EnsureInitialized(ID3D12Device* device, DXGI_FORMAT outputFormat,
                                          uint32_t width, uint32_t height) {
    std::lock_guard<std::mutex> lock(m_mutex);
    outputFormat = ViewFormat(outputFormat);
    if (!device || !width || !height ||
        (outputFormat != DXGI_FORMAT_R8G8B8A8_UNORM &&
         outputFormat != DXGI_FORMAT_B8G8R8A8_UNORM)) {
        return false;
    }
    if (m_device.Get() == device && m_outputFormat == outputFormat &&
        m_width == width && m_height == height && m_pipeline) return true;

    m_pipeline.Reset(); m_rootSignature.Reset(); m_srvHeap.Reset(); m_rtvHeap.Reset();
    m_device = device; m_outputFormat = outputFormat; m_width = width; m_height = height; m_slot = 0;

    D3D12_DESCRIPTOR_HEAP_DESC sh{};
    sh.NumDescriptors = kSlots;
    sh.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    sh.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device->CreateDescriptorHeap(&sh, IID_PPV_ARGS(&m_srvHeap)))) return false;
    D3D12_DESCRIPTOR_HEAP_DESC rh{};
    rh.NumDescriptors = kSlots;
    rh.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    if (FAILED(device->CreateDescriptorHeap(&rh, IID_PPV_ARGS(&m_rtvHeap)))) return false;
    m_srvStride = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_rtvStride = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 1;
    range.BaseShaderRegister = 0;
    D3D12_ROOT_PARAMETER params[2]{};
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[0].Constants.ShaderRegister = 0;
    params[0].Constants.Num32BitValues = 24;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[1].DescriptorTable.NumDescriptorRanges = 1;
    params[1].DescriptorTable.pDescriptorRanges = &range;
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ShaderRegister = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    D3D12_ROOT_SIGNATURE_DESC rs{};
    rs.NumParameters = 2; rs.pParameters = params;
    rs.NumStaticSamplers = 1; rs.pStaticSamplers = &sampler;
    rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ComPtr<ID3DBlob> rsBlob, rsErrors;
    if (FAILED(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &rsBlob, &rsErrors)) ||
        FAILED(device->CreateRootSignature(0, rsBlob->GetBufferPointer(), rsBlob->GetBufferSize(),
                                           IID_PPV_ARGS(&m_rootSignature)))) return false;

    ComPtr<ID3DBlob> vs, ps;
    if (!Compile("VSMain", "vs_5_0", vs) || !Compile("PSMain", "ps_5_0", ps)) return false;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pd{};
    pd.pRootSignature = m_rootSignature.Get();
    pd.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    pd.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    pd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pd.NumRenderTargets = 1; pd.RTVFormats[0] = outputFormat;
    pd.SampleDesc.Count = 1; pd.SampleMask = UINT_MAX;
    pd.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pd.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pd.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pd.DepthStencilState.DepthEnable = FALSE;
    pd.DepthStencilState.StencilEnable = FALSE;
    if (FAILED(device->CreateGraphicsPipelineState(&pd, IID_PPV_ARGS(&m_pipeline)))) return false;
    Log("[native-post] D3D12 pass initialized %ux%u fmt=%u\n", width, height,
        static_cast<unsigned>(outputFormat));
    return true;
}

bool NativePostProcess::Record(ID3D12GraphicsCommandList* list, ID3D12Resource* src,
                               ID3D12Resource* dst, const NativePostSettings& s) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!list || !src || !dst || !m_pipeline) return false;
    const uint32_t slot = m_slot++ % kSlots;
    auto srvCpu = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
    auto srvGpu = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
    auto rtvCpu = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    srvCpu.ptr += static_cast<SIZE_T>(slot) * m_srvStride;
    srvGpu.ptr += static_cast<UINT64>(slot) * m_srvStride;
    rtvCpu.ptr += static_cast<SIZE_T>(slot) * m_rtvStride;

    D3D12_SHADER_RESOURCE_VIEW_DESC sv{};
    sv.Format = ViewFormat(src->GetDesc().Format);
    sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    sv.Texture2D.MipLevels = 1;
    m_device->CreateShaderResourceView(src, &sv, srvCpu);
    D3D12_RENDER_TARGET_VIEW_DESC rv{};
    rv.Format = m_outputFormat;
    rv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    m_device->CreateRenderTargetView(dst, &rv, rtvCpu);

    D3D12_VIEWPORT vp{}; vp.Width = static_cast<float>(m_width); vp.Height = static_cast<float>(m_height); vp.MaxDepth = 1;
    D3D12_RECT rect{0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height)};
    list->RSSetViewports(1, &vp); list->RSSetScissorRects(1, &rect);
    list->OMSetRenderTargets(1, &rtvCpu, FALSE, nullptr);

    const float k[24] = {
        s.mix, s.exposure, s.gamma, s.saturation,
        s.bleach, s.defog, 0, 0,
        s.fogColor[0], s.fogColor[1], s.fogColor[2], 0,
        s.lift[0], s.lift[1], s.lift[2], 0,
        s.rgbGamma[0], s.rgbGamma[1], s.rgbGamma[2], 0,
        s.gain[0], s.gain[1], s.gain[2], static_cast<float>(s.enabled)
    };
    list->SetGraphicsRootSignature(m_rootSignature.Get());
    list->SetPipelineState(m_pipeline.Get());
    ID3D12DescriptorHeap* heaps[] = {m_srvHeap.Get()};
    list->SetDescriptorHeaps(1, heaps);
    list->SetGraphicsRoot32BitConstants(0, 24, k, 0);
    list->SetGraphicsRootDescriptorTable(1, srvGpu);
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list->DrawInstanced(3, 1, 0, 0);
    return true;
}
