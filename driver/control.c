/*
KMDF control-only driver sample for Windows Phone 8.1 KMDF 1.11 (32-bit).

*/

#include <ntddk.h>
#include <wdf.h>

#pragma comment(lib, "WdfDriverEntry.lib")

// IOCTLs exposed to user-mode (METHOD_BUFFERED)
#define IOCTL_MYDRV_GET_FUNCTIONS  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MYDRV_INIT_LOG_BUFFER CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MYDRV_READ_LOG_EVENTS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)

// IoTarget to the other driver
#define SMEM_DRIVER_DEVICE_NAME L"\\Device\\SMEM"

// IOCTL sent to the other driver
#define SMEM_GET_FUNCTIONS_IOCTL 0x42000

// Sizes mentioned in the spec
#define SMEM_DRIVER_FUNCTION_TABLE_SIZE 52    // 13 pointers * 4 bytes (32-bit)
#define READ_LOG_EVENTS_OUTPUT_BUFFER_TOTAL 112

// Function pointer typedefs (32-bit environment).
typedef
unsigned int
(__fastcall *PFN_READ_LOG_EVENTS)(
    unsigned int logIndex,
    unsigned int maxNbEntries,
    char *buffer,
    unsigned int *nbDroppedEntries,
    unsigned int *nbAvailableEntries
    );

typedef
int
(__fastcall *PFN_INIT_LOG_BUFFER)(
    unsigned int logIndex
    );

// Device context
typedef struct _DEVICE_CONTEXT {
    WDFIOTARGET IoTargetSmem; // handle to \Device\SMEM
    PFN_READ_LOG_EVENTS SmemReadLogEvents;
    PFN_INIT_LOG_BUFFER SmemInitLogBuffer;
    WDFSPINLOCK FunctionLock; // protects the two pointers
} DEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceGetContext)

//
// Forward declarations
//
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DEVICE_CONTEXT_CLEANUP MyEvtDeviceContextCleanup;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL MyEvtIoDeviceControl;

static
NTSTATUS
OpenSmemDriverIoTarget(
    _In_ WDFDEVICE Device,
    _Out_ WDFIOTARGET *TargetOut
    );

static
NTSTATUS
QuerySmemDriverFunctionTable(
    _In_ DEVICE_CONTEXT* devCtx
    );

