//
// LoggerDriver.h
//
// Definitions for the KMDF Control Device Driver.
//

#pragma once

#include <ntddk.h>
#include <wdf.h>

// -----------------------------------------------------------------------------
// 1. Function Pointer Definitions (Matching the External Driver's Signatures)
// -----------------------------------------------------------------------------

// Function: unsigned int smem_read_log_events(
//               unsigned int logIndex, 
//               unsigned int maxNbEntries, 
//               char *buffer, 
//               _DWORD *nbDroppedEntries, 
//               _DWORD *nbAvailableEntries
//           );
// Mapped to kernel types: ULONG, ULONG, PCHAR, PULONG, PULONG
typedef ULONG (*PfnSmemReadLogEvents)(
    _In_ ULONG LogIndex,
    _In_ ULONG MaxNbEntries,
    _Out_writes_bytes_(MaxNbEntries) PCHAR Buffer,
    _Out_ PULONG NbDroppedEntries,
    _Out_ PULONG NbAvailableEntries
);

// Function: int smem_init_log_buffer(unsigned int logIndex);
// Mapped to kernel types: LONG, ULONG
typedef LONG (*PfnSmemInitLogBuffer)(
    _In_ ULONG LogIndex
);


// -----------------------------------------------------------------------------
// 2. Control Device Context Structure
// -----------------------------------------------------------------------------

// Define the context structure for our WDFDEVICE object. This is where we
// store the function pointers retrieved from the external driver.
typedef struct _DEVICE_CONTEXT {
    WDFDEVICE       ControlDevice;
    WDFIOTARGET     SmemDriverIoTarget;
    PfnSmemReadLogEvents SmemReadLogEvents;
    PfnSmemInitLogBuffer SmemInitLogBuffer;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, GetDeviceContext)


// -----------------------------------------------------------------------------
// 3. IOCTL Codes for User-Mode Communication
// -----------------------------------------------------------------------------

// IOCTL 1: Get Function Pointers from \Device\SMEM
#define LKMDF_GET_FUNCTIONS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

// IOCTL 2: Initialize Log Buffer
#define LKMDF_INIT_LOG      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

// IOCTL 3: Read Log Events
#define LKMDF_READ_LOG      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)


// -----------------------------------------------------------------------------
// 4. Driver and Device Callback Function Prototypes
// -----------------------------------------------------------------------------

DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_DEVICE_ADD EvtDriverDeviceAdd;

EVT_WDF_DEVICE_CONTEXT_CLEANUP EvtDeviceContextCleanup;

EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL EvtIoDeviceControl;

// Forward declaration for the helper function to open the IO Target
NTSTATUS LoggerDriverOpenIoTarget(
    _In_ WDFDEVICE Device
);

// Forward declaration for the handler of the specific IOCTLs
VOID LoggerDriverHandleIoctlGetFunctions(
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength
);

VOID LoggerDriverHandleIoctlInitLog(
    _In_ WDFREQUEST Request,
    _In_ size_t InputBufferLength,
    _In_ size_t OutputBufferLength
);

VOID LoggerDriverHandleIoctlReadLog(
    _In_ WDFREQUEST Request,
    _In_ size_t InputBufferLength,
    _In_ size_t OutputBufferLength
);
