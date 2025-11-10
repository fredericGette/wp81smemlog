// wp81smemlog.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Getopt-for-Visual-Studio/getopt.h"

#define IOCTL_MYDRV_GET_FUNCTIONS  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MYDRV_INIT_LOG_BUFFER CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MYDRV_READ_LOG_EVENTS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)

BOOL isRunning = TRUE;

BOOL WINAPI consoleHandler(DWORD signal)
{
	switch (signal)
	{
	case CTRL_C_EVENT:
		isRunning = FALSE;
		// Signal is handled - don't pass it on to the next handler.
		return TRUE;
	default:
		// Pass signal on to the next handler.
		return FALSE;
	}
}

static void usage(char *programName)
{
	printf("%s - Read event records from the SMEM_LOG_EVENTS circular buffer.\n"
		"Usage:\n", programName);
	printf("\t%s [options]n", programName);
	printf("options:\n"
		"\t-h, --help               Show help options\n"
		"\t-i, --index              Log index (default is 0)\n"
		"\t-r, --raw                Print only raw data\n"
		"\t-v, --verbose            Increase verbosity\n");
}

static const struct option main_options[] = {
	{ "help",      no_argument,       NULL, 'h' },
	{ "index",     required_argument, NULL, 'i' },
	{ "raw",       no_argument,       NULL, 'r' },
	{ "verbose",   no_argument,       NULL, 'v' },
	{}
};

int main(int argc, char* argv[])
{
	BOOL verbose = FALSE;
	BOOL raw = FALSE;
	int logIndex = 0;

	for (;;) {
		int opt;

		opt = getopt_long(argc, argv,
			"hi:rv",
			main_options, NULL);

		if (opt < 0) {
			// no more option to parse
			break;
		}

		switch (opt) {
		case 'h':
			usage(argv[0]);
			return EXIT_SUCCESS;
		case 'r':
			raw = TRUE;
			break;
		case 'i':
			logIndex = atoi(optarg);
			if (logIndex < 0 || logIndex > 1)
			{
				printf("Index must be 0 or 1.\n", optarg);
				usage(argv[0]);
				return EXIT_FAILURE;
			}
			break;
		case 'v':
			printf("Verbose mode\n");
			verbose = TRUE;
			break;
		default:
			usage(argv[0]);
			return EXIT_FAILURE;
		}
	}

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
    } else if (verbose) {
        printf("IOCTL_GET_FUNCTIONS succeeded\n");
    }

    // 2) INIT_LOG_BUFFER
	uint32_t in_init = logIndex;
    uint32_t out_init = 0;
    ok = DeviceIoControl(h, IOCTL_MYDRV_INIT_LOG_BUFFER, &in_init, sizeof(in_init), &out_init, sizeof(out_init), &bytes, NULL);
    if (!ok) {
        printf("IOCTL_INIT_LOG_BUFFER failed %u\n", GetLastError());
    } else if (verbose) {
        printf("IOCTL_INIT_LOG_BUFFER returned %d (bytes=%u)\n", (int)out_init, bytes);
    }

	SetConsoleCtrlHandler(consoleHandler, TRUE);
	printf("Listening to SMEM_LOG_EVENTS...Press Ctrl-C to stop.\n");

    // 3) READ_LOG_EVENTS
    uint32_t in_read[2];
	in_read[0] = logIndex;
    in_read[1] = 5;  // maxNbEntries



	uint32_t nbAvailable = 0;
	uint32_t out_read[28];
	do {

		memset(out_read, 0, sizeof(out_read));
		ok = DeviceIoControl(h, IOCTL_MYDRV_READ_LOG_EVENTS, in_read, sizeof(in_read), out_read, sizeof(out_read), &bytes, NULL);
		if (!ok) {
			printf("IOCTL_READ_LOG_EVENTS failed %u\n", GetLastError());
		}
		else {
			uint32_t nbDropped = *(uint32_t*)(out_read + 0);
			nbAvailable = *(uint32_t*)(out_read + 1);
			uint32_t nbRead = *(uint32_t*)(out_read + 2);
			if (verbose) printf("IOCTL_READ_LOG_EVENTS succeeded: nbDropped=%u nbAvailable=%u nbRead=%u bytes=%u\n", nbDropped, nbAvailable, nbRead, bytes);
			uint32_t base_time = 0;
			bool relative_time = FALSE;

			// 1. Calculate the base address: Skip 3 x uint32_t (12 bytes)
			SmemLogRecord* base_record_ptr = (SmemLogRecord*)(out_read + 3);

			SmemLogRecord record;
			// (sizeof(out_read) / sizeof(uint32_t)) - 3 is the total number of uint32_t available 
			// for records. Max records is that divided by (sizeof(SmemLogRecord) / sizeof(uint32_t)) = 5
			unsigned int max_records_in_buffer = (sizeof(out_read) / sizeof(uint32_t) - 3) / (sizeof(SmemLogRecord) / sizeof(uint32_t));

			for (unsigned int i = 0; i < nbRead && i < max_records_in_buffer; ++i) {
				// 2. Access the i-th record using array indexing on the base pointer
				record = base_record_ptr[i];

				if (verbose) {
					print_raw_event(record);
					printf(" ");
					print_event(&record, &base_time, &relative_time, FALSE, FALSE);
					printf("\n");
				}
				else if (raw) {
					print_raw_event(record);
					printf("\n");
				}
				else {
					print_event(&record, &base_time, &relative_time, FALSE, TRUE);
				}
			}
		}

		if (nbAvailable == 0)
			Sleep(500);

	} while (ok && isRunning);

    CloseHandle(h);
    return EXIT_SUCCESS;
}

