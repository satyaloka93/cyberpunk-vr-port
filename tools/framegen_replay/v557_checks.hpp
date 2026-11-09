// Prove both eye preparations can finish while the previous display still owns
// both outputs. This would deadlock behind the first eye in the serial adapter.
void V557BatchChecks(){
 for(bool timed:{false,true}){
  Gpu gpu(true);
  constexpr unsigned w=128,h=96;
  std::array<std::shared_ptr<xrfg::D3D12SwapchainHistory>,2> histories;
  std::array<xrfg::D3D12FrameSynthesizer,2> synths;
  ComPtr<ID3D12Resource> source[2],output[2];
  ComPtr<ID3D12Fence> completions[2],consumer;
  Hr(gpu.device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&consumer)));
  struct Unblock {ID3D12Fence* f;~Unblock(){f->Signal(1);}} unblock{consumer.Get()};
  xrfg::D3D12ReprojectionView view;view.pose.orientation.w=1;
  view.fov={-.8f,.8f,.7f,-.7f};view.image_rect={0,0,w,h};view.array_slice=0;
  xrfg::D3D12FrameSynthesisTicket tickets[2];
  for(unsigned e=0;e<2;++e){
   source[e]=gpu.Texture(w,h,DXGI_FORMAT_R8G8B8A8_UNORM,D3D12_RESOURCE_STATE_COPY_DEST);
   const std::vector<uint32_t> pixels(w*h,e?0xffe02010:0xff1020e0);
   gpu.Upload(source[e].Get(),pixels.data(),4,D3D12_RESOURCE_STATE_COPY_SOURCE);
   output[e]=gpu.Texture(w,h,DXGI_FORMAT_R8G8B8A8_UNORM,D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
   histories[e]=std::make_shared<xrfg::D3D12SwapchainHistory>();
   ID3D12Resource* src[]{source[e].Get()},*dst[]{output[e].Get()};
   Hr(histories[e]->initialize(gpu.device.Get(),gpu.queue.Get(),src,D3D12_RESOURCE_STATE_COPY_SOURCE));
   Hr(synths[e].initialize(gpu.device.Get(),gpu.queue.Get(),histories[e],dst,dst,DXGI_FORMAT_R8G8B8A8_UNORM,
      D3D12_RESOURCE_STATE_COPY_SOURCE,xrfg::D3D12OpticalFlowBackend::nvidia,{},timed,true));gpu.Wait();
   xrfg::D3D12HistoryCaptureTicket capture;
   Hr(histories[e]->capture_source(source[e].Get(),&capture));Hr(histories[e]->commit(capture));
   Hr(synths[e].submit_prime(capture,{&view,1},0,&tickets[e]));gpu.Wait();
   xrfg::D3D12NvidiaGpuTiming primeTiming{};
   static_cast<void>(synths[e].consume_nvidia_gpu_timing(&primeTiming));
   Hr(synths[e].completion_fence(&completions[e]));
   synths[e].output_consumer(consumer.Get(),1);
  }
  for(unsigned e=0;e<2;++e){
   xrfg::D3D12HistoryCaptureTicket capture;
   Hr(histories[e]->capture_source(source[e].Get(),&capture));Hr(histories[e]->commit(capture));
   Check(synths[e].submit_pair(capture,{&view,1},{&view,1},0,0,&tickets[e],std::nullopt,{},true,.5f,std::nullopt,true)==E_INVALIDARG,
      "deferred reconstruction rejects deferred real copy");
   Hr(synths[e].submit_pair(capture,{&view,1},{&view,1},0,0,&tickets[e],std::nullopt,{},false,.5f,std::nullopt,true));
  }
  // There is deliberately no consumer signal yet. Both packs must drain.
  gpu.Wait();
  for(unsigned e=0;e<2;++e){
   Check(completions[e]->GetCompletedValue()<tickets[e].fence_value,"prepare must not signal completed output");
   Hr(synths[e].flush_reconstruction());Hr(synths[e].flush_reconstruction());
  }
  Check(completions[0]->GetCompletedValue()<tickets[0].fence_value &&
        completions[1]->GetCompletedValue()<tickets[1].fence_value,"output waits preserve consumer ownership");
  Hr(consumer->Signal(1));gpu.Wait();
  for(unsigned e=0;e<2;++e){
   Check(completions[e]->GetCompletedValue()>=tickets[e].fence_value,"both completion receipts signaled");
   const auto pixels=gpu.Read(output[e].Get());
   Check(e?pixels[2]>pixels[0]+100:pixels[0]>pixels[2]+100,"batched eyes remain independent");
   xrfg::D3D12NvidiaGpuTiming t{};
   Check(synths[e].consume_nvidia_gpu_timing(&t)==(timed?S_OK:S_FALSE),"deferred timing resolves");
   if(timed)Check(t.gpu_end_qpc>=t.gpu_begin_qpc && t.total_microseconds>0,"valid deferred GPU timing");
  }
  printf("PASS V557 both-eye preparation before consumer wait: timing %s\n",timed?"on":"off");
 }
}
// GPU integration checks for the production adapter, without a game or XR session.
void V557Checks(){
 V557BatchChecks();
 for(bool privateQueue:{false,true}){
  targeted=privateQueue;Gpu gpu(true);Generator generator;
  Settings s;s.enabled=true;s.overlay=true;s.backend=Backend::NvidiaV557;s.flowScale=50;SetSettings(s);
  constexpr unsigned w=128,h=96;
  ComPtr<ID3D12Resource> colorsOwned[2];ID3D12Resource* colors[2];std::shared_ptr<const Inputs> inputs[2];EyePair cameras[2];
  for(unsigned e=0;e<2;++e){
   colorsOwned[e]=gpu.Texture(w,h,DXGI_FORMAT_R8G8B8A8_UNORM,D3D12_RESOURCE_STATE_COPY_DEST);colors[e]=colorsOwned[e].Get();
   std::vector<uint32_t> pixels(w*h,e?0xffe02010:0xff1020e0);
   for(unsigned y=30;y<60;++y)for(unsigned x=30;x<60;++x)pixels[y*w+x]=0xffa0a0a0;
   gpu.Upload(colors[e],pixels.data(),4,D3D12_RESOURCE_STATE_COPY_SOURCE);
   auto in=std::make_shared<Inputs>();in->width=in->motionWidth=w;in->height=in->motionHeight=h;
   in->hasDepth=in->hasMotion=true;in->camera.inverted=true;in->camera.nearPlane=.1f;in->camera.farPlane=100;
   in->camera.verticalFov=1.4f;in->camera.aspect=float(w)/h;
   in->depth=gpu.Texture(w,h,DXGI_FORMAT_R32_FLOAT,D3D12_RESOURCE_STATE_COPY_DEST);
   in->motion=gpu.Texture(w,h,DXGI_FORMAT_R32G32_FLOAT,D3D12_RESOURCE_STATE_COPY_DEST);
   std::vector<float> depth(w*h,.25f),motion(w*h*2,0);
   gpu.Upload(in->depth.Get(),depth.data(),4,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
   gpu.Upload(in->motion.Get(),motion.data(),8,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);inputs[e]=in;
   for(auto* c:{&cameras[e].previous,&cameras[e].current}){c->fov[0]=-.8f;c->fov[1]=.8f;c->fov[2]=.7f;c->fov[3]=-.7f;}
  }
  bool interpolated=false;
  auto generate=[&](bool reset){return generator.Generate(gpu.device.Get(),gpu.queue.Get(),colors,inputs,cameras,s,22.22,reset,1,&interpolated,gpu.fence.Get(),gpu.value);};
  bool primed=generate(true);gpu.Wait();if(!primed){primed=generate(true);gpu.Wait();}
  Check(primed && !interpolated,"V557 prime must not publish synthetic");
  Check(generator.CanPrepare(gpu.device.Get(),gpu.queue.Get(),colors,inputs,s)==privateQueue,"prepare only on targeted private queue");
  Check(generate(false),"V557 pair");gpu.Wait();Check(interpolated && generator.OutputReady(),"paired output receipt");
  for(unsigned e=0;e<2;++e){const auto pixels=gpu.Read(generator.Output(e));
   Check(e?pixels[2]>pixels[0]+100:pixels[0]>pixels[2]+100,"both eyes retain independent colors");}
  Check(generator.Bytes()>w*h*4*8,"reconstruction allocation accounting");
  // A consumer receipt must block Reset/backend replacement and preserve resources.
  ComPtr<ID3D12CommandQueue> display;D3D12_COMMAND_QUEUE_DESC desc{};Hr(gpu.device->CreateCommandQueue(&desc,IID_PPV_ARGS(&display)));
  ComPtr<ID3D12Fence> gate,receipt;Hr(gpu.device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&gate)));
  Hr(gpu.device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&receipt)));
  struct Release {ComPtr<ID3D12Fence> gate;~Release(){if(gate)gate->Signal(1);}} release{gate};
  Hr(display->Wait(gate.Get(),1));Hr(display->Signal(receipt.Get(),1));generator.ConsumerFence(receipt.Get(),1);
  Check(!generator.Reset(),"display still owns output");
  s.backend=Backend::Nvidia;SetSettings(s);Check(!generate(true),"backend switch must wait for display receipt");
  Hr(gate->Signal(1));Hr(gpu.queue->Wait(receipt.Get(),1));gpu.Wait();
  Check(generate(true),"switch to existing NVIDIA backend");gpu.Wait();Check(!interpolated && ActiveBackend()==Backend::Nvidia,"existing backend primes after switch");
  Check(generate(false),"existing backend pair after switch");gpu.Wait();
  s.backend=Backend::NvidiaV557;SetSettings(s);primed=generate(true);gpu.Wait();if(!primed){primed=generate(true);gpu.Wait();}
  Check(primed && !interpolated,"switch back safely primes V557");
  if(privateQueue){
   // Hold capture upstream without blocking Generate's CPU thread. A deferred output
   // must not be joined onto the display queue until requested by Presenter.
   ComPtr<ID3D12Fence> captureGate;Hr(gpu.device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&captureGate)));
   Release captureRelease{captureGate};Hr(gpu.queue->Wait(captureGate.Get(),1));Hr(gpu.queue->Signal(gpu.fence.Get(),++gpu.value));
   CaptureTimingPool pool;auto sample=pool.Next(gpu.device.Get());Check(bool(sample),"capture timing allocation");
   gpu.Begin();sample->Start(gpu.list.Get());sample->End(gpu.list.Get());Hr(gpu.list->Close());
   ID3D12CommandList* lists[]{gpu.list.Get()};sample->Enqueue(gpu.queue.Get());gpu.queue->ExecuteCommandLists(1,lists);
   Hr(gpu.queue->Signal(gpu.fence.Get(),++gpu.value));sample->Submitted(gpu.fence.Get(),gpu.value);
   const auto oldPairs=capturePairs,oldInvalid=captureInvalid,oldQueueSamples=queueSamples;
   flight::enabled=true;
   auto lease=std::make_shared<int>(7);std::weak_ptr<void> weak=lease;
   const auto begin=GetTickCount64();
   Check(generator.Generate(gpu.device.Get(),gpu.queue.Get(),colors,inputs,cameras,s,22.22,false,3,&interpolated,
      gpu.fence.Get(),gpu.value,lease,display.Get(),sample,true),"deferred private pair");
   lease.reset();Check(GetTickCount64()-begin<200,"Generate blocked on capture CPU wait");
   Check(!generator.OutputReady() && !weak.expired() && !generator.Reset(),"pending GPU capture retains ownership");
   Hr(captureGate->Signal(1));Check(generator.JoinOutput(gpu.queue.Get()),"deferred output join");gpu.Wait();generator.Poll();
   Check(generator.OutputReady() && weak.expired(),"receipt completion releases input lease");
   Check(capturePairs==oldPairs+1 && captureInvalid==oldInvalid && queueSamples==oldQueueSamples+1,
      "V557 capture-to-generation correlation restored");
   Check(GetStatistics().generationSpanMs>=GetStatistics().generationMs,"pair span covers active GPU work");
   flight::enabled=false;
  }
  Check(generate(true),"recenter/reset priming");gpu.Wait();Check(!interpolated,"reset never emits stale synthetic");
  Check(generate(false),"generation recovers after reset");gpu.Wait();
  Check(generator.Reset(),"completed V557 reset");gpu.Wait();
  printf("PASS V557 GPU: %s queue, separate eyes, priming/reset, backend switching, consumer protection, capture lease\n",privateQueue?"private":"shared");
 }
 Check(syncFailures==0,"unexpected GPU synchronization failure");
}
