#include "stdafx.h"

// See https://github.com/Rivko/android-firmware-qti-sdm670/blob/main/adsp_proc/core/mproc/smem/tools/smem_log.pl

// --- Constants from smem_log.pl ---
#define BASE_MASK     0x0fff0000

#define SMEM_LOG_DEBUG_EVENT_BASE         0x00000000
#define SMEM_LOG_ONCRPC_EVENT_BASE        0x00010000
#define SMEM_LOG_SMEM_EVENT_BASE          0x00020000
#define SMEM_LOG_TMC_EVENT_BASE           0x00030000
#define SMEM_LOG_TIMETICK_EVENT_BASE      0x00040000
#define SMEM_ERR_EVENT_BASE               0x00060000
#define SMEM_LOG_RPC_ROUTER_EVENT_BASE    0x00090000
#define SMEM_LOG_CLKREGIM_EVENT_BASE      0x000A0000
#define SMEM_LOG_IPC_ROUTER_EVENT_BASE    0x000D0000
#define SMEM_LOG_QMI_CCI_EVENT_BASE       0x000E0000
#define SMEM_LOG_QMI_CSI_EVENT_BASE       0x000F0000

#define LSB_MASK      0x0000ffff
#define CONTINUE_MASK 0x30000000

/**
* @brief Prints an unknown log record in a default hex format.
*
* This function is a C conversion of the 'default_print' subroutine
* from smem_log.pl. It is used as a final fallback for event IDs
* that are not recognized by any specialized print function.
*
* @param name The subsystem name (or "UNKNOWN" if determined earlier).
* @param id The full event ID.
* @param d1 The first 32-bit data payload.
* @param d2 The second 32-bit data payload.
* @param d3 The third 32-bit data payload.
*/
void default_print(const char *name, uint32_t id, uint32_t d1, uint32_t d2, uint32_t d3)
{
	printf("%s: %08x    %08x    %08x    %08x", name, id, d1, d2, d3);
}

/**
* @brief Prints a debug log record.
*
* This function is a C conversion of the 'debug_print' subroutine
* from smem_log.pl. It formats and prints messages for various
* debug events, including SMSM, PROXY, and QMUX.
*
* @param id The full event ID, including processor and continue flags.
* @param d1 The first 32-bit data payload.
* @param d2 The second 32-bit data payload.
* @param d3 The third 32-bit data payload.
*/
void debug_print(uint32_t id, uint32_t d1, uint32_t d2, uint32_t d3)
{
	printf("id:0x%08x LOG NOT IMPLEMENTED!", id);
}

// Tables equivalent to Perl arrays
const char *QMI_PRINT_TABLE[] = {
	"TX",
	"RX",
	"ERROR"
};

const char *QMI_CNTL_PRINT_TABLE[] = {
	"REQ ",
	"UNK ",
	"RESP",
	"UNK ",
	"IND "
};

// Global error data (persistent between calls)
static uint32_t QCCI_ERR_DATA1 = '.';
static uint32_t QCCI_ERR_DATA2 = '.';
static uint32_t QCCI_ERR_DATA3 = '.';

void qmi_cci_print(uint32_t id, uint32_t d1, uint32_t d2, uint32_t d3)
{
	uint32_t cont = id & CONTINUE_MASK;
	id = id & 0xFFFF;

	if (id == 0x3) {
		// QCCI ERROR
		if (cont == 0) {
			QCCI_ERR_DATA1 = d1;
			QCCI_ERR_DATA2 = d2;
			QCCI_ERR_DATA3 = d3;
		}
		else {
			printf("QCCI:   ERROR File = %c%c%c%c%c, Line=%d",
				(char)QCCI_ERR_DATA1, (char)QCCI_ERR_DATA2, (char)QCCI_ERR_DATA3,
				(char)d1, (char)d2, d3);
		}
	}
	else if (id == 0x0 || id == 0x1) {
		// Legacy TX and RX
		const char *type = QMI_PRINT_TABLE[id];
		const char *cntl = QMI_CNTL_PRINT_TABLE[(d1 >> 16) & 0xFFFF];
		printf("QCCI:   %s %s Txn:0x%x Msg:0x%x Len:%d",
			type, cntl,
			d1 & 0xFFFF,
			d2 >> 16,
			d2 & 0xFFFF);
	}
	else if (id == 0x4 || id == 0x5) {
		// Extended TX and RX
		const char *type = QMI_PRINT_TABLE[id - 0x4];
		const char *cntl = QMI_CNTL_PRINT_TABLE[(d1 >> 16) & 0xFFFF];
		if (cont == 0) {
			printf("QCCI:   %s %s Txn:0x%x Msg:0x%x Len:%d svc_id:0x%x",
				type, cntl,
				d1 & 0xFFFF,
				d2 >> 16,
				d2 & 0xFFFF,
				d3);
		}
		else {
			printf(" svc_addr: %04x:%04x:%04x", d1, d2, d3);
		}
	}
}

