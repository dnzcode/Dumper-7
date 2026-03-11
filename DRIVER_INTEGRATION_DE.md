# Integration mit Ihrem Kernel-Treiber / Integration with Your Kernel Driver

## Zusammenfassung / Summary

Ich habe Dumper-7 erweitert, um Ihren Kernel-Treiber über IOCTL für das Lesen des Prozessspeichers zu unterstützen.

I've extended Dumper-7 to support your kernel driver via IOCTL for reading process memory.

## Wie es funktioniert / How It Works

### Deutsch

1. **Ihr Treiber muss folgende IOCTLs implementieren:**
   - `IOCTL_READ_MEMORY` (0x800) - Speicher lesen
   - `IOCTL_GET_MODULE_BASE` (0x801) - Modulbasisadresse abrufen
   - `IOCTL_GET_MODULE_SIZE` (0x802) - Modulgröße abrufen
   - `IOCTL_GET_PROCESS_BASE` (0x803) - Prozessbasisadresse abrufen

2. **Verwendung:**
   ```bash
   # Mit Ihrem Treiber
   Dumper-7.exe Game.exe --driver \\\\.\\MeinTreiber
   ```

3. **Automatische Auswahl:**
   - Wenn der Treiber verfügbar ist → Verwendet IOCTL
   - Wenn der Treiber nicht verfügbar ist → Verwendet ReadProcessMemory

4. **Was Sie anpassen können:**
   - Die IOCTL-Codes in `DriverInterface.h` (Zeilen 16-19)
   - Die Datenstrukturen für die Kommunikation
   - Den Gerätenamen (z.B. `\\\\.\\MeinTreiber`)

### English

1. **Your driver must implement these IOCTLs:**
   - `IOCTL_READ_MEMORY` (0x800) - Read memory
   - `IOCTL_GET_MODULE_BASE` (0x801) - Get module base address
   - `IOCTL_GET_MODULE_SIZE` (0x802) - Get module size
   - `IOCTL_GET_PROCESS_BASE` (0x803) - Get process base address

2. **Usage:**
   ```bash
   # With your driver
   Dumper-7.exe Game.exe --driver \\\\.\\MyDriver
   ```

3. **Automatic Selection:**
   - If driver is available → Uses IOCTL
   - If driver is not available → Uses ReadProcessMemory

4. **What you can customize:**
   - The IOCTL codes in `DriverInterface.h` (lines 16-19)
   - The data structures for communication
   - The device name (e.g., `\\\\.\\MyDriver`)

## Beispiel Treiber-Implementierung / Example Driver Implementation

```cpp
// In Ihrem Kernel-Treiber / In your kernel driver

NTSTATUS HandleIoctl(PIRP Irp)
{
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    void* buffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG controlCode = stack->Parameters.DeviceIoControl.IoControlCode;
    
    switch (controlCode)
    {
    case IOCTL_READ_MEMORY:  // 0x222000
    {
        DRIVER_READ_REQUEST* req = (DRIVER_READ_REQUEST*)buffer;
        
        // PID, Adresse, Größe sind in req
        // req->ProcessId
        // req->Address
        // req->Size
        // req->Buffer
        
        // Speicher lesen und in req->Buffer kopieren
        NTSTATUS status = ReadProcessMemoryFromKernel(
            req->ProcessId,
            req->Address,
            req->Buffer,
            req->Size
        );
        
        Irp->IoStatus.Information = req->Size;
        return status;
    }
    
    case IOCTL_GET_MODULE_BASE:  // 0x222004
    {
        DRIVER_MODULE_REQUEST* req = (DRIVER_MODULE_REQUEST*)buffer;
        
        // ModuleName ist in req->ModuleName (WCHAR)
        // ProcessId ist in req->ProcessId
        
        // Modulbasisadresse finden
        req->BaseAddress = FindModuleBase(
            req->ProcessId,
            req->ModuleName
        );
        
        Irp->IoStatus.Information = sizeof(DRIVER_MODULE_REQUEST);
        return STATUS_SUCCESS;
    }
    
    // ... andere IOCTLs
    }
}
```

## Anpassung an Ihren Treiber / Adapting to Your Driver

### Wenn Ihre IOCTL-Codes anders sind / If your IOCTL codes are different:

Bearbeiten Sie `Dumper/DriverInterface.h`:

```cpp
// Ändern Sie diese Werte / Change these values:
#define IOCTL_READ_MEMORY       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_MODULE_BASE   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
// ... usw.

// Zu Ihren Werten / To your values:
#define IOCTL_READ_MEMORY       0x12345678  // Ihr Code
#define IOCTL_GET_MODULE_BASE   0x12345679  // Ihr Code
```

### Wenn Ihre Strukturen anders sind / If your structures are different:

Bearbeiten Sie die Strukturen in `DriverInterface.h` (Zeilen 22-42):

```cpp
struct DRIVER_READ_REQUEST
{
    // Passen Sie an Ihre Struktur an
    // Adapt to your structure
    DWORD ProcessId;
    uintptr_t Address;
    SIZE_T Size;
    void* Buffer;
};
```

## Vorteile / Benefits

✅ **Kann geschützte Prozesse lesen** / Can read from protected processes
✅ **Umgeht Anti-Cheat** / Bypasses anti-cheat  
✅ **Automatischer Fallback** / Automatic fallback
✅ **Keine Codeänderungen nötig** / No code changes needed
✅ **Funktioniert mit und ohne Treiber** / Works with and without driver

## Testen / Testing

```bash
# 1. Treiber laden / Load driver
sc create MeinTreiber binPath="C:\\Pfad\\zu\\treiber.sys" type=kernel
sc start MeinTreiber

# 2. Dumper mit Treiber starten / Start dumper with driver
Dumper-7.exe Spiel.exe --driver \\\\.\\MeinTreiber

# 3. Ausgabe prüfen / Check output
# Sollte sehen: "Using Kernel Driver for memory access"
# Should see: "Using Kernel Driver for memory access"
```

## Fehlerbehebung / Troubleshooting

### Treiber nicht gefunden / Driver not found
- Treiber läuft? / Driver running? `sc query MeinTreiber`
- Gerätename korrekt? / Device name correct?
- Als Administrator ausführen / Run as Administrator

### Zugriff verweigert / Access denied
- Treiber-Service läuft / Driver service running
- Treiber signiert oder Testmodus / Driver signed or test mode
- `bcdedit /set testsigning on` für Testmodus / for test mode

## Vollständige Dokumentation / Complete Documentation

Siehe / See: `DRIVER_USAGE.md` für Details zu:
- Alle IOCTL-Spezifikationen / All IOCTL specifications
- Vollständige Strukturdefinitionen / Complete structure definitions
- Beispiel-Treiber-Code / Example driver code
- Treiber-Entwicklung / Driver development
- Signierung / Signing

## Kontakt / Contact

Wenn Sie Fragen haben oder Hilfe bei der Integration benötigen, erstellen Sie bitte ein Issue im Repository.

If you have questions or need help with integration, please create an issue in the repository.
