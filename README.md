
# Dumper-7

SDK Generator for all Unreal Engine games. Supported versions are all of UE4 and UE5.

## How to use

- Compile the executable in x64-Release
- Run `Dumper-7.exe` with the target process name or PID as an argument:
  ```
  Dumper-7.exe Game-Win64-Shipping.exe
  ```
  or
  ```
  Dumper-7.exe 12345
  ```
- Alternatively, run without arguments to see an interactive process selection menu
- The SDK is generated into the path specified by `Settings::SDKGenerationPath`, by default this is `C:\\Dumper-7`
- **Note:** You may need to run as Administrator if the target process has elevated privileges
- **See [UsingTheSDK](UsingTheSDK.md) for a guide to get started, or to migrate from an old SDK.**

## Command Line Arguments

```
Dumper-7.exe <process_name_or_pid>

Examples:
  Dumper-7.exe Game-Win64-Shipping.exe    # Attach by process name
  Dumper-7.exe 12345                       # Attach by PID
  Dumper-7.exe                            # Interactive mode - shows process list
```
## Support Me

KoFi: https://ko-fi.com/fischsalat \
Patreon: https://www.patreon.com/u119629245

LTC (LTC-network): `LLtXWxDbc5H9d96VJF36ZpwVX6DkYGpTJU` \
BTC (Bitcoin): `1DVDUMcotWzEG1tyd1FffyrYeu4YEh7spx` \
USDT (Tron (TRC20)): `TWHDoUr2H52Gb2WYdZe7z1Ct316gMg64ps`

## Overriding Offsets

- ### Only override any offsets if the generator doesn't find them, or if they are incorrect
- All overrides are made in **Generator::InitEngineCore()** inside of **Generator.cpp**

- GObjects (see [GObjects-Layout](#overriding-gobjects-layout) too)
  ```cpp
  ObjectArray::Init(/*GObjectsOffset*/, /*ChunkSize*/, /*bIsChunked*/);
  ```
  ```cpp
  /* Make sure only to use types which exist in the sdk (eg. uint8, uint64) */
  InitObjectArrayDecryption([](void* ObjPtr) -> uint8* { return reinterpret_cast<uint8*>(uint64(ObjPtr) ^ 0x8375); });
  ```
- FName::AppendString
  - Forcing GNames:
    ```cpp
    FName::Init(/*bForceGNames*/); // Useful if the AppendString offset is wrong
    ```
  - Overriding the offset:
    ```cpp
    FName::Init(/*OverrideOffset, OverrideType=[AppendString, ToString, GNames], bIsNamePool*/);
    ```
- ProcessEvent
  ```cpp
  Off::InSDK::InitPE(/*PEIndex*/);
  ```
## Overriding GObjects-Layout
- Only add a new layout if GObjects isn't automatically found for your game.
- Layout overrides are at roughly line 30 of `ObjectArray.cpp`
- For UE4.11 to UE4.20 add the layout to `FFixedUObjectArrayLayouts`
- For UE4.21 and higher add the layout to `FChunkedFixedUObjectArrayLayouts`
- **Examples:**
  ```cpp
  FFixedUObjectArrayLayout // Default UE4.11 - UE4.20
  {
      .ObjectsOffset = 0x0,
      .MaxObjectsOffset = 0x8,
      .NumObjectsOffset = 0xC
  }
  ```
  ```cpp
  FChunkedFixedUObjectArrayLayout // Default UE4.21 and above
  {
      .ObjectsOffset = 0x00,
      .MaxElementsOffset = 0x10,
      .NumElementsOffset = 0x14,
      .MaxChunksOffset = 0x18,
      .NumChunksOffset = 0x1C,
  }
  ```

## Config File
You can optionally dynamically change settings through a `Dumper-7.ini` file, instead of modifying `Settings.h`.
- **Per-game**: Create `Dumper-7.ini` in the same directory as the game's exe file.
- **Global**: Create `Dumper-7.ini` under `C:\Dumper-7`

Example:
```ini
[Settings]
SleepTimeout=100
SDKNamespaceName=MyOwnSDKNamespace
```
## Issues

If you have any issues using the Dumper, please create an Issue on this repository\
and explain the problem **in detail**.

- Should the dumper fail to attach to a process, try running as Administrator
- Should the dumper crash while dumping, attach Visual Studio's debugger to Dumper-7.exe in debug-configuration and include screenshots of the exception, callstack, and console output
- Should there be any compiler-errors in the SDK please send screenshots of them. Please note that **only build errors** are considered errors, as Intellisense often reports false positives.
Make sure to always send screenshots of the code causing the first error, as it's likely to cause a chain-reaction of errors.
- Should your own project crash using the SDK, verify your code thoroughly to make sure the error actually lies within the generated SDK.

## Known Limitations

- **ProcessEvent**: Since the dumper runs externally, it cannot directly call ProcessEvent in the target process. Some features that rely on ProcessEvent (like getting game name/version from Kismet functions) may not work automatically and may require manual configuration.
- **Performance**: Reading memory remotely is slower than direct memory access. Dumping may take longer compared to the injected DLL approach.
- **Access Rights**: The dumper needs sufficient permissions to read process memory. Run as Administrator if you encounter access denied errors.
