#include <Windows.h>
#include <iostream>
#include <chrono>
#include <fstream>

#include "Generators/CppGenerator.h"
#include "Generators/MappingGenerator.h"
#include "Generators/IDAMappingGenerator.h"
#include "Generators/DumpspaceGenerator.h"

#include "Generators/Generator.h"
#include "Platform/Public/MemoryInterface.h"

// Define this to build as external EXE instead of injected DLL
#define EXTERNAL_MODE 1

enum class EFortToastType : uint8
{
        Default                        = 0,
        Subdued                        = 1,
        Impactful                      = 2,
        EFortToastType_MAX             = 3,
};

DWORD MainThread(HMODULE Module)
{
	AllocConsole();
	FILE* Dummy;
	freopen_s(&Dummy, "CONOUT$", "w", stderr);
	freopen_s(&Dummy, "CONIN$", "r", stdin);

	std::cerr << "Started Generation [Dumper-7]!\n";

#ifdef EXTERNAL_MODE
	// External mode: Initialize driver and attach to process
	std::cerr << "[External Mode] Initializing driver...\n";
	
	Memory::bUseDriverMode = true;
	
	std::cerr << "Enter target process name (e.g., GameProcess.exe): ";
	std::string processNameStr;
	std::getline(std::cin, processNameStr);
	
	std::wstring processNameW(processNameStr.begin(), processNameStr.end());
	
	if (!Memory::Initialize(processNameW.c_str(), 0))
	{
		std::cerr << "[!] Failed to initialize memory interface!\n";
		std::cerr << "Press any key to exit...\n";
		std::cin.get();
		return 1;
	}
#else
	// Injected mode: Use direct memory access
	Memory::bUseDriverMode = false;
	Memory::Initialize(nullptr, 0);
#endif

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

	Memory::Shutdown();

#ifdef EXTERNAL_MODE
	// External mode: Wait for user input before closing
	std::cerr << "Press Enter to exit...\n";
	std::cin.get();
	
	fclose(stderr);
	if (Dummy) fclose(Dummy);
	FreeConsole();
	
	return 0;
#else
	// Injected mode: Wait for F6 to unload
	while (true)
	{
		if (GetAsyncKeyState(VK_F6) & 1)
		{
			fclose(stderr);
			if (Dummy) fclose(Dummy);
			FreeConsole();

			FreeLibraryAndExitThread(Module, 0);
		}

		Sleep(100);
	}

	return 0;
#endif
}

#ifdef EXTERNAL_MODE
// External mode: Standard Windows application entry point
int main(int argc, char* argv[])
{
	return MainThread(nullptr);
}
#else
// Injected mode: DLL entry point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)MainThread, hModule, 0, 0);
		break;
	}

	return TRUE;
}
#endif
