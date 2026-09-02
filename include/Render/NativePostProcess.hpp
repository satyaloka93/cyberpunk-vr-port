#pragma once

#include <d3d12.h>
#include <mutex>
#include <wrl.h>

// A deliberately small, native subset of the installed ReShade effects. This is not an .fx
// loader: arbitrary effect graphs stay unsupported. The arithmetic below ports the simple
// single-pass LiftGammaGain.fx and Tonemap.fx controls into the VR port so they can run after
// RenoDX NR without loading ReShade or touching NGX.
struct NativePostSettings {
    int enabled = 0;
    float mix = 1.0f;

    // SweetFX Tonemap.fx (neutral defaults match that file).
    float exposure = 0.0f;
    float gamma = 1.0f;
    float saturation = 0.0f;
    float bleach = 0.0f;
    float defog = 0.0f;
    float fogColor[3] = {0.0f, 0.0f, 1.0f};

    // SweetFX LiftGammaGain.fx (1,1,1 is neutral for each triplet).
    float lift[3] = {1.0f, 1.0f, 1.0f};
    float rgbGamma[3] = {1.0f, 1.0f, 1.0f};
    float gain[3] = {1.0f, 1.0f, 1.0f};
};

void NativePostLoadSettings();
void NativePostSaveSettings();
NativePostSettings NativePostGetSettings();
void NativePostSetSettings(const NativePostSettings& settings);
void NativePostResetNeutral(bool keepEnabled);
const char* NativePostConfigPath();

class NativePostProcess {
public:
    NativePostProcess() = default;
    ~NativePostProcess();

    // This first implementation intentionally accepts only the port's proven 8-bit eye path.
    // It fails closed on HDR/unknown formats rather than guessing at transfer functions.
    bool EnsureInitialized(ID3D12Device* device, DXGI_FORMAT outputFormat,
                           uint32_t width, uint32_t height);

    // Caller owns resource barriers. src must be PIXEL_SHADER_RESOURCE and dst RENDER_TARGET.
    bool Record(ID3D12GraphicsCommandList* list, ID3D12Resource* src,
                ID3D12Resource* dst, const NativePostSettings& settings);

    void Shutdown();

private:
    static constexpr uint32_t kSlots = 6; // two views x three frames in flight

    std::mutex m_mutex;
    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipeline;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    DXGI_FORMAT m_outputFormat = DXGI_FORMAT_UNKNOWN;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_srvStride = 0;
    uint32_t m_rtvStride = 0;
    uint32_t m_slot = 0;
};