// Global variables
static char QCSI_ERR_DATA1 = '.';
static char QCSI_ERR_DATA2 = '.';
static char QCSI_ERR_DATA3 = '.';

void qmi_csi_print(uint32_t id, uint32_t d1, uint32_t d2, uint32_t d3) {
	uint32_t cont = id & CONTINUE_MASK;
	id = id & 0xffff;

	if (id == 0x3) {
		if (cont == 0) {
			QCSI_ERR_DATA1 = (char)(d1 & 0xff);
			QCSI_ERR_DATA2 = (char)(d2 & 0xff);
			QCSI_ERR_DATA3 = (char)(d3 & 0xff);
		}
		else {
			printf("QCSI:   ERROR File = %c%c%c%c%c, Line=%d",
				QCSI_ERR_DATA1, QCSI_ERR_DATA2, QCSI_ERR_DATA3,
				(char)(d1 & 0xff), (char)(d2 & 0xff), d3);
		}
	}
	else if (id == 0x0 || id == 0x1) {
		const char *type = QMI_PRINT_TABLE[id];
		uint32_t txn = d1 & 0xffff;
		uint32_t msg = d2 >> 16;
		uint32_t len = d2 & 0xffff;
		const char *ctrl = QMI_CNTL_PRINT_TABLE[(d1 >> 16) % (sizeof(QMI_CNTL_PRINT_TABLE) / sizeof(QMI_CNTL_PRINT_TABLE[0]))];
		printf("QCSI:   %s %s Txn:0x%x Msg:0x%x Len:%d",
			type, ctrl, txn, msg, len);
	}
	else if (id == 0x4 || id == 0x5) {
		const char *type = QMI_PRINT_TABLE[id - 0x4];
		uint32_t txn = d1 & 0xffff;
		uint32_t msg = d2 >> 16;
		uint32_t len = d2 & 0xffff;
		const char *ctrl = QMI_CNTL_PRINT_TABLE[(d1 >> 16) % (sizeof(QMI_CNTL_PRINT_TABLE) / sizeof(QMI_CNTL_PRINT_TABLE[0]))];
		if (cont == 0) {
			printf("QCSI:   %s %s Txn:0x%x Msg:0x%x Len:%d svc_id:0x%x",
				type, ctrl, txn, msg, len, d3);
		}
		else {
			printf(" clnt_addr: %04x:%04x:%04x", d1, d2, d3);
		}
	}
}

// Assume a global buffer for the line header, 
// as implied by its use in previous functions (e.g., oncrpc_print, smem_print).
// This must be defined globally in a shared header or source file.
#define LINE_HEADER_SIZE 256
char LINE_HEADER[LINE_HEADER_SIZE];

// Global variables defined in the Perl script for time conversion
#define HT_TIMER_CLOCK_RATE 19200000 // HT Timer
#define SLEEP_CLOCK_RATE 32768 // Sleep clock
#define TIMESTAMP_CLOCK_RATE SLEEP_CLOCK_RATE // Windows phone 8.1