#pragma alloc_text(INIT, DriverEntry)

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
{
    NTSTATUS status;
    WDF_DRIVER_CONFIG config;
    WDFDRIVER driver;
    PWDFDEVICE_INIT pInit = NULL;
    WDFDEVICE device;
    WDF_IO_QUEUE_CONFIG queueConfig;
    WDF_OBJECT_ATTRIBUTES deviceAttributes;
    UNICODE_STRING deviceName;
    UNICODE_STRING symbolicName;
    DEVICE_CONTEXT* devCtx;

    WDF_DRIVER_CONFIG_INIT(&config, WDF_NO_EVENT_CALLBACK);
    config.DriverPoolTag = 'wp81';

    status = WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES, &config, &driver);
    if (!NT_SUCCESS(status)) {
        KdPrint(("WdfDriverCreate failed 0x%08x\n", status));
        return status;
    }

    //
    // Create a control device. Use WdfControlDeviceInitAllocate because this driver
    // is not PnP-managed and doesn't talk to hardware.
    //
    pInit = WdfControlDeviceInitAllocate(driver, &SDDL_DEVOBJ_KERNEL_ONLY);
    if (pInit == NULL) {
        KdPrint(("WdfControlDeviceInitAllocate failed\n"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlInitUnicodeString(&deviceName, L"\\Device\\Wp81SmemLogControlDriver");
    status = WdfDeviceInitAssignName(pInit, &deviceName);
    if (!NT_SUCCESS(status)) {
        KdPrint(("WdfDeviceInitAssignName failed 0x%08x\n", status));
        WdfDeviceInitFree(pInit);
        return status;
    }

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);
    deviceAttributes.EvtCleanupCallback = MyEvtDeviceContextCleanup;

    status = WdfDeviceCreate(&pInit, &deviceAttributes, &device);
    if (!NT_SUCCESS(status)) {
        KdPrint(("WdfDeviceCreate failed 0x%08x\n", status));
        WdfDeviceInitFree(pInit);
        return status;
    }

    // After WdfDeviceCreate, framework owns the pInit; don't use it further.
    pInit = NULL;

    RtlInitUnicodeString(&symbolicName, L"\\DosDevices\\Wp81SmemLogControlDriver");
    status = WdfDeviceCreateSymbolicLink(device, &symbolicName);
    if (!NT_SUCCESS(status)) {
        KdPrint(("WdfDeviceCreateSymbolicLink failed 0x%08x\n", status));
        return status;
    }

    //
    // Configure default queue for DeviceIoControl
    //
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchSequential);
    queueConfig.EvtIoDeviceControl = MyEvtIoDeviceControl;

    status = WdfIoQueueCreate(device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, NULL);
    if (!NT_SUCCESS(status)) {
        KdPrint(("WdfIoQueueCreate failed 0x%08x\n", status));
        return status;
    }

    //
    // Initialize device context
    //
    devCtx = DeviceGetContext(device);
    devCtx->IoTargetSmem = NULL;
    devCtx->SmemReadLogEvents = NULL;
    devCtx->SmemInitLogBuffer = NULL;

    status = WdfSpinLockCreate(WDF_NO_OBJECT_ATTRIBUTES, &devCtx->FunctionLock);
    if (!NT_SUCCESS(status)) {
        KdPrint(("WdfSpinLockCreate failed 0x%08x\n", status));
        return status;
    }

    //
    // Create/open IoTarget to other driver (\Device\SMEM).
    //
    status = OpenSmemDriverIoTarget(device, &devCtx->IoTargetSmem);
    if (!NT_SUCCESS(status)) {
        KdPrint(("OpenSmemDriverIoTarget failed 0x%08x\n", status));
        // Not fatal here; user can call first IOCTL to try again.
    }

    WdfControlFinishInitializing(device);

    KdPrint(("DriverEntry completed successfully\n"));
    return STATUS_SUCCESS;
}

VOID
MyEvtDeviceContextCleanup(
    _In_ WDFOBJECT DeviceObject
    )
{
    WDFDEVICE device = (WDFDEVICE)DeviceObject;
    DEVICE_CONTEXT* devCtx = DeviceGetContext(device);

    if (devCtx->IoTargetSmem) {
        WdfIoTargetClose(devCtx->IoTargetSmem);
        WdfObjectDelete(devCtx->IoTargetSmem);
        devCtx->IoTargetSmem = NULL;
    }
}

static
NTSTATUS
OpenSmemDriverIoTarget(
    _In_ WDFDEVICE Device,
    _Out_ WDFIOTARGET *TargetOut
    )
{
    NTSTATUS status;
    WDFIOTARGET ioTarget = NULL;
    WDF_OBJECT_ATTRIBUTES attrs;
    UNICODE_STRING deviceName;
    WDF_IO_TARGET_OPEN_PARAMS openParams;

    RtlInitUnicodeString(&deviceName, SMEM_DRIVER_DEVICE_NAME);

    WDF_OBJECT_ATTRIBUTES_INIT(&attrs);
    attrs.ParentObject = Device;

    status = WdfIoTargetCreate(Device, &attrs, &ioTarget);
    if (!NT_SUCCESS(status)) {
        KdPrint(("WdfIoTargetCreate failed 0x%08x\n", status));
        return status;
    }

    WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&openParams, &deviceName, (GENERIC_READ | GENERIC_WRITE));
    status = WdfIoTargetOpen(ioTarget, &openParams);
    if (!NT_SUCCESS(status)) {
        KdPrint(("WdfIoTargetOpen(%wZ) failed 0x%08x\n", &deviceName, status));
        WdfObjectDelete(ioTarget);
        return status;
    }

    *TargetOut = ioTarget;
    return STATUS_SUCCESS;
}

