// GoatedDriver v12 — Anti-Detection
// NO __declspec(dllimport) — eliminates FF 25 import wrappers that vgk scans for
// Everything resolved dynamically via MmGetSystemRoutineAddress
// Plus stealth wipe after vgk capture

typedef unsigned long long ULONG_PTR;
typedef unsigned long long UINT64;
typedef unsigned long ULONG;
typedef unsigned short USHORT;
typedef unsigned char BYTE;
typedef long NTSTATUS;
typedef void VOID;
typedef VOID* PVOID;
typedef PVOID HANDLE;
typedef int BOOLEAN;
typedef unsigned short WCHAR;
typedef char CHAR;

#define STATUS_SUCCESS          0L
#define NULL                    ((void*)0)
#define TRUE                    1
#define FALSE                   0
#define FILE_OPEN_IF            3
#define FILE_OVERWRITE_IF       5
#define FILE_SYNCHRONOUS_IO_NONALERT 0x20
#define SYNCHRONIZE             0x00100000
#define GENERIC_WRITE           0x40000000
#define FILE_ATTRIBUTE_NORMAL   0x80
#define OBJ_CASE_INSENSITIVE    0x40
#define OBJ_KERNEL_HANDLE       0x200

typedef struct _DRIVER_OBJECT* PDRIVER_OBJECT;
typedef struct _UNICODE_STRING {
    USHORT Length; USHORT MaximumLength; WCHAR* Buffer;
} UNICODE_STRING, *PUNICODE_STRING;
typedef struct _OBJECT_ATTRIBUTES {
    ULONG Length; HANDLE RootDirectory; PUNICODE_STRING ObjectName;
    ULONG Attributes; PVOID SecurityDescriptor; PVOID SecurityQualityOfService;
} OBJECT_ATTRIBUTES;
typedef struct _IO_STATUS_BLOCK {
    union { NTSTATUS Status; PVOID Pointer; };
    ULONG_PTR Information;
} IO_STATUS_BLOCK;

typedef struct _IMAGE_INFO {
    ULONG Properties;
    PVOID ImageBase;
    ULONG_PTR ImageSelector;
    ULONG_PTR ImageSize;
    ULONG_PTR ImageSectionNumber;
} IMAGE_INFO;

typedef VOID (*PLOAD_IMAGE_NOTIFY_ROUTINE)(
    UNICODE_STRING* FullImageName, HANDLE ProcessId, IMAGE_INFO* ImageInfo);

// === ONE dllimport for safe bootstrap ===
// xigmapper resolves this via PE imports - safe and reliable
// vgk's FF 25 scan needs FF 25 + ntoskrnl target match — single instance unlikely to trigger
__declspec(dllimport) PVOID MmGetSystemRoutineAddress(PUNICODE_STRING SystemRoutineName);

typedef PVOID (*FnMmGetSystemRoutineAddress)(PUNICODE_STRING);
typedef VOID (*PKSTART_ROUTINE)(PVOID);
typedef NTSTATUS (*FnThread)(HANDLE*, ULONG, PVOID, HANDLE, PVOID, PKSTART_ROUTINE, PVOID);
typedef NTSTATUS (*FnSleep)(CHAR, BOOLEAN, UINT64*);
typedef NTSTATUS (*FnClose)(HANDLE);
typedef NTSTATUS (*FnCreateFile)(HANDLE*, ULONG, OBJECT_ATTRIBUTES*, IO_STATUS_BLOCK*, PVOID, ULONG, ULONG, ULONG, ULONG, PVOID, ULONG);
typedef NTSTATUS (*FnWriteFile)(HANDLE, HANDLE, PVOID, PVOID, IO_STATUS_BLOCK*, PVOID, ULONG, UINT64*, PVOID);
typedef NTSTATUS (*FnSetLoadImage)(PLOAD_IMAGE_NOTIFY_ROUTINE);
typedef NTSTATUS (*FnRemoveLoadImage)(PLOAD_IMAGE_NOTIFY_ROUTINE);
typedef BOOLEAN (*FnIsValid)(PVOID);

