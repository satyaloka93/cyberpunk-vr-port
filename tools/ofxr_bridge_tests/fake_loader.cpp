// Real loader + real V557, with only the headset runtime replaced by a fixture.
#define main unused_ofxr_fixture_main
#include "tests/layer_call_chain.cpp"
#undef main
XrResult XRAPI_CALL RuntimeCreate(const XrInstanceCreateInfo* info,XrInstance* instance) {
    return fake_create_api_layer_instance(info,nullptr,instance);
}
XrResult XRAPI_CALL RuntimeDestroySession(XrSession session) {
    if(!wait_for_queue_idle())return XR_ERROR_RUNTIME_FAILURE;
    // The real loader unloads this fixture on instance destruction. Release
    // driver objects here, outside the Windows loader lock.
    for(auto* images : {&g_application_swapchain_images,&g_current_swapchain_images,
        &g_current_swapchain_b_images,&g_synthetic_swapchain_images,&g_synthetic_swapchain_b_images,
        &g_application_swapchain_right_images,&g_current_swapchain_right_images,
        &g_current_swapchain_right_b_images,&g_synthetic_swapchain_right_images,&g_synthetic_swapchain_right_b_images})
        for(auto& image:*images)image.Reset();
    g_queue.Reset();g_device.Reset();
    return fake_destroy_session(session);
}
XrResult XRAPI_CALL RuntimeSession(XrInstance instance,const XrSessionCreateInfo* info,XrSession* session) {
    const auto* binding=static_cast<const XrGraphicsBindingD3D12KHR*>(info->next);
    if(!binding || binding->type!=XR_TYPE_GRAPHICS_BINDING_D3D12_KHR)return XR_ERROR_GRAPHICS_DEVICE_INVALID;
    g_device=binding->device;g_queue=binding->queue;
    g_create_swapchain_calls.store(0);g_destroy_swapchain_calls.store(0);
    if(!create_fake_d3d12_images())return XR_ERROR_RUNTIME_FAILURE;
    return fake_create_session(instance,info,session);
}
XrResult XRAPI_CALL RuntimeSystem(XrInstance,const XrSystemGetInfo*,XrSystemId* system) {
    if(!system)return XR_ERROR_VALIDATION_FAILURE;
    *system=1;return XR_SUCCESS;
}
XrResult XRAPI_CALL RuntimeExtensions(const char*,uint32_t capacity,uint32_t* count,XrExtensionProperties* properties) {
    if(!count)return XR_ERROR_VALIDATION_FAILURE;
    *count=1;
    if(!capacity)return XR_SUCCESS;
    if(!properties)return XR_ERROR_VALIDATION_FAILURE;
    properties[0]={XR_TYPE_EXTENSION_PROPERTIES};
    strcpy_s(properties[0].extensionName,XR_KHR_D3D12_ENABLE_EXTENSION_NAME);
    properties[0].extensionVersion=XR_KHR_D3D12_enable_SPEC_VERSION;
    return XR_SUCCESS;
}
XrResult XRAPI_CALL RuntimeGipa(XrInstance instance,const char* name,PFN_xrVoidFunction* out) {
    if(!strcmp(name,"xrDestroySession")) {*out=reinterpret_cast<PFN_xrVoidFunction>(RuntimeDestroySession);return XR_SUCCESS;}
    if(!strcmp(name,"xrCreateSession")) {*out=reinterpret_cast<PFN_xrVoidFunction>(RuntimeSession);return XR_SUCCESS;}
    if(!strcmp(name,"xrGetSystem")) {*out=reinterpret_cast<PFN_xrVoidFunction>(RuntimeSystem);return XR_SUCCESS;}
    if(!strcmp(name,"xrEnumerateInstanceExtensionProperties")) {*out=reinterpret_cast<PFN_xrVoidFunction>(RuntimeExtensions);return XR_SUCCESS;}
    if(!strcmp(name,"xrCreateInstance")) {*out=reinterpret_cast<PFN_xrVoidFunction>(RuntimeCreate);return XR_SUCCESS;}
    return fake_get_instance_proc_addr(instance,name,out);
}
extern "C" __declspec(dllexport) XrResult XRAPI_CALL xrNegotiateLoaderRuntimeInterface(
    const XrNegotiateLoaderInfo*,XrNegotiateRuntimeRequest* request) {
    request->runtimeInterfaceVersion=1;
    request->runtimeApiVersion=XR_MAKE_VERSION(1,0,0);
    request->getInstanceProcAddr=RuntimeGipa;
    return XR_SUCCESS;
}
