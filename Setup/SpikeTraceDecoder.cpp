// Offline reader only. Uses installed UE TraceAnalysis; never loaded by the game.
#include "Trace/Analysis.h"
#include "Trace/Analyzer.h"
#include "Trace/DataStream.h"
#include "Containers/UnrealString.h"
#include "Misc/CommandLine.h"
#include <cstdio>
#include <string>
#include <map>

class Input final : public UE::Trace::IInDataStream {
 FILE* f;
 public: Input(const char* p):f(fopen(p,"rb")){} ~Input(){if(f)fclose(f);}
 int32 Read(void* p,uint32 n) override {return f?int32(fread(p,1,n,f)):-1;}
};
class Decoder final : public UE::Trace::IAnalyzer {
 FILE *tasks,*threads,*clock;bool clockWritten=false;
 public:
 Decoder(const char* folder){
  std::string p(folder);tasks=fopen((p+"/tasks.csv").c_str(),"w");threads=fopen((p+"/system-threads.csv").c_str(),"w");clock=fopen((p+"/trace-clock.csv").c_str(),"w");
  if(!tasks||!threads||!clock){fprintf(stderr,"Cannot open output folder: %s\n",folder);exit(3);}
  fprintf(tasks,"event,time,qpc,trace_thread,system_thread,task_id,thread_to_execute,name\n");
  fprintf(threads,"trace_thread,system_thread,name,group\n");fprintf(clock,"start_cycle,frequency,start_unix\n");
 }
 ~Decoder(){fclose(tasks);fclose(threads);fclose(clock);}
 void OnAnalysisBegin(const FOnAnalysisContext& c)override{
  c.InterfaceBuilder.RouteLoggerEvents(1,"TaskTrace");
  c.InterfaceBuilder.RouteEvent(2,"$Trace","Timing");
  c.InterfaceBuilder.RouteEvent(2,"$Trace","NewTrace");
 }
 void OnThreadInfo(const FThreadInfo& t)override{
  fprintf(threads,"%u,%u,\"%s\",\"%s\"\n",t.GetId(),t.GetSystemId(),t.GetName(),t.GetGroupName());
 }
 bool OnEvent(uint16 route,EStyle,const FOnEventContext& c)override{
  const auto& e=c.EventData;
  if(route==2){
   uint64 freq=e.GetValue<uint64>("CycleFrequency");
   if(freq&&!clockWritten){fprintf(clock,"%llu,%llu,%.9f\n",e.GetValue<uint64>("StartCycle"),freq,e.GetValue<double>("StartDateTime"));clockWritten=true;}
   return true;
  }
  const auto* event=e.GetTypeInfo().GetName();
  uint64 q=e.GetValue<uint64>("Timestamp");if(!q)return true;
  FString debug;e.GetString("DebugName",debug);
  std::string name(TCHAR_TO_UTF8(*debug));for(auto& ch:name)if(ch=='"'||ch=='\n'||ch=='\r')ch=' ';
  fprintf(tasks,"%s,%.9f,%llu,%u,%u,%llu,%d,\"%s\"\n",event,c.EventTime.AsSeconds(q),q,c.ThreadInfo.GetId(),c.ThreadInfo.GetSystemId(),e.GetValue<uint64>("TaskId"),e.GetValue<int32>("ThreadToExecuteOn",-1),name.c_str());
  return true;
 }
};
int main(int argc,char** argv){
 fprintf(stderr,"Offline decoder entered main\n");
 if(argc!=3){fprintf(stderr,"usage: SpikeTraceDecoder trace.utrace output-folder\n");return 2;}
 FCommandLine::Set(TEXT("SpikeTraceDecoder -unattended -nosound"));
 Input input(argv[1]);Decoder decoder(argv[2]);UE::Trace::FAnalysisContext context;
 context.AddAnalyzer(decoder);auto processor=context.Process(input);processor.Wait();return 0;
}
