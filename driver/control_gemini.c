//
// control.c
//
// KMDF Control Device Driver implementation for log interaction.
//

#include "control.h"

// The external driver name we need to open
#define SMEM_DEVICE_NAME L"\\Device\\SMEM"

// IOCTL 1: External IOCTL we send to MYOTHERDRIVER
#define SMEM_GET_FUNCTIONS_IOCTL (0x42000)

// -----------------------------------------------------------------------------
// DriverEntry
// -----------------------------------------------------------------------------

NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    NTSTATUS status;
    WDF_DRIVER_CONFIG config;
    WDF_OBJECT_ATTRIBUTES attributes;

    // Initialize driver configuration
    WDF_DRIVER_CONFIG_INIT(&config, EvtDriverDeviceAdd);

    // Initialize object attributes for WDFDRIVER
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);

    // Create the WDFDRIVER object
    status = WdfDriverCreate(
        DriverObject,
        RegistryPath,
        &attributes,
        &config,
        WDF_NO_HANDLE
    );

    if (!NT_SUCCESS(status)) {
        DbgPrint("LoggerDriver: WdfDriverCreate failed with status 0x%X\n", status);
    }

    return status;
}

// -----------------------------------------------------------------------------
// EvtDriverDeviceAdd (Creates the Control Device)
// -----------------------------------------------------------------------------

NTSTATUS EvtDriverDeviceAdd(
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
)
{
    NTSTATUS status;
    WDFDEVICE hDevice;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_IO_QUEUE_CONFIG queueConfig;
    PDEVICE_CONTEXT deviceContext;
    DECLARE_CONST_UNICODE_STRING(ntDeviceName, L"\\Device\\Wp81SmemLogControlDevice");

    UNREFERENCED_PARAMETER(Driver);

    // This is a control device object, as it doesn't represent hardware.
    WdfControlDeviceInitAllocate(Driver, &DeviceInit);

    // Set the device name
    status = WdfDeviceInitAssignName(DeviceInit, &ntDeviceName);
    if (!NT_SUCCESS(status)) {
        DbgPrint("LoggerDriver: WdfDeviceInitAssignName failed 0x%X\n", status);
        WdfDeviceInitFree(DeviceInit);
        return status;
    }

    // Initialize the device object attributes and context space
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DEVICE_CONTEXT);
    attributes.EvtCleanupCallback = EvtDeviceContextCleanup;

    // Create the device object
    status = WdfDeviceCreate(&DeviceInit, &attributes, &hDevice);
    if (!NT_SUCCESS(status)) {
        DbgPrint("LoggerDriver: WdfDeviceCreate failed 0x%X\n", status);
        WdfDeviceInitFree(DeviceInit); // This is safe even if WdfDeviceCreate succeeded partially
        return status;
    }

    // Store the control device handle in its context
    deviceContext = GetDeviceContext(hDevice);
    deviceContext->ControlDevice = hDevice;
    
    // Configure the default I/O Queue
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel);
    queueConfig.EvtIoDeviceControl = EvtIoDeviceControl;

    status = WdfIoQueueCreate(
        hDevice,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        WDF_NO_HANDLE
    );
    if (!NT_SUCCESS(status)) {
        DbgPrint("LoggerDriver: WdfIoQueueCreate failed 0x%X\n", status);
        return status;
    }

    // Set the control device as operational
    WdfControlFinishInitializing(hDevice);

    return status;
}

// -----------------------------------------------------------------------------
// EvtDeviceContextCleanup
// -----------------------------------------------------------------------------

VOID EvtDeviceContextCleanup(
    _In_ WDFOBJECT Device
)
// This is where we clean up the IO target before the device object is deleted.
{
    PDEVICE_CONTEXT deviceContext = GetDeviceContext(Device);

    if (deviceContext->SmemDriverIoTarget != WDF_NO_HANDLE) {
        // Close the I/O target before its parent (the device) is deleted.
        WdfIoTargetCloseForQueryRemove(deviceContext->SmemDriverIoTarget);
    }
}