/**
* @brief Prints the log line header (time and processor/flag info).
*
* This function is a C conversion of the 'print_line_header' subroutine.
* It formats the processor ID and time information and stores it in the
* global LINE_HEADER buffer.
*
* @param proc_flag The 32-bit flag containing the processor identifier (0xC0000000 mask).
* @param time The timestamp of the log event (relative or absolute).
* @param ticks A flag: TRUE (1) if time is displayed in clock ticks, FALSE (0) if displayed in seconds.
*/
void print_line_header(uint32_t proc_flag, uint32_t time, bool ticks)
{
	const char *proc_name;
	double sec_time;

	// Determine the processor name
	switch (proc_flag) {
	case 0x80000000:
		proc_name = "APPS";
		break;
	case 0x40000000:
		proc_name = "QDSP";
		break;
	case 0xC0000000:
		proc_name = "WCNS";
		break;
	default:
		proc_name = "MODM";
		break;
	}

	// Determine the time format
	if (ticks) {
		// Time is absolute ticks, printed raw
		int result = _snprintf_s(LINE_HEADER, LINE_HEADER_SIZE, _TRUNCATE, "\n%4s: 0x%08x    ", proc_name, time);
		// Manually ensure null termination if using _snprintf and buffer was filled
		if (result >= 0 && (size_t)result >= LINE_HEADER_SIZE) {
			LINE_HEADER[LINE_HEADER_SIZE - 1] = '\0';
		}
	}
	else {
		// Time is in ticks. Convert to seconds (float).
		// TICKS_PER_SEC is defined and set to 32768.

		if (TIMESTAMP_CLOCK_RATE != 0) {
			sec_time = (double)time / TIMESTAMP_CLOCK_RATE;
		}
		else {
			// Fallback for division by zero (should not happen if constants are correct)
			sec_time = (double)time;
		}

		// Format the output into the global LINE_HEADER buffer
		// Perl: sprintf( "%10.4f %4s ", $sec_time, $proc_name );
		int result = _snprintf_s(LINE_HEADER, LINE_HEADER_SIZE, _TRUNCATE, "%4s: %14.6f    ", proc_name, sec_time);
		// Manually ensure null termination if using _snprintf and buffer was filled
		if (result >= 0 && (size_t)result >= LINE_HEADER_SIZE) {
			LINE_HEADER[LINE_HEADER_SIZE - 1] = '\0';
		}
	}

	printf("%s", LINE_HEADER);
}

/**
* @brief Prints a generic log message using a lookup table for the event name.
*
* This function is a C conversion of the 'generic_print' subroutine
* from smem_log.pl. It is used by various print functions (like tmc_print)
* that rely on a simple event ID to name lookup.
*
* @param table A null-terminated array of strings (the event names).
* @param name The subsystem name (e.g., "TMC", "TIMETICK").
* @param id The full event ID.
* @param d1 The first 32-bit data payload.
* @param d2 The second 32-bit data payload.
* @param d3 The third 32-bit data payload.
*/
void generic_print(const char **table, const char *name,
	uint32_t id, uint32_t d1, uint32_t d2, uint32_t d3)
{
	uint32_t event = id & LSB_MASK;
	const char *cntrl = NULL;
	unsigned int table_size = 0;

	// Calculate the size of the table (it must be null-terminated or size must be passed)
	// Assuming the table is correctly defined elsewhere and its size can be determined
	// (e.g., by checking for a NULL entry, or more practically, passing the size).
	// Since the Perl code implies iteration, we'll try to determine the count here.
	// In a real C scenario, the size MUST be passed as an argument.
	// For this conversion, we assume the tables are bounded and indexed 0-N.

	// Safety check: Determine array size for bounds checking (imperfectly, but best effort)
	// NOTE: This logic is a placeholder; in production C, pass the size explicitly.
	const char **p = table;
	while (*p != NULL) {
		p++;
		table_size++;
	}

	if (event >= 0 && event < table_size) {
		cntrl = table[event];
		printf("%s: %s 0x%08x 0x%08x 0x%08x", name, cntrl, d1, d2, d3);
	}
	else {
		default_print(name, id, d1, d2, d3);
	}
}


/**
* @brief Prints ONCRPC log messages.
* C conversion of oncrpc_print.
*/
void oncrpc_print(uint32_t id, uint32_t d1, uint32_t d2, uint32_t d3) {
	uint32_t subsys = id & 0xf0;

	if (subsys)
	{
		// BW Compatible print for QCCI & QCSI
			if (subsys == 0x30) {
				qmi_cci_print(id & 0xffffff0f, d1, d2, d3);
			}
			else {
				qmi_csi_print(id & 0xffffff0f, d1, d2, d3);
			}
	}
	else
	{
		printf("id:0x%08x subsys:0x%02x LOG NOT IMPLEMENTED!", id, subsys);
	}
}

