#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include "Framegen/OfxrBridge.hpp"
#include "Framegen/Framegen.hpp"
#include "xrfg/provider_api.hpp"
#include <filesystem>
#include <cstdio>
#include <cstdarg>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <vector>
namespace {cvr::framegen::Settings settings;}
extern "C" {int CyberpunkVR_RuntimeDiagnostics=0;}
namespace cvr::framegen {
void WakeHardwareTelemetry(){}
void StopHardwareTelemetry(){}
HardwareStatistics GetHardwareStatistics(){return {};}
}
void Log(const char* fmt,...) {va_list args;va_start(args,fmt);vprintf(fmt,args);va_end(args);}
void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char**) {
 try {
    wchar_t path[MAX_PATH]{};GetModuleFileNameW(nullptr,path,MAX_PATH);
    auto root=std::filesystem::path(path).parent_path();
    auto runtime=root/L"runtime.json";
    {std::ofstream f(runtime);f << "{\"file_format_version\":\"1.0.0\",\"runtime\":{\"library_path\":\"" << (root/L"ofxr_test_loader.dll").generic_string() << "\"}}";}
    SetEnvironmentVariableW(L"XR_RUNTIME_JSON",runtime.c_str());
    SetEnvironmentVariableW(L"CHEEKY_OPENXR_LAYER_DISABLE",L"1");
    SetEnvironmentVariableW(L"XRFG_DISABLE_OFXR_BRIDGE",L"1");
    SetEnvironmentVariableW(L"XR_API_LAYER_PATH",L"C:/nonexistent-ofxr-test-path");
    {std::ofstream f(root/L"ofxr-v557/observer.json");
     f << "{\"file_format_version\":\"1.0.0\",\"api_layer\":{\"name\":\"XR_APILAYER_test_observer\",\"library_path\":\"" << (root/L"ofxr_observer.dll").generic_string() << "\",\"api_version\":\"1.0\",\"implementation_version\":\"1\",\"description\":\"test observer\"}}";}
    if(argc>1) {
        std::filesystem::copy_file(root/L"ofxr_test_loader.dll",root/L"XR_APILAYER_XRFrameBridge_test.dll",std::filesystem::copy_options::overwrite_existing);
        Check(LoadLibraryW((root/L"XR_APILAYER_XRFrameBridge_test.dll").c_str())!=nullptr,"external fixture load");
    }
    settings.enabled=true;settings.backend=cvr::framegen::Backend::Nvidia;settings.quality=2;settings.flowScale=50;
    cvr::framegen::SetSettings(settings);
    Check(!cvr::framegen::Enabled(),"native generator enabled alongside V557 selection");
    Check(cvr::framegen::ParseSetting("xr_framegen_logging=0",settings) && !settings.ofxrLogging,"logging parse off");
    Check(cvr::framegen::ParseSetting("xr_framegen_logging=1",settings) && settings.ofxrLogging,"logging parse on");
    FILE* saved=tmpfile();Check(saved!=nullptr,"settings temp file");cvr::framegen::WriteSettings(saved,settings);rewind(saved);
    char line[512]{};bool persisted=false;while(fgets(line,sizeof(line),saved))persisted|=std::string(line)=="xr_framegen_logging=1\n";
    fclose(saved);Check(persisted,"logging setting not saved");
    for(int pass=0;pass<2;++pass) {
        settings.backend=pass?cvr::framegen::Backend::DlssG:cvr::framegen::Backend::Nvidia;
        cvr::framegen::SetSettings(settings);
        XrInstanceCreateInfo ci{XR_TYPE_INSTANCE_CREATE_INFO};ci.applicationInfo.apiVersion=XR_MAKE_VERSION(1,0,0);strcpy_s(ci.applicationInfo.applicationName,"OFXR integration test");
        const char* observerName="XR_APILAYER_test_observer";
        const char* graphicsExtension=XR_KHR_D3D12_ENABLE_EXTENSION_NAME;
        ci.enabledExtensionCount=1;ci.enabledExtensionNames=&graphicsExtension;
        if(argc==1) {ci.enabledApiLayerCount=1;ci.enabledApiLayerNames=&observerName;}
        XrInstance instance{};Check(XR_SUCCEEDED(xrCreateInstance(&ci,&instance)),"private create failed");
        wchar_t search[256]{};GetEnvironmentVariableW(L"XR_API_LAYER_PATH",search,256);
        Check(std::wstring(search)==L"C:/nonexistent-ofxr-test-path","layer search environment leaked");
        XrInstanceProperties props{XR_TYPE_INSTANCE_PROPERTIES};Check(XR_SUCCEEDED(xrGetInstanceProperties(instance,&props)),"core dispatch failed");
        PFN_xrVoidFunction ext{};Check(XR_SUCCEEDED(xrGetInstanceProcAddr(instance,"xrGetD3D12GraphicsRequirementsKHR",&ext))&&ext,"extension forwarding failed");
        PFN_xrVoidFunction unknown=reinterpret_cast<PFN_xrVoidFunction>(1);Check(XR_FAILED(xrGetInstanceProcAddr(instance,"xrUnknownTest",&unknown)),"unsupported extension accepted");
        if(argc>1)Check(std::string(cvr::ofxr::Status().text).find("External OFXR")!=std::string::npos,"external guard missing");
        else {
            auto observer=GetModuleHandleW(L"ofxr_observer.dll");
            auto count=reinterpret_cast<unsigned(*)()>(GetProcAddress(observer,"ObserverCalls"));
            Check(count && count()==1,"OFXR internal runtime query leaked through upper observer layer");
            Check(cvr::ofxr::RecordingStatus()==1,"recorder requested on but not writing");
            auto module=GetModuleHandleW(L"CyberpunkVR_OFXR.dll");Check(module!=nullptr,"private backend absent");
            auto snap=reinterpret_cast<int(*)(OFXR_ControlSnapshotV1*) noexcept>(GetProcAddress(module,"OFXR_GetControlSnapshotV1"));
            auto recording=reinterpret_cast<int(*)() noexcept>(GetProcAddress(module,"OFXR_GetLoggingSettingV1"));
            Check(snap&&recording,"control ABI absent");OFXR_ControlSnapshotV1 s{};Check(snap(&s)!=0,"snapshot failed");
            Check(bool(s.desired.enabled)==(pass==0) && s.desired.backend==1 && s.desired.preset==0 && s.desired.scale==2 && !s.desired.backward,"quality mapping differs from V557 baseline");
            Microsoft::WRL::ComPtr<ID3D12Device> device;
            Check(SUCCEEDED(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device))),"test device");
            D3D12_COMMAND_QUEUE_DESC qd{};Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue;
            Check(SUCCEEDED(device->CreateCommandQueue(&qd,IID_PPV_ARGS(&queue))),"test queue");
            XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};systemInfo.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
            XrSystemId system{};Check(XR_SUCCEEDED(xrGetSystem(instance,&systemInfo,&system)),"get system");
            XrGraphicsRequirementsD3D12KHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR};
            Check(XR_SUCCEEDED(reinterpret_cast<PFN_xrGetD3D12GraphicsRequirementsKHR>(ext)(instance,system,&requirements)),"graphics requirements");
            XrGraphicsBindingD3D12KHR binding{XR_TYPE_GRAPHICS_BINDING_D3D12_KHR};binding.device=device.Get();binding.queue=queue.Get();
            XrSessionCreateInfo sci{XR_TYPE_SESSION_CREATE_INFO};sci.next=&binding;sci.systemId=system;
            XrSession session{};Check(XR_SUCCEEDED(xrCreateSession(instance,&sci,&session)),"create session");
            XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO};begin.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            Check(XR_SUCCEEDED(xrBeginSession(session,&begin)),"begin session");
            XrSwapchainCreateInfo imageInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
            imageInfo.createFlags=XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT;
            imageInfo.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
            imageInfo.format=DXGI_FORMAT_R8G8B8A8_UNORM;imageInfo.sampleCount=1;
            imageInfo.width=4;imageInfo.height=4;imageInfo.faceCount=1;imageInfo.arraySize=2;imageInfo.mipCount=1;
            XrSwapchain imageChain{};Check(XR_SUCCEEDED(xrCreateSwapchain(session,&imageInfo,&imageChain)),"create history fixture");
            uint32_t imageCount{};Check(XR_SUCCEEDED(xrEnumerateSwapchainImages(imageChain,0,&imageCount,nullptr)),"enumerate history count");
            std::vector<XrSwapchainImageD3D12KHR> images(imageCount,{XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR});
            Check(XR_SUCCEEDED(xrEnumerateSwapchainImages(imageChain,imageCount,&imageCount,reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data()))),"enumerate history images");
            auto trim=reinterpret_cast<int(*)(uint64_t) noexcept>(GetProcAddress(module,"OFXR_TrimInactiveSessionV1"));
            Check(trim!=nullptr,"trim export missing");
            if(pass==0)Check(trim(reinterpret_cast<uint64_t>(session))==0,"active session was trimmed");
            const auto frame=[&] {
                XrFrameState fs{XR_TYPE_FRAME_STATE};XrFrameWaitInfo wi{XR_TYPE_FRAME_WAIT_INFO};XrFrameBeginInfo bi{XR_TYPE_FRAME_BEGIN_INFO};
                Check(XR_SUCCEEDED(xrWaitFrame(session,&wi,&fs)),"wait");Check(XR_SUCCEEDED(xrBeginFrame(session,&bi)),"begin");
                XrFrameEndInfo ei{XR_TYPE_FRAME_END_INFO};ei.displayTime=fs.predictedDisplayTime;ei.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
                Check(XR_SUCCEEDED(xrEndFrame(session,&ei)),"end");
            };
            const auto settle=[&] {for(int i=0;i<3;++i)frame();};
            settle();
            settings.ofxrLogging=false;cvr::framegen::SetSettings(settings);settle();Check(recording()==0,"F10 recording off failed");
            settings.ofxrLogging=true;cvr::framegen::SetSettings(settings);settle();Check(recording()==1,"F10 recording on failed");
            settings.enabled=false;cvr::framegen::SetSettings(settings);settle();snap(&s);Check(!s.desired.enabled,"generation disable failed");
            settings.enabled=true;cvr::framegen::SetSettings(settings);
            settings.backend=cvr::framegen::Backend::DlssG;cvr::framegen::SetSettings(settings);
            Check(!cvr::framegen::Enabled(),"native generation overlapped attached V557");
            settle();Check(cvr::framegen::Enabled(),"native generator did not resume after bypass acknowledgement");
            snap(&s);Check(!s.desired.enabled && !s.pending && !s.ready && s.bypass==1,"V557 still active with native generator");
            for(int toggle=0;toggle<4;++toggle) {
                settings.backend=cvr::framegen::Backend::Nvidia;cvr::framegen::SetSettings(settings);
                Check(!cvr::framegen::Enabled(),"native remained enabled entering V557");settle();
                snap(&s);Check(s.desired.enabled && s.ready==1,"V557 failed live enable");
                settings.backend=toggle%2?cvr::framegen::Backend::DlssG:cvr::framegen::Backend::FidelityFX;
                cvr::framegen::SetSettings(settings);Check(!cvr::framegen::Enabled(),"native enabled before drain");
                settle();Check(cvr::framegen::Enabled(),"native switch failed");
            }
            // Rapid changes before a frame boundary must coalesce to the last choice.
            settings.backend=cvr::framegen::Backend::Nvidia;cvr::framegen::SetSettings(settings);
            settings.backend=cvr::framegen::Backend::DlssG;cvr::framegen::SetSettings(settings);settle();
            Check(cvr::framegen::Enabled(),"rapid switch failed");
            Check(trim(reinterpret_cast<uint64_t>(session))==1,"inactive trim not idempotent");
            Check(XR_SUCCEEDED(xrDestroySwapchain(imageChain)),"destroy history fixture");
            Check(XR_SUCCEEDED(xrEndSession(session)),"end session");
            Check(XR_SUCCEEDED(xrDestroySession(session)),"destroy session");
            settings.backend=cvr::framegen::Backend::Nvidia;cvr::framegen::SetSettings(settings);
        }
        Check(XR_SUCCEEDED(xrDestroyInstance(instance)),"destroy failed");
    }
    if(argc==1) {
        bool receipt=false,retired=false,rebuilt=false;
        for(auto& e:std::filesystem::directory_iterator(root/L"ofxr-v557")) {
            if(e.path().extension()==L".log" && e.path().filename().string().find("pid"+std::to_string(GetCurrentProcessId()))!=std::string::npos) {
                std::ifstream f(e.path());std::string line;
                while(std::getline(f,line)) {
                    receipt|=line.find("op=logger result=557")!=std::string::npos;
                    retired|=line.find("op=embedded_configuration result=880")!=std::string::npos && line.find(" b=1 ")!=std::string::npos;
                    rebuilt|=line.find("op=embedded_configuration result=882")!=std::string::npos;
                }
            }
        }
        Check(receipt,"flight recorder produced no V557 receipt");
        Check(retired,"no actual history retirement receipt");
        Check(rebuilt,"no actual history reconstruction receipt");
    }
    puts("Private V557 dispatch/recreate, controls, logging and exclusion checks passed");return 0;
 } catch(const std::exception& e){fprintf(stderr,"FAILED: %s\n",e.what());return 1;}
}