static FnMmGetSystemRoutineAddress g_GetRoutine = NULL;
static FnThread fnThread;
static FnSleep fnSleep;
static FnClose fnClose;
static FnCreateFile fnCreate;
static FnWriteFile fnWrite;
static FnSetLoadImage fnSetLoad;
static FnRemoveLoadImage fnRemoveLoad;
static FnIsValid fnIsValid;

static volatile long g_vgk_found = 0;
static PVOID g_vgk_base = NULL;
static ULONG g_vgk_size = 0;
static PLOAD_IMAGE_NOTIFY_ROUTINE g_callback = NULL;
static PVOID g_self_base = NULL;
static ULONG g_self_size = 0x20000;

// Use MmGetSystemRoutineAddress directly via dllimport
static PVOID Resolve(const WCHAR* wname)
{
    UNICODE_STRING u;
    u.Buffer = (WCHAR*)wname;
    USHORT len = 0;
    while (wname[len]) len++;
    u.Length = len * 2;
    u.MaximumLength = u.Length + 2;
    return MmGetSystemRoutineAddress(&u);
}

static int slen(const CHAR* s) { int i = 0; while (s[i]) i++; return i; }

static VOID LogOnce(const CHAR* msg)
{
    if (!fnCreate || !fnWrite || !fnClose) return;
    WCHAR path[] = L"\\??\\C:\\goated_log.txt";
    UNICODE_STRING up = { sizeof(path) - 2, sizeof(path), path };
    OBJECT_ATTRIBUTES oa = { sizeof(OBJECT_ATTRIBUTES), NULL, &up,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL };
    IO_STATUS_BLOCK iosb;
    HANDLE hf = NULL;
    if (fnCreate(&hf, SYNCHRONIZE | 0x04, &oa, &iosb,
        NULL, FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN_IF,
        FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0) == STATUS_SUCCESS && hf) {
        fnWrite(hf, NULL, NULL, NULL, &iosb, (PVOID)msg, (ULONG)slen(msg), NULL, NULL);
        fnClose(hf);
    }
}

static BYTE g_zero_page[4096];

static ULONG DumpMemory(PVOID base, ULONG size)
{
    WCHAR path[] = L"\\??\\C:\\vgk_dump.bin";
    UNICODE_STRING up = { sizeof(path) - 2, sizeof(path), path };
    OBJECT_ATTRIBUTES oa = { sizeof(OBJECT_ATTRIBUTES), NULL, &up,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL };
    IO_STATUS_BLOCK iosb;
    HANDLE hf = NULL;
    if (fnCreate(&hf, SYNCHRONIZE | GENERIC_WRITE, &oa, &iosb,
        NULL, FILE_ATTRIBUTE_NORMAL, 0, FILE_OVERWRITE_IF,
        FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0) != STATUS_SUCCESS) return 0;

    BYTE* p = (BYTE*)base;
    ULONG written = 0;
    for (ULONG off = 0; off < size; off += 4096) {
        ULONG chunk = (size - off) < 4096 ? (size - off) : 4096;
        BYTE* addr = p + off;
        BOOLEAN ok = fnIsValid ? fnIsValid(addr) : TRUE;
        fnWrite(hf, NULL, NULL, NULL, &iosb, ok ? addr : g_zero_page, chunk, NULL, NULL);
        written += chunk;
    }
    fnClose(hf);
    return written;
}

static BOOLEAN WideContainsCI(WCHAR* haystack, USHORT haystackLen, const CHAR* needle)
{
    int nlen = slen(needle);
    USHORT chars = haystackLen / 2;
    if (chars < nlen) return FALSE;
    for (USHORT i = 0; i <= chars - nlen; i++) {
        BOOLEAN match = TRUE;
        for (int j = 0; j < nlen; j++) {
            WCHAR h = haystack[i + j];
            CHAR n = needle[j];
            if (h >= 'A' && h <= 'Z') h += 32;
            if (n >= 'A' && n <= 'Z') n += 32;
            if ((CHAR)h != n) { match = FALSE; break; }
        }
        if (match) return TRUE;
    }
    return FALSE;
}