// -----------------------------------------------------------------------------
// LoggerDriverOpenIoTarget (Helper function for IOCTL 1 setup)
// -----------------------------------------------------------------------------

NTSTATUS LoggerDriverOpenIoTarget(
    _In_ WDFDEVICE Device
)
{
    NTSTATUS status;
    PDEVICE_CONTEXT deviceContext = GetDeviceContext(Device);
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_IO_TARGET_OPEN_PARAMS openParams;

    // 1. Create the IO Target object
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    // Make the device the parent, so WDF deletes the IO target automatically
    attributes.ParentObject = Device; 

    status = WdfIoTargetCreate(
        Device,
        &attributes,
        &deviceContext->SmemDriverIoTarget
    );
    if (!NT_SUCCESS(status)) {
        DbgPrint("LoggerDriver: WdfIoTargetCreate failed 0x%X\n", status);
        return status;
    }

    // 2. Open the IO Target to the external driver
    WDF_IO_TARGET_OPEN_PARAMS_INIT_CREATE_BY_NAME(
        &openParams,
        SMEM_DEVICE_NAME,
        FILE_GENERIC_READ | FILE_GENERIC_WRITE
    );
    
    status = WdfIoTargetOpen(deviceContext->SmemDriverIoTarget, &openParams);
    if (!NT_SUCCESS(status)) {
        DbgPrint("LoggerDriver: WdfIoTargetOpen failed 0x%X. Did %ws exist?\n", 
                 status, SMEM_DEVICE_NAME);
        // The IO target will be cleaned up by the cleanup callback.
        deviceContext->SmemDriverIoTarget = WDF_NO_HANDLE; 
        return status;
    }

    return status;
}

// -----------------------------------------------------------------------------
// IOCTL 1: LKMDF_GET_FUNCTIONS Handler
// -----------------------------------------------------------------------------

VOID LoggerDriverHandleIoctlGetFunctions(
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength
)
{
    NTSTATUS status;
    PDEVICE_CONTEXT deviceContext = GetDeviceContext(WdfIoTargetGetDevice(WdfRequestGetIoTarget(Request)));
    WDF_OBJECT_ATTRIBUTES attributes;
    WDFMEMORY outputMemory = WDF_NO_HANDLE;
    WDF_MEMORY_DESCRIPTOR memoryDescriptor;
    
    // Check if the output buffer size is zero as per the user's IOCTL 1 description
    // (No input/output for the *user* IOCTL, but the driver sends an IOCTL *internally*).
    // If the user provided an output buffer, it's ignored for this IOCTL.

    // 1. Ensure the IO Target is open
    if (deviceContext->SmemDriverIoTarget == WDF_NO_HANDLE) {
        status = LoggerDriverOpenIoTarget(deviceContext->ControlDevice);
        if (!NT_SUCCESS(status)) {
            WdfRequestComplete(Request, status);
            return;
        }
    }
    
    // 2. Prepare the memory for the external IOCTL output (52 bytes)
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    // Parent the memory to the request so it's auto-deleted upon request completion
    attributes.ParentObject = Request; 

    status = WdfMemoryCreate(
        &attributes,
        NonPagedPoolNx,
        'DLg',
        52, // 52 bytes output buffer for SMEM_GET_FUNCTIONS_IOCTL
        &outputMemory,
        NULL // We don't need a pointer to the buffer here, we'll get it from WdfMemoryGetBuffer
    );
    if (!NT_SUCCESS(status)) {
        DbgPrint("LoggerDriver: WdfMemoryCreate failed 0x%X\n", status);
        WdfRequestComplete(Request, status);
        return;
    }

    // 3. Send the IOCTL synchronously to \Device\MYOTHERDRIVER
    WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(&memoryDescriptor, outputMemory);

    status = WdfIoTargetSendIoctlSynchronously(
        deviceContext->SmemDriverIoTarget,
        Request, // The user request is used as the context for the internal request
        SMEM_GET_FUNCTIONS_IOCTL,
        NULL, // No input buffer
        &memoryDescriptor, // 52-byte output buffer
        NULL, // No time-out
        NULL // No information about the I/O Target's IRP
    );

    if (!NT_SUCCESS(status)) {
        DbgPrint("LoggerDriver: External IOCTL 0x%X failed 0x%X\n", SMEM_GET_FUNCTIONS_IOCTL, status);
        WdfRequestComplete(Request, status);
        return;
    }
    
    // 4. Extract function pointers from the 52-byte buffer
    PULONG_PTR functionPointers = (PULONG_PTR)WdfMemoryGetBuffer(outputMemory, NULL);
    
    ULONG *pointerArray = (ULONG *)functionPointers;

    // Index 6: read_log_events 
    // The cast is required because pointerArray[6] is a ULONG (4-byte address)
    deviceContext->SmemReadLogEvents = (PfnSmemReadLogEvents)(ULONG_PTR)pointerArray[6];

    // Index 7: init_log_buffer
    deviceContext->SmemInitLogBuffer = (PfnSmemInitLogBuffer)(ULONG_PTR)pointerArray[7];

    DbgPrint("LoggerDriver: Pointers retrieved: SmemReadLogEvents=0x%p, SmemInitLogBuffer=0x%p\n", 
             (PVOID)deviceContext->SmemReadLogEvents, (PVOID)deviceContext->SmemInitLogBuffer);
             
    // 5. Complete the user request successfully
    WdfRequestComplete(Request, status);
}

