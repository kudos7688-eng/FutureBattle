// Offline ETL reader. Raw QPC timestamps permit direct joining with UE trace clocks.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <evntrace.h>
#include <evntcons.h>
#include <tdh.h>
#include <cstdio>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <cstring>
static FILE *switches,*ready,*threadFile,*schemaFile,*interrupts;
static unsigned targetPid;static unsigned long long totalEvents=0,csEvents=0,readyEvents=0;
static unsigned layoutErrors=0;static bool layoutChecked=false;
static std::set<unsigned> gameThreads;
static std::set<std::string> schemas;
static unsigned long long number(PEVENT_RECORD e,const wchar_t* name,unsigned long long fallback=0){
 PROPERTY_DATA_DESCRIPTOR d{};d.PropertyName=(ULONGLONG)name;d.ArrayIndex=ULONG_MAX;ULONG size=0;
 if(TdhGetPropertySize(e,0,nullptr,1,&d,&size)!=ERROR_SUCCESS||size>8)return fallback;
 unsigned long long value=0;return TdhGetProperty(e,0,nullptr,1,&d,size,(PBYTE)&value)==ERROR_SUCCESS?value:fallback;
}
static void schema(PEVENT_RECORD e){
 char key[100];sprintf_s(key,"%08lx-%u-%u",e->EventHeader.ProviderId.Data1,e->EventHeader.EventDescriptor.Opcode,e->EventHeader.EventDescriptor.Version);
 if(!schemas.insert(key).second)return;
 ULONG size=0;TdhGetEventInformation(e,0,nullptr,nullptr,&size);std::vector<BYTE>b(size);auto* i=(TRACE_EVENT_INFO*)b.data();
 if(TdhGetEventInformation(e,0,nullptr,i,&size)!=ERROR_SUCCESS){fprintf(schemaFile,"%s bytes=%u TDH unavailable\n",key,e->UserDataLength);fflush(schemaFile);return;}
 fprintf(schemaFile,"%s bytes=%u\n",key,e->UserDataLength);
 for(ULONG n=0;n<i->TopLevelPropertyCount;n++){
  auto& p=i->EventPropertyInfoArray[n];fprintf(schemaFile,"  %ls type=%u length=%u count=%u\n",(wchar_t*)(b.data()+p.NameOffset),p.nonStructType.InType,p.length,p.count);
 }
 fflush(schemaFile);
}
static void WINAPI event(PEVENT_RECORD e){
 ++totalEvents;const auto& h=e->EventHeader;const auto op=h.EventDescriptor.Opcode;
 const auto q=h.TimeStamp.QuadPart;const unsigned cpu=e->BufferContext.ProcessorNumber;
 if(h.ProviderId.Data1==0x3d6fa8d1){ // classic kernel thread provider
  if(op==1||op==3){
   schema(e);unsigned pid=(unsigned)number(e,L"ProcessId"),tid=(unsigned)number(e,L"TThreadId");
   if(!tid)tid=(unsigned)number(e,L"ThreadId");
   fprintf(threadFile,"%lld,%u,%u,%u\n",q,pid,tid,op);if(pid==targetPid)gameThreads.insert(tid);
  } else if(op==36){
   schema(e);if(e->UserDataLength<24)return;
   // MOF CSwitch v2: NewThreadId, OldThreadId, four signed priority/quantum
   // bytes, four wait/state bytes, NewThreadWaitTime, Reserved.
   const auto* p=(const unsigned char*)e->UserData;unsigned newer,older;memcpy(&newer,p,4);memcpy(&older,p+4,4);
   if(!layoutChecked){layoutChecked=true;auto a=number(e,L"NewThreadId",~0ULL),b=number(e,L"OldThreadId",~0ULL),c=number(e,L"OldThreadState",~0ULL);fprintf(schemaFile,"CSwitch check version=%u length=%u raw=%u,%u,%u tdh=%llu,%llu,%llu\n",h.EventDescriptor.Version,e->UserDataLength,newer,older,p[14],a,b,c);fflush(schemaFile);if(a==~0ULL&&b==~0ULL&&c==~0ULL&&h.EventDescriptor.Version==5&&e->UserDataLength==28){fprintf(schemaFile,"Using Microsoft PerfView CSwitch prefix offsets; TDH v5 unavailable\n");}else if(a!=newer||b!=older||c!=p[14]){++layoutErrors;}}
   if(layoutErrors)return;
   fprintf(switches,"%lld,%u,%u,%u,%u,%u,%u,%u\n",q,cpu,newer,older,p[8],p[9],p[12],p[14]);++csEvents;
  } else if(op==50){
   schema(e);if(e->UserDataLength<4)return;unsigned tid;memcpy(&tid,e->UserData,4);
   if(gameThreads.count(tid)){fprintf(ready,"%lld,%u,%u,%u,%u\n",q,cpu,tid,h.ProcessId,h.ThreadId);++readyEvents;}
  }
 } else if(h.ProviderId.Data1==0xce1dbfb4 && (op==66||op==67||op==68||op==69)){
  schema(e);auto initial=number(e,L"InitialTime");fprintf(interrupts,"%lld,%u,%u,%llu\n",q,cpu,op,initial);
 }
}
int wmain(int argc,wchar_t**argv){
 if(argc==2&&!wcscmp(argv[1],L"--topology")){
  ULONG size=0;GetSystemCpuSetInformation(nullptr,0,&size,GetCurrentProcess(),0);std::vector<BYTE>b(size);
  if(!GetSystemCpuSetInformation((PSYSTEM_CPU_SET_INFORMATION)b.data(),size,&size,GetCurrentProcess(),0))return 6;
  printf("group,cpu,core,efficiency_class,scheduling_class,parked\n");
  for(ULONG off=0;off<size;){auto* info=(PSYSTEM_CPU_SET_INFORMATION)(b.data()+off);if(info->Type==CpuSetInformation)printf("%u,%u,%u,%u,%u,%u\n",info->CpuSet.Group,info->CpuSet.LogicalProcessorIndex,info->CpuSet.CoreIndex,info->CpuSet.EfficiencyClass,info->CpuSet.SchedulingClass,info->CpuSet.Parked);off+=info->Size;}
  return 0;
 }
 if(argc!=4){fwprintf(stderr,L"usage: SpikeEtwDecoder os.etl folder process-id\n");return 2;}
 targetPid=wcstoul(argv[3],nullptr,10);std::wstring p(argv[2]);
 switches=_wfopen((p+L"/cswitch.csv").c_str(),L"w");ready=_wfopen((p+L"/ready.csv").c_str(),L"w");threadFile=_wfopen((p+L"/os-threads.csv").c_str(),L"w");schemaFile=_wfopen((p+L"/etw-schema.txt").c_str(),L"w");interrupts=_wfopen((p+L"/interrupts.csv").c_str(),L"w");
 if(!switches||!ready||!threadFile||!schemaFile||!interrupts)return 3;
 fprintf(switches,"qpc,cpu,new_tid,old_tid,new_priority,old_priority,old_wait_reason,old_state\n");fprintf(ready,"qpc,cpu,tid,readying_pid,readying_tid\n");fprintf(threadFile,"qpc,pid,tid,opcode\n");fprintf(interrupts,"end_qpc,cpu,opcode,initial_qpc\n");
 EVENT_TRACE_LOGFILEW log{};log.LogFileName=argv[1];log.ProcessTraceMode=PROCESS_TRACE_MODE_EVENT_RECORD|PROCESS_TRACE_MODE_RAW_TIMESTAMP;log.EventRecordCallback=event;
 auto handle=OpenTraceW(&log);if(handle==INVALID_PROCESSTRACE_HANDLE){fprintf(stderr,"OpenTrace error %lu\n",GetLastError());return 4;}
 auto result=ProcessTrace(&handle,1,nullptr,nullptr);CloseTrace(handle);
 auto meta=_wfopen((p+L"/etw-metadata.json").c_str(),L"w");fprintf(meta,"{\"status\":%lu,\"events_lost\":%lu,\"buffers_lost\":%lu,\"frequency\":%lld,\"events\":%llu,\"switches\":%llu,\"target_ready_events\":%llu,\"target_threads\":%zu,\"layout_errors\":%u}\n",result,log.LogfileHeader.EventsLost,log.LogfileHeader.BuffersLost,log.LogfileHeader.PerfFreq.QuadPart,totalEvents,csEvents,readyEvents,gameThreads.size(),layoutErrors);fclose(meta);
 fclose(switches);fclose(ready);fclose(threadFile);fclose(schemaFile);fclose(interrupts);return result==ERROR_SUCCESS?0:5;
}