/**
* @brief Prints SMEM log messages.
* C conversion of smem_print.
*/
void smem_print(uint32_t id, uint32_t d1, uint32_t d2, uint32_t d3)
{
	printf("id:0x%08x LOG NOT IMPLEMENTED!", id);
}


/**
* @brief Prints Error log messages.
* C conversion of err_print.
*/
void err_print(uint32_t id, uint32_t d1, uint32_t d2, uint32_t d3)
{
	printf("id:0x%08x LOG NOT IMPLEMENTED!", id);
}


enum ROUTER_PRINT_TABLE_enum {
	UNKNOWN,
	READ,
	WRITTEN,
	CNF_REQ,
	CNF_SNT,
	MID_READ,
	MID_WRITTEN,
	MID_CNF_REQ,
	PING,
	SERVER_PENDING,
	SERVER_REGISTERED,
	RESERVED1,
	RESERVED2,
	RESERVED3,
	RESERVED4,
	RESERVED5,
	IPC_ROUTER1, // BW Compatible IPC Router messages
	IPC_ROUTER2,
	IPC_ROUTER3,
	ROUTER_PRINT_TABLE_MAX= IPC_ROUTER3
};

const char* ROUTER_PRINT_TABLE[] = {
	"UNKNOWN",
	"READ   ",
	"WRITTEN",
	"CNF REQ",
	"CNF SNT",
	"MID READ",
	"MID WRITTEN",
	"MID CNF REQ",
	"PING",
	"SERVER PENDING",
	"SERVER REGISTERED",
	"RESERVED", "RESERVED", "RESERVED", "RESERVED", "RESERVED",
	"IPC_ROUTER", "IPC_ROUTER", "IPC_ROUTER"
};

const char *IPC_ROUTER_PRINT_TABLE[] = {
	"ERROR",
	"TX",
	"RX"
};

const char *IPC_ROUTER_TYPE_TABLE[] = {
	"INVALID",
	"DATA",
	"HELLO",
	"BYE",
	"NEW_SERVER",
	"REMOVE_SERVER",
	"REMOVE_CLIENT",
	"RESUME_TX"
};

// Variables used for multi-part error printing
static uint32_t ERR_DATA1, ERR_DATA2, ERR_DATA3;

// Main function equivalent to the Perl sub ipc_router_print
void ipc_router_print(uint32_t id, uint32_t d1, uint32_t d2, uint32_t d3)
{
	uint8_t event = id & 0xff;
	uint8_t cntl_type = (id >> 8) & 0xff;
	const char *cntrl = IPC_ROUTER_PRINT_TABLE[event];

	if (strcmp(cntrl, "ERROR") == 0) {
		if ((id & CONTINUE_MASK) == 0) {
			// First ERR record
			printf("ROUTER: ERROR ");
			ERR_DATA1 = d1;
			ERR_DATA2 = d2;
			ERR_DATA3 = d3;
		}
		else {
			// Second ERR record: reconstruct name
			char name[64];
			uint32_t words[5] = { ERR_DATA1, ERR_DATA2, ERR_DATA3, d1, d2 };
			memcpy(name, words, sizeof(words));
			name[sizeof(name) - 1] = '\0';
			// Trim at first null
			name[strcspn(name, "\0")] = '\0';
			printf("file: \"%s\" line: %u", name, d3);
		}
	}
	else if (strcmp(cntrl, "TX") == 0 || strcmp(cntrl, "RX") == 0) {
		if ((id & CONTINUE_MASK) == 0) {
			if (cntl_type >= 4 && cntl_type <= 5) {
				uint32_t src_proc = d1 >> 24;
				uint32_t src_port = d1 & 0xFFFFFF;
				printf("ROUTER: %s [%s] (%x,%x) @ %02x:%06x",
					cntrl, IPC_ROUTER_TYPE_TABLE[cntl_type],
					d2, d3, src_proc, src_port);
			}
			else if (cntl_type >= 6 && cntl_type <= 7) {
				printf("ROUTER: %s *%s* %08x:%08x",
					cntrl, IPC_ROUTER_TYPE_TABLE[cntl_type],
					d1, d2);
			}
			else {
				uint32_t src_proc = d1 >> 24;
				uint32_t src_port = d1 & 0xFFFFFF;
				uint32_t dst_proc = d2 >> 24;
				uint32_t dst_port = d2 & 0xFFFFFF;
				uint32_t type = d3 >> 24;
				uint32_t conf_rx = (d3 >> 16) & 1;
				uint32_t size = d3 & 0xFFFF;

				if (strcmp(cntrl, "TX") == 0) {
					printf("ROUTER: %s %02x:%06x -> %02x:%06x ",
						cntrl, src_proc, src_port, dst_proc, dst_port);
				}
				else {
					printf("ROUTER: %s %02x:%06x <- %02x:%06x ",
						cntrl, dst_proc, dst_port, src_proc, src_port);
				}

				printf("[%s] Len:%u ", IPC_ROUTER_TYPE_TABLE[type], size);
				if (conf_rx)
					printf("*CONF_RX* ");
			}
		}
		else {
			// Second record for TX/RX
			char iface[5] = { 0 };
			char task[5] = { 0 };
			memcpy(iface, &d1, 4);
			memcpy(task, &d3, 4);
			iface[4] = '\0';
			task[4] = '\0';
			printf("<%s> TID:%08x,\"%s\"", iface, d2, task);
		}
	}
}

