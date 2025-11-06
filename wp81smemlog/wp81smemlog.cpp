// wp81smemlog.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#define IOCTL_MYDRV_GET_FUNCTIONS  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MYDRV_INIT_LOG_BUFFER CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MYDRV_READ_LOG_EVENTS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)


int main()
{
    HANDLE h = CreateFileA("\\\\.\\Wp81SmemLogControlDriver",
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (h == INVALID_HANDLE_VALUE) {
        printf("Failed to open device (error %u)\n", GetLastError());
        return 1;
    }

    DWORD bytes = 0;
    BOOL ok;

    // 1) GET FUNCTIONS
    ok = DeviceIoControl(h, IOCTL_MYDRV_GET_FUNCTIONS, NULL, 0, NULL, 0, &bytes, NULL);
    if (!ok) {
        printf("IOCTL_GET_FUNCTIONS failed %u\n", GetLastError());
    } else {
        printf("IOCTL_GET_FUNCTIONS succeeded\n");
    }

    // 2) INIT_LOG_BUFFER
    uint32_t in_init = 0; // example logIndex
    uint32_t out_init = 0;
    ok = DeviceIoControl(h, IOCTL_MYDRV_INIT_LOG_BUFFER, &in_init, sizeof(in_init), &out_init, sizeof(out_init), &bytes, NULL);
    if (!ok) {
        printf("IOCTL_INIT_LOG_BUFFER failed %u\n", GetLastError());
    } else {
        printf("IOCTL_INIT_LOG_BUFFER returned %d (bytes=%u)\n", (int)out_init, bytes);
    }

    // 3) READ_LOG_EVENTS
    uint32_t in_read[2];
    in_read[0] = 0;   // logIndex
    in_read[1] = 5;  // maxNbEntries

    uint8_t out_read[112];
    memset(out_read, 0, sizeof(out_read));
    ok = DeviceIoControl(h, IOCTL_MYDRV_READ_LOG_EVENTS, in_read, sizeof(in_read), out_read, sizeof(out_read), &bytes, NULL);
    if (!ok) {
        printf("IOCTL_READ_LOG_EVENTS failed %u\n", GetLastError());
    } else {
        uint32_t nbDropped = *(uint32_t*)(out_read + 0);
        uint32_t nbAvailable = *(uint32_t*)(out_read + 4);
        uint32_t ret = *(uint32_t*)(out_read + 8);
        printf("IOCTL_READ_LOG_EVENTS succeeded: nbDropped=%u nbAvailable=%u ret=%u bytes=%u\n", nbDropped, nbAvailable, ret, bytes);
        printf("Payload:\n");
        for (int i = 0; i < 100 && i < (int)(sizeof(out_read) - 12); ++i) {
            printf("%02X ", out_read[12 + i]);
            if ((i % 20) == 19) printf("\n");
        }
        printf("\n");
    }

    CloseHandle(h);
    return 0;
}

