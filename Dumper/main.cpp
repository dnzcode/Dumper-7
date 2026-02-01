#include <Windows.h>
#include <iostream>
#include <chrono>
#include <fstream>
#include <string>

#include "RemoteProcess.h"

#include "Generators/CppGenerator.h"
#include "Generators/MappingGenerator.h"
#include "Generators/IDAMappingGenerator.h"
#include "Generators/DumpspaceGenerator.h"

#include "Generators/Generator.h"

enum class EFortToastType : uint8
{
Default                        = 0,
Subdued                        = 1,
Impactful                      = 2,
EFortToastType_MAX             = 3,
};

void PrintUsage(const char* programName)
{
std::cerr << "Dumper-7 - External Unreal Engine SDK Generator\n\n";
std::cerr << "Usage:\n";
std::cerr << "  " << programName << " <process_name_or_pid>\n\n";
std::cerr << "Arguments:\n";
std::cerr << "  process_name_or_pid    Name of the process (e.g., Game.exe) or PID\n\n";
std::cerr << "Examples:\n";
std::cerr << "  " << programName << " Game-Win64-Shipping.exe\n";
std::cerr << "  " << programName << " 12345\n\n";
}

bool SelectProcess()
{
std::cout << "\n=== Process Selection ===\n";
std::cout << "Available processes:\n\n";

auto processes = RemoteProcess::ListProcesses();

// Filter to only show .exe processes
std::vector<std::pair<DWORD, std::string>> filteredProcesses;
for (const auto& [pid, name] : processes)
{
if (name.find(".exe") != std::string::npos)
{
filteredProcesses.push_back({ pid, name });
}
}

// Display processes
for (size_t i = 0; i < filteredProcesses.size() && i < 50; i++)
{
std::cout << "  [" << i + 1 << "] " << filteredProcesses[i].second 
          << " (PID: " << filteredProcesses[i].first << ")\n";
}

std::cout << "\nEnter selection number or process name: ";
std::string input;
std::getline(std::cin, input);

if (input.empty())
{
return false;
}

// Try to parse as number first
try
{
size_t idx = std::stoull(input);
if (idx > 0 && idx <= filteredProcesses.size())
{
return g_RemoteProcess->Attach(filteredProcesses[idx - 1].first);
}
}
catch (...)
{
// Not a number, try as process name
}

// Try to attach by name or PID
try
{
DWORD pid = std::stoul(input);
return g_RemoteProcess->Attach(pid);
}
catch (...)
{
return g_RemoteProcess->Attach(input);
}
}

int main(int argc, char* argv[])
{
std::cerr << "\n=== Dumper-7 - External SDK Generator ===\n";
std::cerr << "By me, you & him\n\n";

// Initialize remote process
InitRemoteProcess();

bool attached = false;

// Check for command line arguments
if (argc > 1)
{
std::string arg = argv[1];

// Check if it's a PID
try
{
DWORD pid = std::stoul(arg);
attached = g_RemoteProcess->Attach(pid);
}
catch (...)
{
// Not a PID, try as process name
attached = g_RemoteProcess->Attach(arg);
}
}

// If not attached via command line, show selection menu
if (!attached)
{
if (argc > 1)
{
std::cerr << "Failed to attach to process: " << argv[1] << "\n";
}

attached = SelectProcess();
}

if (!attached)
{
std::cerr << "\nFailed to attach to process!\n";
std::cerr << "Press Enter to exit...";
std::cin.get();
ShutdownRemoteProcess();
return 1;
}

std::cerr << "\nStarted Generation [Dumper-7]!\n";

Settings::Config::Load();

if (Settings::Config::SleepTimeout > 0)
{
std::cerr << "Sleeping for " << Settings::Config::SleepTimeout << "ms...\n";
Sleep(Settings::Config::SleepTimeout);
}

auto DumpStartTime = std::chrono::high_resolution_clock::now();

Generator::InitEngineCore();
Generator::InitInternal();

if (Settings::Generator::GameName.empty() && Settings::Generator::GameVersion.empty())
{
// Only Possible in Main()
FString Name;
FString Version;
UEClass Kismet = ObjectArray::FindClassFast("KismetSystemLibrary");
UEFunction GetGameName = Kismet.GetFunction("KismetSystemLibrary", "GetGameName");
UEFunction GetEngineVersion = Kismet.GetFunction("KismetSystemLibrary", "GetEngineVersion");

Kismet.ProcessEvent(GetGameName, &Name);
Kismet.ProcessEvent(GetEngineVersion, &Version);

Settings::Generator::GameName = Name.ToString();
Settings::Generator::GameVersion = Version.ToString();
}

std::cerr << "GameName: " << Settings::Generator::GameName << "\n";
std::cerr << "GameVersion: " << Settings::Generator::GameVersion << "\n\n";

std::cerr << "FolderName: " << (Settings::Generator::GameVersion + '-' + Settings::Generator::GameName) << "\n\n";

Generator::Generate<CppGenerator>();
Generator::Generate<MappingGenerator>();
Generator::Generate<IDAMappingGenerator>();
Generator::Generate<DumpspaceGenerator>();

auto DumpFinishTime = std::chrono::high_resolution_clock::now();

std::chrono::duration<double, std::milli> DumpTime = DumpFinishTime - DumpStartTime;

std::cerr << "\n\nGenerating SDK took (" << DumpTime.count() << "ms)\n\n\n";

std::cerr << "Press Enter to exit...";
std::cin.get();

ShutdownRemoteProcess();

return 0;
}