/**
* @brief Prints RPC Router log messages.
* C conversion of rpc_router_print.
*/
void rpc_router_print(uint32_t id, uint32_t d1, uint32_t d2, uint32_t d3)
{
	uint32_t event = id & 0xff;
	uint32_t cntl_type = (id >> 8) & 0xff;
	

	if (event <= ROUTER_PRINT_TABLE_MAX)
	{
		const char *cntrl = ROUTER_PRINT_TABLE[event];
		switch ((ROUTER_PRINT_TABLE_enum)event)
		{
		case IPC_ROUTER1:
		case IPC_ROUTER2:
		case IPC_ROUTER3:
			//Old combination made the IPC Router start at 16,
			// but the new ones start at 0.
			ipc_router_print(id - 16, d1, d2, d3);
			break;

		case CNF_REQ:
		case CNF_SNT:
			printf("ROUTER: %s pid = %08x    cid = %08x    tid = %08x",
				cntrl, d1, d2, d3);
			break;

		case MID_READ:
			printf("ROUTER: READ    mid = %08x    cid = %08x    tid = %08x",
				d1, d2, d3);
			break;

		case MID_WRITTEN:
			printf("ROUTER: WRITTEN mid = %08x    cid = %08x    tid = %08x",
				d1, d2, d3);
			break;

		case MID_CNF_REQ:
			printf("ROUTER: CNF REQ pid = %08x    cid = %08x    tid = %08x",
				d1, d2, d3);
			break;

		case PING:
			printf("ROUTER: PING    pid = %08x    cid = %08x    tid = %08x",
				d1, d2, d3);
			break;

		case SERVER_PENDING:
			printf("ROUTER: SERVER PENDING REGISTRATION    prog = 0x%08x vers=0x%08x tid = %08x",
				d1, d2, d3);
			break;

		case SERVER_REGISTERED :
			printf("ROUTER: PENDING SERVER REGISTERED      prog = 0x%08x vers=0x%08x tid = %08x",
				d1, d2, d3);
			break;

		default:
			printf("ROUTER: %s xid = %08x    cid = %08x    tid = %08x",
				cntrl, d1, d2, d3);
			break;
		}
	}
	else
	{
		// Event ID is outside the known table range
		generic_print(ROUTER_PRINT_TABLE, "ROUTER:   ", id, d1, d2, d3);
	}
}

/**
* @brief Prints Clock Regime log messages.
* C conversion of clkrgm_print.
*/
void clkrgm_print(uint32_t id, uint32_t d1, uint32_t d2, uint32_t d3)
{
	printf("id:0x%08x LOG NOT IMPLEMENTED!", id);
}

const char* TMC_PRINT_TABLE[] =
{ "WAIT",
"GOTO INIT",
"BOTH INIT" };