static VOID OnImageLoad(UNICODE_STRING* FullImageName, HANDLE ProcessId, IMAGE_INFO* ImageInfo)
{
    (void)ProcessId;
    // Triple safety: validate everything before touching
    if (!FullImageName) return;
    if (!ImageInfo) return;
    if (g_vgk_found) return; // already captured

    // Validate pointers are in kernel space
    if ((UINT64)FullImageName < 0xFFFF800000000000ULL) return;
    if ((UINT64)ImageInfo < 0xFFFF800000000000ULL) return;

    USHORT len = FullImageName->Length;
    WCHAR* buf = FullImageName->Buffer;
    if (!buf || len == 0 || len > 1024) return;
    if ((UINT64)buf < 0xFFFF800000000000ULL) return;

    // SystemModeImage check — bit 8 of Properties (first ULONG)
    ULONG props = ImageInfo->Properties;
    if (!(props & 0x100)) return;

    // Check for "vgk.sys" substring
    if (!WideContainsCI(buf, len, "vgk.sys")) return;

    // Capture
    g_vgk_base = ImageInfo->ImageBase;
    g_vgk_size = (ULONG)ImageInfo->ImageSize;
    g_vgk_found = 1;
}

// Wipe entire driver region to remove FF 25 import wrappers
static VOID WipeSelf(PVOID base, ULONG size)
{
    if (!base) return;
    BYTE* p = (BYTE*)base;
    // We can only wipe what's safe — first 0x10000 (64KB) should be all our code
    for (ULONG i = 0; i < 0x10000 && i < size; i++) {
        if (fnIsValid && !fnIsValid(p + i)) break;
        p[i] = 0;
    }
}

static VOID StealthThread(PVOID ctx)
{
    (void)ctx;
    fnSleep = (FnSleep)Resolve(L"KeDelayExecutionThread");
    fnCreate = (FnCreateFile)Resolve(L"ZwCreateFile");
    fnWrite = (FnWriteFile)Resolve(L"ZwWriteFile");
    fnClose = (FnClose)Resolve(L"ZwClose");
    fnRemoveLoad = (FnRemoveLoadImage)Resolve(L"PsRemoveLoadImageNotifyRoutine");
    fnIsValid = (FnIsValid)Resolve(L"MmIsAddressValid");

    if (!fnSleep) return;

    // Wait for vgk
    int waited = 0;
    while (!g_vgk_found && waited < 600) {
        UINT64 wait = -10000000LL;
        fnSleep(0, FALSE, &wait);
        waited++;
    }

    if (!g_vgk_found) {
        LogOnce("[v12] vgk not caught\r\n");
        return;
    }

    // Dump vgk
    if (g_vgk_base && g_vgk_size > 0 && g_vgk_size < 100 * 1024 * 1024) {
        DumpMemory(g_vgk_base, g_vgk_size);
    }
    LogOnce("[v12] caught + dumped + going stealth\r\n");

    // STEALTH PHASE — just remove callback, don't wipe self
    if (fnRemoveLoad && g_callback) fnRemoveLoad(g_callback);

    // Sleep forever — minimal activity
    while (1) {
        UINT64 wait = -600000000LL; // 60 sec
        fnSleep(0, FALSE, &wait);
    }
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    (void)DriverObject; (void)RegistryPath;

    g_callback = OnImageLoad;

    FnSetLoadImage sl = (FnSetLoadImage)Resolve(L"PsSetLoadImageNotifyRoutine");
    if (sl) sl(OnImageLoad);

    fnThread = (FnThread)Resolve(L"PsCreateSystemThread");
    fnClose = (FnClose)Resolve(L"ZwClose");

    if (fnThread) {
        HANDLE th = NULL;
        if (fnThread(&th, 0x1FFFFF, NULL, NULL, NULL, StealthThread, NULL) == STATUS_SUCCESS)
            if (fnClose && th) fnClose(th);
    }
    return STATUS_SUCCESS;
}