static
NTSTATUS
QuerySmemDriverFunctionTable(
    _In_ DEVICE_CONTEXT* devCtx
    )
{
    NTSTATUS status;
    BYTE buffer[SMEM_DRIVER_FUNCTION_TABLE_SIZE];
    WDF_MEMORY_DESCRIPTOR outMemDesc;
    WDF_REQUEST_SEND_OPTIONS options;
    SIZE_T bytesReturned = 0;

    if (devCtx->IoTargetSmem == NULL) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    RtlZeroMemory(buffer, sizeof(buffer));
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outMemDesc, (PVOID)buffer, sizeof(buffer));

    WDF_REQUEST_SEND_OPTIONS_INIT(&options, WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);
    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&options, WDF_REL_TIMEOUT_IN_SEC(5));

    status = WdfIoTargetSendInternalIoctlSynchronously(
        devCtx->IoTargetSmem,
        NULL,
        SMEM_GET_FUNCTIONS_IOCTL,
        NULL,
        &outMemDesc,
        &options,
        &bytesReturned
        );

    if (!NT_SUCCESS(status)) {
        KdPrint(("WdfIoTargetSendInternalIoctlSynchronously failed 0x%08x\n", status));
        return status;
    }

    if (bytesReturned < SMEM_DRIVER_FUNCTION_TABLE_SIZE) {
        KdPrint(("Unexpected size from other driver: %Iu\n", bytesReturned));
        return STATUS_UNSUCCESSFUL;
    }

    {
        PFN_READ_LOG_EVENTS readPtr = NULL;
        PFN_INIT_LOG_BUFFER initPtr = NULL;
        ULONG_PTR ptrValue;

        RtlCopyMemory(&ptrValue, &buffer[6 * sizeof(ULONG_PTR)], sizeof(ULONG_PTR));
        readPtr = (PFN_READ_LOG_EVENTS)(ULONG_PTR)ptrValue;

        RtlCopyMemory(&ptrValue, &buffer[7 * sizeof(ULONG_PTR)], sizeof(ULONG_PTR));
        initPtr = (PFN_INIT_LOG_BUFFER)(ULONG_PTR)ptrValue;

        WdfSpinLockAcquire(devCtx->FunctionLock);
        devCtx->SmemReadLogEvents = readPtr;
        devCtx->SmemInitLogBuffer = initPtr;
        WdfSpinLockRelease(devCtx->FunctionLock);
    }

    return STATUS_SUCCESS;
}