// -----------------------------------------------------------------------------
// IOCTL 2: LKMDF_INIT_LOG Handler
// -----------------------------------------------------------------------------

VOID LoggerDriverHandleIoctlInitLog(
    _In_ WDFREQUEST Request,
    _In_ size_t InputBufferLength,
    _In_ size_t OutputBufferLength
)
{
    NTSTATUS status;
    PDEVICE_CONTEXT deviceContext = GetDeviceContext(WdfIoTargetGetDevice(WdfRequestGetIoTarget(Request)));
    
    PULONG inputBuffer = NULL;
    PLONG outputBuffer = NULL;
    size_t length = 0;
    LONG result = 0;

    // 1. Check if function pointer is available
    if (deviceContext->SmemInitLogBuffer == NULL) {
        status = STATUS_INVALID_DEVICE_REQUEST;
        WdfRequestComplete(Request, status);
        return;
    }

    // 2. Check buffer lengths
    if (InputBufferLength < sizeof(ULONG) || OutputBufferLength < sizeof(LONG)) {
        status = STATUS_BUFFER_TOO_SMALL;
        WdfRequestComplete(Request, status);
        return;
    }

    // 3. Get input buffer (logIndex)
    status = WdfRequestRetrieveInputBuffer(Request, sizeof(ULONG), (PVOID*)&inputBuffer, &length);
    if (!NT_SUCCESS(status)) {
        WdfRequestComplete(Request, status);
        return;
    }
    ULONG logIndex = *inputBuffer;

    // 4. Call the function
    result = deviceContext->SmemInitLogBuffer(logIndex);

    // 5. Get and fill output buffer with the result (LONG, 4 bytes)
    status = WdfRequestRetrieveOutputBuffer(Request, sizeof(LONG), (PVOID*)&outputBuffer, &length);
    if (!NT_SUCCESS(status)) {
        WdfRequestComplete(Request, status);
        return;
    }

    *outputBuffer = result;

    // 6. Complete the request
    WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(LONG));
}

// -----------------------------------------------------------------------------
// IOCTL 3: LKMDF_READ_LOG Handler
// -----------------------------------------------------------------------------

