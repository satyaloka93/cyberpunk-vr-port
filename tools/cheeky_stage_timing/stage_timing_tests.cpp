#include "stage_timing_export.hpp"
#include <string>
#include <limits>
#include <stdexcept>
#include <iostream>
using namespace cheeky::foveated_dlss;
std::string captured_line;
void emit(const char* format, const char* message) { if(std::string(format)!="%s") std::abort(); captured_line=message; }
void require(bool ok) { if(!ok) throw std::runtime_error("Timing export check failed"); }
int main() {
 try {
  export_stage_timings(123,100,1000,emit);
  require(captured_line.find("native_sr=0.000000,0")!=std::string::npos);
  std::cout<<captured_line<<'\n';
  diagnostic_note_d3d12_dlss_gpu_time(2.0f,false);
  diagnostic_note_d3d12_dlss_gpu_time(4.0f,false);
  diagnostic_note_dlss_nr_gpu_time(DiagnosticApi::d3d12,7.0f,false,false);
  diagnostic_note_pipeline_gpu_time(DiagnosticApi::d3d12,12.0f,false);
  diagnostic_note_d3d12_dlss_gpu_time(std::numeric_limits<float>::quiet_NaN(),false);
  diagnostic_note_d3d12_dlss_gpu_time(-1.0f,false);
  export_stage_timings(123,5100,1000,emit);
  require(captured_line.find("native_sr=6.000000,2")!=std::string::npos);
  require(captured_line.find("nr_after_full=7.000000,1")!=std::string::npos);
  require(captured_line.find("pipeline_after=12.000000,1")!=std::string::npos);
  require(captured_line.find("foveated_sr=0.000000,0")!=std::string::npos);
  std::cout<<captured_line<<'\n';
  export_stage_timings(123,10100,1000,emit); std::cout<<captured_line<<'\n';
  diagnostic_note_d3d12_dlss_gpu_time(8.0f,false);
  export_stage_timings(123,15100,1000,emit); std::cout<<captured_line<<'\n';
  std::cout<<"PASS: actual diagnostic totals, export, independent stages, invalid durations, stale sample fixture\n";
 } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