VOID
MyEvtIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
{
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
    WDFDEVICE device = WdfIoQueueGetDevice(Queue);
    DEVICE_CONTEXT* devCtx = DeviceGetContext(device);

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    switch (IoControlCode) {

    case IOCTL_MYDRV_GET_FUNCTIONS:
        {
            if (devCtx->IoTargetSmem == NULL) {
                status = OpenSmemDriverIoTarget(device, &devCtx->IoTargetSmem);
                if (!NT_SUCCESS(status)) {
                    KdPrint(("Failed to open IoTargetSmem 0x%08x\n", status));
                    break;
                }
            }

            status = QuerySmemDriverFunctionTable(devCtx);
            break;
        }

    case IOCTL_MYDRV_INIT_LOG_BUFFER:
        {
            size_t inBufSize = 0;
            size_t outBufSize = 0;
            unsigned int inputValue = 0;
            int initResult = 0;
            int* outPtr = NULL;
            void* userInBuf = NULL;

            status = WdfRequestRetrieveInputBuffer(Request, sizeof(unsigned int), &userInBuf, &inBufSize);
            if (!NT_SUCCESS(status)) {
                KdPrint(("WdfRequestRetrieveInputBuffer failed 0x%08x\n", status));
                break;
            }
            inputValue = *(unsigned int*)userInBuf;

            status = WdfRequestRetrieveOutputBuffer(Request, sizeof(int), (PVOID*)&outPtr, &outBufSize);
            if (!NT_SUCCESS(status)) {
                KdPrint(("WdfRequestRetrieveOutputBuffer failed 0x%08x\n", status));
                break;
            }

            WdfSpinLockAcquire(devCtx->FunctionLock);
            PFN_INIT_LOG_BUFFER initFunc = devCtx->SmemInitLogBuffer;
            WdfSpinLockRelease(devCtx->FunctionLock);

            if (initFunc == NULL) {
                KdPrint(("SmemInitLogBuffer pointer is NULL\n"));
                status = STATUS_INVALID_DEVICE_STATE;
                break;
            }

            __try {
                initResult = initFunc(inputValue);
                *outPtr = initResult;
                status = STATUS_SUCCESS;
                WdfRequestSetInformation(Request, sizeof(int));
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                KdPrint(("Exception while calling initFunc\n"));
                status = STATUS_UNSUCCESSFUL;
            }

            break;
        }

    case IOCTL_MYDRV_READ_LOG_EVENTS:
        {
            void* inBuf = NULL;
            void* outBuf = NULL;
            size_t inBufSize = 0, outBufSize = 0;
            unsigned int logIndex = 0, maxNbEntries = 0;
            unsigned int* retValuePtr = NULL;
            unsigned int* nbDroppedPtr = NULL;
            unsigned int* nbAvailablePtr = NULL;
            char* payloadBuf = NULL;

            status = WdfRequestRetrieveInputBuffer(Request, sizeof(unsigned int) * 2, &inBuf, &inBufSize);
            if (!NT_SUCCESS(status)) {
                KdPrint(("WdfRequestRetrieveInputBuffer failed 0x%08x\n", status));
                break;
            }

            status = WdfRequestRetrieveOutputBuffer(Request, READ_LOG_EVENTS_OUTPUT_BUFFER_TOTAL, &outBuf, &outBufSize);
            if (!NT_SUCCESS(status)) {
                KdPrint(("WdfRequestRetrieveOutputBuffer failed 0x%08x\n", status));
                break;
            }

            logIndex = *(unsigned int*)((char*)inBuf + 0);
            maxNbEntries = *(unsigned int*)((char*)inBuf + 4);

            nbDroppedPtr = (unsigned int*)((char*)outBuf + 0);
            nbAvailablePtr = (unsigned int*)((char*)outBuf + 4);
            retValuePtr = (unsigned int*)((char*)outBuf + 8);
            payloadBuf = (char*)outBuf + 12;

            RtlZeroMemory(outBuf, READ_LOG_EVENTS_OUTPUT_BUFFER_TOTAL);

            WdfSpinLockAcquire(devCtx->FunctionLock);
            PFN_READ_LOG_EVENTS readFunc = devCtx->SmemReadLogEvents;
            WdfSpinLockRelease(devCtx->FunctionLock);

            if (readFunc == NULL) {
                KdPrint(("SmemReadLogEvents pointer is NULL\n"));
                status = STATUS_INVALID_DEVICE_STATE;
                break;
            }

            __try {
                unsigned int ret = readFunc(logIndex, maxNbEntries, payloadBuf, nbDroppedPtr, nbAvailablePtr);
                *retValuePtr = ret;
                status = STATUS_SUCCESS;
                WdfRequestSetInformation(Request, READ_LOG_EVENTS_OUTPUT_BUFFER_TOTAL);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                KdPrint(("Exception while calling readFunc\n"));
                status = STATUS_UNSUCCESSFUL;
            }

            break;
        }

    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    WdfRequestComplete(Request, status);
}