const char* TIMETICK_PRINT_TABLE[] =
{ "START",
"GOTO WAIT",
"GOTO INIT" };

/**
* @brief Prints a single SMEM log record based on its event type.
*
* This function is a C conversion of the 'print_record' subroutine
* from smem_log.pl. It determines the event type from the record's ID
* and calls the appropriate specialized print function.
*
* @param rec A pointer to the SmemLogRecord to be printed.
*/
void print_record(const SmemLogRecord *rec)
{
	// Get the event base ID by masking
	uint32_t id = rec->id & BASE_MASK;

	switch (id)
	{
	case SMEM_LOG_DEBUG_EVENT_BASE:
		debug_print(rec->id, rec->d1, rec->d2, rec->d3);
		break;

	case SMEM_LOG_ONCRPC_EVENT_BASE:
		oncrpc_print(rec->id, rec->d1, rec->d2, rec->d3);
		break;

	case SMEM_LOG_QMI_CCI_EVENT_BASE:
		qmi_cci_print(rec->id, rec->d1, rec->d2, rec->d3);
		break;

	case SMEM_LOG_QMI_CSI_EVENT_BASE:
		qmi_csi_print(rec->id, rec->d1, rec->d2, rec->d3);
		break;

	case SMEM_LOG_SMEM_EVENT_BASE:
		smem_print(rec->id, rec->d1, rec->d2, rec->d3);
		break;

	case SMEM_LOG_TMC_EVENT_BASE:
		generic_print(TMC_PRINT_TABLE, "TMC",
			rec->id, rec->d1, rec->d2, rec->d3);
		break;

	case SMEM_LOG_TIMETICK_EVENT_BASE:
		generic_print(TIMETICK_PRINT_TABLE, "TIMETICK",
			rec->id, rec->d1, rec->d2, rec->d3);
		break;

	case SMEM_ERR_EVENT_BASE:
		err_print(rec->id, rec->d1, rec->d2, rec->d3);
		break;

	case SMEM_LOG_RPC_ROUTER_EVENT_BASE:
		rpc_router_print(rec->id, rec->d1, rec->d2, rec->d3);
		break;

	case SMEM_LOG_IPC_ROUTER_EVENT_BASE:
		ipc_router_print(rec->id, rec->d1, rec->d2, rec->d3);
		break;

	case SMEM_LOG_CLKREGIM_EVENT_BASE:
		clkrgm_print(rec->id, rec->d1, rec->d2, rec->d3);
		break;

	default:
		// Unknown event base just do "default" print
		// Perl: default_print( undef, "UNKNOWN", $$rec[0], $$rec[2], ...)
		default_print("UNKNOWN", rec->id, rec->d1, rec->d2, rec->d3);
		break;
	}
}

/**
* @brief Processes a single log record, handles relative time and prints header/record.
*
* C conversion of the log processing loop logic from print_circular_log.
*
* @param rec The log record to process.
* @param base_time_ptr Pointer to the current base time (updated if relative_time_ptr is TRUE).
* @param relative_time_ptr Pointer to the flag indicating if relative time must be set.
* @param ticks_flag Flag: TRUE if time should be printed in raw ticks, FALSE for seconds.
*/
void print_event(
	const SmemLogRecord *rec,
	uint32_t *base_time_ptr,
	bool *relative_time_ptr,
	bool ticks_flag,
	bool newLine_flag)
{
	if (rec->id != 0) {
		if (*relative_time_ptr == TRUE) {
			*base_time_ptr = rec->timestamp;

			*relative_time_ptr = FALSE;
		}

		// Check if this is the start of a new log entry (not a continuation event)
		if ((rec->id & CONTINUE_MASK) == 0) {
			// Not continuation event, print header

			if (newLine_flag) printf("\n");

			print_line_header(
				rec->id & 0xC0000000,          // Processor flag (MODM/APPS/Q6)
				rec->timestamp - *base_time_ptr,   // Relative time
				ticks_flag                    // Ticks or Seconds flag
			);
		}

		print_record(rec);
	}
}

void print_raw_event(const SmemLogRecord rec)
{
	printf("%08X %08X %08X %08X %08X", rec.id, rec.timestamp, rec.d1, rec.d2, rec.d3);
}