VOID LoggerDriverHandleIoctlReadLog(
    _In_ WDFREQUEST Request,
    _In_ size_t InputBufferLength,
    _In_ size_t OutputBufferLength
)
{
    NTSTATUS status;
    PDEVICE_CONTEXT deviceContext = GetDeviceContext(WdfIoTargetGetDevice(WdfRequestGetIoTarget(Request)));
    
    PULONG inputBuffer = NULL;
    PCHAR outputBuffer = NULL;
    size_t length = 0;
    
    ULONG logIndex;
    ULONG maxNbEntries;
    
    // The output parameters are the first 8 bytes of the output buffer
    PULONG nbDroppedEntries; // Index 0-3 (4 bytes)
    PULONG nbAvailableEntries; // Index 4-7 (4 bytes)
    PULONG result; // Index 8-11 (4 bytes)
    PCHAR buffer; // Index 12-111 (100 bytes)

    // 1. Check if function pointer is available
    if (deviceContext->SmemReadLogEvents == NULL) {
        status = STATUS_INVALID_DEVICE_REQUEST;
        WdfRequestComplete(Request, status);
        return;
    }

    // 2. Check buffer lengths
    if (InputBufferLength < (2 * sizeof(ULONG)) || OutputBufferLength < (12 + 100)) { // 12 bytes headers + 100 bytes data = 112 bytes
        status = STATUS_BUFFER_TOO_SMALL;
        WdfRequestComplete(Request, status);
        return;
    }

    // 3. Get input buffer (logIndex, maxNbEntries)
    status = WdfRequestRetrieveInputBuffer(Request, 2 * sizeof(ULONG), (PVOID*)&inputBuffer, &length);
    if (!NT_SUCCESS(status)) {
        WdfRequestComplete(Request, status);
        return;
    }

    // Input buffer structure: [LogIndex (0-3)] [MaxNbEntries (4-7)]
    logIndex = inputBuffer[0];
    maxNbEntries = inputBuffer[1];

    // 4. Get output buffer
    status = WdfRequestRetrieveOutputBuffer(Request, 12 + 100, (PVOID*)&outputBuffer, &length);
    if (!NT_SUCCESS(status)) {
        WdfRequestComplete(Request, status);
        return;
    }
    
    // 5. Map the output buffer to the function's parameters
    nbDroppedEntries = (PULONG)outputBuffer; // 0-3
    nbAvailableEntries = (PULONG)(outputBuffer + sizeof(ULONG)); // 4-7
    result = (PULONG)(outputBuffer + sizeof(ULONG)*2); // 8-11
    buffer = (PCHAR)(outputBuffer + 12); // 12 and onwards (100 bytes)

    // 6. Call the function
    (*result) = deviceContext->SmemReadLogEvents(
        logIndex, 
        maxNbEntries, 
        buffer, 
        nbDroppedEntries, 
        nbAvailableEntries
    );

    // 7. Complete the request
    // We report back that the first 108 bytes of the buffer were modified.
    WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 8 + 100);
}

// -----------------------------------------------------------------------------
// EvtIoDeviceControl (Main IOCTL Dispatcher)
// -----------------------------------------------------------------------------

VOID EvtIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
)
{
    WDFDEVICE Device = WdfIoQueueGetDevice(Queue);
    PDEVICE_CONTEXT deviceContext = GetDeviceContext(Device);

    UNREFERENCED_PARAMETER(deviceContext);
    
    // The KMDF framework ensures the request is validated based on the IOCTL type (METHOD_BUFFERED).

    switch (IoControlCode) {
    case LKMDF_GET_FUNCTIONS:
        // IOCTL 1: Retrieve and store function pointers
        LoggerDriverHandleIoctlGetFunctions(Request, OutputBufferLength);
        break;

    case LKMDF_INIT_LOG:
        // IOCTL 2: Call init_log_buffer
        LoggerDriverHandleIoctlInitLog(Request, InputBufferLength, OutputBufferLength);
        break;

    case LKMDF_READ_LOG:
        // IOCTL 3: Call read_log_events
        LoggerDriverHandleIoctlReadLog(Request, InputBufferLength, OutputBufferLength);
        break;

    default:
        // Unsupported IOCTL
        WdfRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);
        break;
    }
}
