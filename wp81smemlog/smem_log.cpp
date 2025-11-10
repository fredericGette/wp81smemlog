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
	// Corresponds to @smsm_host array in Perl
	static const char *smsm_host[] = {
		"APPS",
		"MODM",
		"ADSP",
		"WCNS",
		"DSPS"
	};
	// Calculate array size for bounds checking
	const unsigned int num_smsm_hosts = sizeof(smsm_host) / sizeof(smsm_host[0]);

	const char *host_name;
	uint32_t event = id & LSB_MASK;

	switch (event)
	{
	case 100: // PROXY dispatch
		printf("PROXY: dispatch to %08x, queue len = %d", d1, d2);
		break;

	case 1: // SMSM Rx Intr
			// Add bounds check for safety in C
		host_name = (d1 < num_smsm_hosts) ? smsm_host[d1] : "UNKNOWN";
		printf("SMSM Rx Intr: From = %s, Shadow=%08x,  Shared=%08x", host_name, d2, d3);
		break;

	case 2: // SMSM Tx Intr
		host_name = (d1 < num_smsm_hosts) ? smsm_host[d1] : "UNKNOWN";
		printf("SMSM Tx Intr: To  =  %s, Shadow=%08x,  Shared=%08x", host_name, d2, d3);
		break;

	case 3: // QMUX RX
		if ((id & CONTINUE_MASK) == 0)
		{
			printf("QMUX: RX, len: %d, ctl_flags: 0x%02x, svc_type: %d, ", d1, d2, d3);
		}
		else
		{
			// Format matches the Perl script's bitwise operations
			printf("clint_id: %d, qmi_inst: %d; msg_cntl: 0x%02x, tx_id: %d",
				d1, (d2 >> 16), (d2 & 0xFFFF), d3);
		}
		break;

	case 4: // QMUX TX
		if ((id & CONTINUE_MASK) == 0)
		{
			printf("QMUX: TX, len: %d, ctl_flags: 0x%02x, svc_type: %d, ", d1, d2, d3);
		}
		else
		{
			printf("clint_id: %d, qmi_inst: %d; msg_cntl: 0x%02x, tx_id: %d",
				d1, (d2 >> 16), (d2 & 0xFFFF), d3);
		}
		break;

	default: // Generic DEBUG
		if ((id & CONTINUE_MASK) == 0)
		{
			printf("DEBUG: 0x%08x 0x%08x 0x%08x ", d1, d2, d3);
		}
		else
		{
			// This is a continuation line, print without the "DEBUG:" prefix
			printf("0x%08x 0x%08x 0x%08x ", d1, d2, d3);
		}
		break;
	}
}

/**
* @brief Enum for indices of the RPC_PRINT_TABLE.
* Based on the array initialization in the Perl BEGIN block.
*/
typedef enum {
	RPC_GOTO_SMD_WAIT = 0,
	RPC_GOTO_RPC_WAIT,
	RPC_GOTO_RPC_BOTH_WAIT,
	RPC_GOTO_RPC_INIT,
	RPC_GOTO_RUNNING,
	RPC_APIS_INITED,
	RPC_AMSS_RESET,
	RPC_SMD_RESET,
	RPC_ONCRPC_RESET,
	RPC_EVENT_CB,
	RPC_STD_CALL,
	RPC_STD_REPLY,
	RPC_STD_CALL_ASYNC,
	RPC_ERR_NO_PROC,
	RPC_ERR_DECODE,
	RPC_ERR_SYSTEM,
	RPC_ERR_AUTH,
	RPC_ERR_NO_PROG,
	RPC_ERR_PROG_LOCK,
	RPC_ERR_PROG_VERS,
	RPC_ERR_VERS_MISMATCH,
	RPC_CALL_START,
	RPC_DISPATCH_PROXY,
	RPC_HANDLE_CALL,
	RPC_MSG_DONE,
	// Aliases mentioned in the script's logic (NBS_*)
	NBS_CALL = RPC_STD_CALL,
	NBS_REPLY = RPC_STD_REPLY,
	NBS_CALL_ASYNC = RPC_STD_CALL_ASYNC,
	NBS_START = RPC_CALL_START,
	NBS_DISPATCH = RPC_DISPATCH_PROXY,
	NBS_HANDLE = RPC_HANDLE_CALL,
	NBS_DONE = RPC_MSG_DONE
} RpcEventId;

// --- Global Data Tables (from Perl BEGIN) ---

const char *RPC_PRINT_TABLE[] = {
	"GOTO SMD WAIT",
	"GOTO RPC WAIT",
	"GOTO RPC BOTH WAIT",
	"GOTO RPC INIT",
	"GOTO RUNNING",
	"APIS INITED",
	"AMSS RESET - GOTO SMD WAIT",
	"SMD RESET - GOTO SMD WAIT",
	"ONCRPC RESET - GOTO SMD WAIT",
	"EVENT_CB", // Placeholder, handled by logic
	"STD_CALL", // Placeholder, handled by logic
	"STD_REPLY", // Placeholder, handled by logic
	"STD_CALL_ASYNC", // Placeholder, handled by logic
	"ERR_NO_PROC", // Placeholder, handled by logic
	"ERR_DECODE", // Placeholder, handled by logic
	"ERR_SYSTEM", // Placeholder, handled by logic
	"ERR_AUTH", // Placeholder, handled by logic
	"ERR_NO_PROG", // Placeholder, handled by logic
	"ERR_PROG_LOCK", // Placeholder, handled by logic
	"ERR_PROG_VERS", // Placeholder, handled by logic
	"ERR_VERS_MISMATCH", // Placeholder, handled by logic
	"CALL_START", // Placeholder, handled by logic
	"DISPATCH_PROXY", // Placeholder, handled by logic
	"HANDLE_CALL", // Placeholder, handled by logic
	"MSG_DONE" // Placeholder, handled by logic
};

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


// --- Constants (SMSM Flags) ---

// Processor state flags (4 bits)
#define SMSM_APPS_MODEM_SYNC_STATE_BMSK  0x0000000F
#define SMSM_MODEM_STATE_SHFT           0x0
#define SMSM_APPS_STATE_SHFT            0x4

// Inter-processor interrupt (IPI) flags (16 bits)
#define SMSM_IPI_MODEM_APPS_BMSK        0x000000FF
#define SMSM_IPI_APPS_MODEM_BMSK        0x0000FF00
#define SMSM_IPI_APPS_MODEM_SHFT        8

// Reset flags (2 bits)
#define SMSM_RESET_APPS_BMSK            0x00010000
#define SMSM_RESET_MODEM_BMSK           0x00020000

// Others
#define SMSM_RPM_ACTIVE_BMSK            0x00040000
#define SMSM_SLEEP_VOTE_BMSK            0x00080000
#define SMSM_SMD_EXIT_MASK              0x00100000
#define SMSM_PROXY_ACTIVE_BMSK          0x00200000
#define SMSM_SHARING_ACTIVE_BMSK        0x00400000
#define SMSM_SENSORS_STATE_BMSK         0x00800000
#define SMSM_QMI_FWD_BMSK               0x01000000
#define SMSM_SSR_SHUTDOWN_BMSK          0x02000000
#define SMSM_MPU_SHUTDOWN_BMSK          0x04000000
#define SMSM_APPS_DEBUG_BMSK            0x08000000
#define SMSM_WCNSS_STATE_BMSK           0x10000000

// --- Helper Data Tables ---

const char *SMSM_STATE_NAMES[] = {
	"OFF",
	"DOWN",
	"BOOT",
	"OOS",
	"SSR",
	"READY",
	"ERR",
	"RST"
};
const unsigned int num_smsm_state_names =
sizeof(SMSM_STATE_NAMES) / sizeof(SMSM_STATE_NAMES[0]);

// --- Core Function ---

/**
* @brief Converts the SMSM 32-bit state mask into a human-readable string.
* C conversion of smsm_state_to_string.
* * @param state The 32-bit state mask.
* @return A static string buffer containing the formatted state description.
*/
const char *smsm_state_to_string(uint32_t state)
{
	// Max size needed:
	// "APPS:READY | MODEM:READY | IPI_M2A:0xFF | IPI_A2M:0xFF | RESET_M | RPM_ACT | SLEEP_V | SMD_E | PROXY_A | SHR_A | SENSORS | QMI_FWD | SSR_SHD | MPU_SHD | APPS_DBG | WCNSS"
	// ~150 characters, 256 is safe.
#define STATE_BUF_SIZE 256
	static char state_buf[STATE_BUF_SIZE];

	// Use a temporary pointer to append to the buffer
	char *p = state_buf;
	size_t remaining = STATE_BUF_SIZE;
	int len;

	// Clear the buffer
	*p = '\0';

	// Helper to append a string, checking buffer bounds
#define APPEND_STR(str) \
        len = _snprintf_s(p, remaining, _TRUNCATE, "%s", str); \
        if (len < 0 || (size_t)len >= remaining) { \
            if (len < 0) { len = 0; } \
            p += remaining - 1; \
            *p = '\0'; \
            remaining = 0; \
        } else { \
            p += len; \
            remaining -= len; \
        }

	// Helper to append a formatted string (used for IPI values)
#define APPEND_FMT(fmt, ...) \
        len = _snprintf_s(p, remaining, _TRUNCATE, fmt, __VA_ARGS__); \
        if (len < 0 || (size_t)len >= remaining) { \
            if (len < 0) { len = 0; } \
            p += remaining - 1; \
            *p = '\0'; \
            remaining = 0; \
        } else { \
            p += len; \
            remaining -= len; \
        }


	// 1. APPS State (Bits 4-7)
	uint32_t apps_state_idx = (state >> SMSM_APPS_STATE_SHFT) & SMSM_APPS_MODEM_SYNC_STATE_BMSK;
	const char *apps_state_name = (apps_state_idx < num_smsm_state_names)
		? SMSM_STATE_NAMES[apps_state_idx]
		: "INV";
	APPEND_FMT("APPS:%s", apps_state_name);

	// 2. MODEM State (Bits 0-3)
	uint32_t modem_state_idx = (state >> SMSM_MODEM_STATE_SHFT) & SMSM_APPS_MODEM_SYNC_STATE_BMSK;
	const char *modem_state_name = (modem_state_idx < num_smsm_state_names)
		? SMSM_STATE_NAMES[modem_state_idx]
		: "INV";
	APPEND_FMT(" | MODEM:%s", modem_state_name);

	// 3. IPI MODEM -> APPS (Bits 0-7 of IPI mask)
	uint32_t ipi_m2a = (state >> 16) & SMSM_IPI_MODEM_APPS_BMSK;
	APPEND_FMT(" | IPI_M2A:0x%02x", ipi_m2a);

	// 4. IPI APPS -> MODEM (Bits 8-15 of IPI mask)
	uint32_t ipi_a2m = (state >> (16 + SMSM_IPI_APPS_MODEM_SHFT)) & SMSM_IPI_MODEM_APPS_BMSK;
	APPEND_FMT(" | IPI_A2M:0x%02x", ipi_a2m);

	// 5. Reset Flags (Bit 16 & 17)
	if (state & SMSM_RESET_APPS_BMSK) {
		APPEND_STR(" | RESET_A");
	}
	if (state & SMSM_RESET_MODEM_BMSK) {
		APPEND_STR(" | RESET_M");
	}

	// 6. Other Flags (Bit 18 onwards)
	if (state & SMSM_RPM_ACTIVE_BMSK) {
		APPEND_STR(" | RPM_ACT");
	}
	if (state & SMSM_SLEEP_VOTE_BMSK) {
		APPEND_STR(" | SLEEP_V");
	}
	if (state & SMSM_SMD_EXIT_MASK) {
		APPEND_STR(" | SMD_E");
	}
	if (state & SMSM_PROXY_ACTIVE_BMSK) {
		APPEND_STR(" | PROXY_A");
	}
	if (state & SMSM_SHARING_ACTIVE_BMSK) {
		APPEND_STR(" | SHR_A");
	}
	if (state & SMSM_SENSORS_STATE_BMSK) {
		APPEND_STR(" | SENSORS");
	}
	if (state & SMSM_QMI_FWD_BMSK) {
		APPEND_STR(" | QMI_FWD");
	}
	if (state & SMSM_SSR_SHUTDOWN_BMSK) {
		APPEND_STR(" | SSR_SHD");
	}
	if (state & SMSM_MPU_SHUTDOWN_BMSK) {
		APPEND_STR(" | MPU_SHD");
	}
	if (state & SMSM_APPS_DEBUG_BMSK) {
		APPEND_STR(" | APPS_DBG");
	}
	if (state & SMSM_WCNSS_STATE_BMSK) {
		APPEND_STR(" | WCNSS");
	}

	return state_buf;
}

#undef APPEND_STR
#undef APPEND_FMT

/**
* @brief (Stub) Gets the program name from its ID.
* The Perl script dynamically populates this from P4, which is complex.
* This stub replicates the fallback behavior of printing the hex ID.
*/
const char *GetProgramName(uint32_t prog_id) {
	static char prog_name_buf[11]; // "0x" + 8 hex digits + null
	int result = _snprintf_s(prog_name_buf, sizeof(prog_name_buf), _TRUNCATE, "0x%08x", prog_id);
	// If using _snprintf, manually ensure null termination if the buffer was filled
	if (result >= 0 && (size_t)result >= sizeof(prog_name_buf)) {
		prog_name_buf[sizeof(prog_name_buf) - 1] = '\0';
	}

	return prog_name_buf;
}

/**
* @brief Prints QMI_CCI log messages.
* C conversion of qmi_cci_print.
*/
void qmi_cci_print(uint32_t id, uint32_t d1, uint32_t d2, uint32_t d3) {
	// Replicates Perl's stateful globals for multi-part messages
	static uint32_t QCCI_ERR_DATA1 = '.';
	static uint32_t QCCI_ERR_DATA2 = '.';
	static uint32_t QCCI_ERR_DATA3 = '.';

	uint32_t cont = id & CONTINUE_MASK;
	uint32_t event_id = id & 0xffff;

	if (event_id == 0x3) { // ERROR
		if (cont == 0) {
			QCCI_ERR_DATA1 = d1;
			QCCI_ERR_DATA2 = d2;
			QCCI_ERR_DATA3 = d3;
		}
		else {
			// Perl casts the full 32-bit int to char, we replicate that
			printf("QCCI:   ERROR File = %c%c%c%c%c, Line=%d",
				(char)QCCI_ERR_DATA1, (char)QCCI_ERR_DATA2, (char)QCCI_ERR_DATA3,
				(char)d1, (char)d2, d3);
		}
	}
	else if (event_id == 0x0 || event_id == 0x1) { // Legacy TX/RX
		const char *type = QMI_PRINT_TABLE[event_id];
		printf("QCCI:   $s %s Txn:0x%x Msg:0x%x Len:%d",
			type, QMI_CNTL_PRINT_TABLE[d1 >> 16], d1 & 0xffff, d2 >> 16, d2 & 0xffff);
	}
	else if (event_id == 0x4 || event_id == 0x5) { // Extended TX/RX
		const char *type = QMI_PRINT_TABLE[event_id - 0x4];
		if (cont == 0) {
			printf("QCCI:   %s %s Txn:0x%x Msg:0x%x Len:%d svc_id:0x%x",
				type, QMI_CNTL_PRINT_TABLE[d1 >> 16], d1 & 0xffff,
				d2 >> 16, d2 & 0xffff, d3);
		}
		else {
			printf(" svc_addr: %04x:%04x:%04x", d1, d2, d3);
		}
	}
}

/**
* @brief Prints QMI_CSI log messages.
* C conversion of qmi_csi_print.
*/
void qmi_csi_print(uint32_t id, uint32_t d1, uint32_t d2, uint32_t d3) {
	// Replicates Perl's stateful globals for multi-part messages
	static uint32_t QCSI_ERR_DATA1 = '.';
	static uint32_t QCSI_ERR_DATA2 = '.';
	static uint32_t QCSI_ERR_DATA3 = '.';

	uint32_t cont = id & CONTINUE_MASK;
	uint32_t event_id = id & 0xffff;

	if (event_id == 0x3) { // ERROR
		if (cont == 0) {
			QCSI_ERR_DATA1 = d1 & 0xff; // Perl script masks these
			QCSI_ERR_DATA2 = d2 & 0xff;
			QCSI_ERR_DATA3 = d3 & 0xff;
		}
		else {
			printf("QCSI:   ERROR File = %c%c%c%c%c, Line=%d",
				(char)QCSI_ERR_DATA1, (char)QCSI_ERR_DATA2, (char)QCSI_ERR_DATA3,
				(char)(d1 & 0xff), (char)(d2 & 0xff), d3);
		}
	}
	else if (event_id == 0x0 || event_id == 0x1) { // Legacy TX/RX
		const char *type = QMI_PRINT_TABLE[event_id];
		printf("QCSI:   %s %s Txn:0x%x Msg:0x%x Len:%d",
			type, QMI_CNTL_PRINT_TABLE[d1 >> 16], d1 & 0xffff, d2 >> 16, d2 & 0xffff);
	}
	else if (event_id == 0x4 || event_id == 0x5) { // Extended TX/RX
		const char *type = QMI_PRINT_TABLE[event_id - 0x4];
		if (cont == 0) {
			printf("QCSI:   %s %s Txn:0x%x Msg:0x%x Len:%d svc_id:0x%x",
				type, QMI_CNTL_PRINT_TABLE[d1 >> 16], d1 & 0xffff,
				d2 >> 16, d2 & 0xffff, d3);
		}
		else {
			printf(" clnt_addr: %04x:%04x:%04x", d1, d2, d3);
		}
	}
}

/**
* @brief Replicates Perl's pack("I3", ...) and string extraction.
* Safely copies 12 bytes from three uint32_t values into a
* null-terminated string.
*/
static void print_task_name(uint32_t d1, uint32_t d2, uint32_t d3) {
	char task_name[13]; // 12 bytes (3 * 4) + null terminator
	uint32_t data[3] = { d1, d2, d3 };

	// Copy the 12 bytes of data
	memcpy(task_name, data, 12);

	// Ensure null termination
	task_name[12] = '\0';

	// The Perl script does s/\0.*//, so we just print it as a C string
	printf("    task = %s", task_name);
}

// --- Constants from smem_log.pl ---
#define PROC_MASK     0xC0000000

// Assume a global buffer for the line header, 
// as implied by its use in previous functions (e.g., oncrpc_print, smem_print).
// This must be defined globally in a shared header or source file.
#define LINE_HEADER_SIZE 256
char LINE_HEADER[LINE_HEADER_SIZE];

// Global variables defined in the Perl script for time conversion
#define HT_TIMER_CLOCK_RATE 19200000 // HT Timer
#define SLEEP_CLOCK_RATE = 32768 // Sleep clock
#define TIMESTAMP_CLOCK_RATE HT_TIMER_CLOCK_RATE // Default to HT Timer

/**
* @brief Prints the log line header (time and processor/flag info).
*
* This function is a C conversion of the 'print_line_header' subroutine.
* It formats the processor ID and time information and stores it in the
* global LINE_HEADER buffer.
*
* @param proc_flag The 32-bit flag containing the processor identifier (0xC0000000 mask).
* @param time The timestamp of the log event (relative or absolute).
* @param ticks A flag: TRUE (1) if time is in clock ticks, FALSE (0) if in seconds.
*/
void print_line_header(uint32_t proc_flag, uint32_t time, bool ticks)
{
	const char *proc_name;
	double sec_time;

	// Determine the processor name
	switch (proc_flag & PROC_MASK) {
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
		// Perl: $sec_time = $time / ($Q6_CLOCK_FREQ_KHZ * 1000);
		// Assuming Q6_CLOCK_FREQ_KHZ is defined globally and set to 19200 (19.2MHz)
		// or $Q6_TICKS_PER_SEC is defined and set to 32768.

		if (TIMESTAMP_CLOCK_RATE != 0) {
			sec_time = (double)time / TIMESTAMP_CLOCK_RATE;
		}
		else {
			// Fallback for division by zero (should not happen if constants are correct)
			sec_time = (double)time;
		}

		// Format the output into the global LINE_HEADER buffer
		// Perl: sprintf( "%10.4f %4s ", $sec_time, $proc_name );
		int result = _snprintf_s(LINE_HEADER, LINE_HEADER_SIZE, _TRUNCATE, "\n%4s: %14.6f    ", proc_name, sec_time);
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
	const char *event_name = NULL;
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

	if (event < table_size) {
		event_name = table[event];
	}

	if ((id & CONTINUE_MASK) == 0) {
		// Print the primary log message
		if (event_name != NULL) {
			printf("%s: %s [0x%04x] 0x%08x 0x%08x 0x%08x",
				name, event_name, event, d1, d2, d3);
		}
		else {
			printf("%s: UNKNOWN [0x%04x] 0x%08x 0x%08x 0x%08x",
				name, event, d1, d2, d3);
		}
	}
	else {
		// Print the continuation log message
		// The Perl script does NOT print the subsystem name for continuation events.
		printf("0x%08x 0x%08x 0x%08x", d1, d2, d3);
	}
}


/**
* @brief Prints ONCRPC log messages.
* C conversion of oncrpc_print.
*/
void oncrpc_print(uint32_t id, uint32_t d1, uint32_t d2, uint32_t d3) {
	uint32_t subsys = id & 0xf0;
	uint32_t event = id & LSB_MASK;
	// Note: C array indices are 0-based.
	// The Perl script's logic maps directly to the enum values.
	RpcEventId cntrl = (RpcEventId)event;
	const char *prog = GetProgramName(d2);

	if (subsys) {
		// BW Compatible print for QCCI & QCSI
		if (subsys == 0x30) {
			qmi_cci_print(id & 0xffffff0f, d1, d2, d3);
		}
		else {
			qmi_csi_print(id & 0xffffff0f, d1, d2, d3);
		}
	}
	else if (cntrl == RPC_ERR_NO_PROC) {
		printf("ONCRPC: ERROR PROC NOT SUPPORTED   xid = %08x    proc = %3d    prog = %s",
			d1, d3, prog);
	}
	else if (cntrl == RPC_ERR_DECODE) {
		printf("ONCRPC: ERROR ARGS DECODE FAILED   xid = %08x    proc = %3d    prog = %s",
			d1, d3, prog);
	}
	else if (cntrl == RPC_ERR_SYSTEM) {
		printf("ONCRPC: ERROR SYSTEM FAULT  xid = %08x    proc = %3d    prog = %s",
			d1, d3, prog);
	}
	else if (cntrl == RPC_ERR_AUTH) {
		printf("ONCRPC: ERROR AUTHENTICATION FAILED  xid = %08x    proc = %3d    prog = %s",
			d1, d3, prog);
	}
	else if (cntrl == RPC_ERR_NO_PROG) {
		printf("ONCRPC: ERROR PROG NOT EXPORTED  xid = %08x    proc = %3d    prog = %s",
			d1, d3, prog);
	}
	else if (cntrl == RPC_ERR_PROG_LOCK) {
		printf("ONCRPC: ERROR PROG LOCKED  xid = %08x    proc = %3d    prog = %s",
			d1, d3, prog);
	}
	else if (cntrl == RPC_ERR_PROG_VERS) {
		printf("ONCRPC: ERROR PROG VERS NOT SUPPORED  xid = %08x    proc = %3d    prog = %s",
			d1, d3, prog);
	}
	else if (cntrl == RPC_ERR_VERS_MISMATCH) {
		printf("ONCRPC: ERROR RPC VERS NOT SUPPORTED   xid = %08x    proc = %3d    prog = %s",
			d1, d3, prog);
	}
	else if (cntrl == RPC_EVENT_CB) {
		printf("ONCRPC: EVENT_CB  %s ", (d1 == 1) ? "LOCAL" : "REMOTE");
		// Assumes LINE_HEADER is a global char buffer, as in the Perl script
		printf("\n%sONCRPC: prev = %s", LINE_HEADER, smsm_state_to_string(d2));
		printf("\n%sONCRPC: curr = %s", LINE_HEADER, smsm_state_to_string(d3));
	}
	else if (cntrl == RPC_STD_CALL) {
		if ((id & CONTINUE_MASK) == 0) {
			printf("ONCRPC: CALL    xid = %08x    proc = %3d    prog = %s",
				d1, d3, prog);
		}
		else {
			print_task_name(d1, d2, d3);
		}
	}
	else if (cntrl == RPC_STD_REPLY) {
		if ((id & CONTINUE_MASK) == 0) {
			printf("ONCRPC: REPLY   xid = %08x    proc = %3d    prog = %s", d1, d3, prog);
		}
		else {
			print_task_name(d1, d2, d3);
		}
	}
	else if (cntrl == RPC_STD_CALL_ASYNC) {
		if ((id & CONTINUE_MASK) == 0) {
			printf("ONCRPC: ASYNC   xid = %08x    proc = %3d    prog = %s",
				d1, d3, prog);
		}
		else {
			print_task_name(d1, d2, d3);
		}
	}
	else if (cntrl == RPC_CALL_START) {
		if ((id & CONTINUE_MASK) == 0) {
			printf("ONCRPC: START   xid = %08x    proc = %3d    prog = %s",
				d1, d3, prog);
		}
		else {
			print_task_name(d1, d2, d3);
		}
	}
	else if (cntrl == RPC_DISPATCH_PROXY) {
		printf("ONCRPC: DISPATCH  xid = %08x    func = %08x", d1, d2);
	}
	else if (cntrl == RPC_HANDLE_CALL) {
		if ((id & CONTINUE_MASK) == 0) {
			printf("ONCRPC: HANDLE  xid = %08x    proc = %3d    prog = %s",
				d1, d3, prog);
		}
		else {
			print_task_name(d1, d2, d3);
		}
	}
	else if (cntrl == RPC_MSG_DONE) {
		if ((id & CONTINUE_MASK) == 0) {
			printf("ONCRPC: DONE    xid = %08x    proc = %3d    prog = %s",
				d1, d3, prog);
		}
		else {
			print_task_name(d1, d2, d3);
		}
	}
	else {
		generic_print(RPC_PRINT_TABLE, "ONCRPC", id, d1, d2, d3);
	}
}

/**
* @brief Enum for indices of the SMEM_PRINT_TABLE.
* Based on the array initialization in the Perl BEGIN block.
*/
typedef enum {
	SMEM_EVENT_CB = 0,
	SMEM_START,
	SMEM_INIT,
	SMEM_RUNNING,
	SMEM_STOP,
	SMEM_RESTART,
	SMEM_EVENT_SS,
	SMEM_EVENT_READ,
	SMEM_EVENT_WRITE,
	SMEM_EVENT_SIGS1,
	SMEM_EVENT_SIGS2,
	SMEM_WRITE_DM,
	SMEM_READ_DM,
	SMEM_SKIP_DM,
	SMEM_STOP_DM,
	SMEM_ISR,
	SMEM_TASK,
	SMEM_EVENT_RS
	// Note: 0x2000 and 0xbcbc are handled as special cases
} SmemEventId;

// --- Global Data Tables (from Perl BEGIN) ---

// Corresponds to @smem_stream_state_names
const char *smem_stream_state_names[] = {
	"CLOSED", "OPENING", "OPENED",
	"FLUSHING", "CLOSING", "RESET",
	"RESET_OPENING"
};
const unsigned int num_smem_stream_state_names =
sizeof(smem_stream_state_names) / sizeof(smem_stream_state_names[0]);

// Corresponds to @smem_stream_event_names
const char *smem_stream_event_names[] = {
	"CLOSE", "OPEN", "REMOTE OPEN",
	"REMOTE CLOSE", "FLUSH",
	"FLUSH COMPLETE", "REMOTE_REOPEN",
	"REMOTE_RESET"
};
const unsigned int num_smem_stream_event_names =
sizeof(smem_stream_event_names) / sizeof(smem_stream_event_names[0]);

// Corresponds to $SMEM_PRINT_TABLE
const char *SMEM_PRINT_TABLE[] = {
	"EVENT_CB", // Placeholder, handled by logic
	"START",
	"INIT",
	"RUNNING",
	"STOP",
	"RESTART",
	"EVENT_SS", // Placeholder, handled by logic
	"EVENT_READ", // Placeholder, handled by logic
	"EVENT_WRITE", // Placeholder, handled by logic
	"EVENT_SIGS1", // Placeholder, handled by logic
	"EVENT_SIGS2", // Placeholder, handled by logic
	"WRITE DM",
	"READ DM",
	"SKIP DM",
	"STOP DM",
	"ISR",
	"TASK",
	"EVENT_RS" // Placeholder, handled by logic
};
const unsigned int num_smem_print_table =
sizeof(SMEM_PRINT_TABLE) / sizeof(SMEM_PRINT_TABLE[0]);

// --- Helper functions for safe array access ---

/**
* @brief Safely gets a stream state name.
* @param index The index from the log.
* @return A string (either the name or "UNKNOWN").
*/
static const char* get_stream_state_name(uint32_t index) {
	if (index < num_smem_stream_state_names) {
		return smem_stream_state_names[index];
	}
	return "UNKNOWN";
}

/**
* @brief Safely gets a stream event name.
* @param index The index from the log.
* @return A string (either the name or "UNKNOWN").
*/
static const char* get_stream_event_name(uint32_t index) {
	if (index < num_smem_stream_event_names) {
		return smem_stream_event_names[index];
	}
	return "UNKNOWN";
}

/**
* @brief Prints SMEM log messages.
* C conversion of smem_print.
*/
void smem_print(uint32_t id, uint32_t d1, uint32_t d2, uint32_t d3)
{
	uint32_t event = id & LSB_MASK;

	// Use a switch for cleaner logic
	switch (event)
	{
	case SMEM_EVENT_CB:
		printf("SMEM:   EVENT_CB  %s ",
			(d1 == 1) ? "LOCAL" : "REMOTE");
		// Assumes LINE_HEADER is a global char buffer, as in the Perl script
		printf("\n%sSMEM:   prev = %s ", LINE_HEADER, smsm_state_to_string(d2));
		printf("\n%sSMEM:   curr = %s ", LINE_HEADER, smsm_state_to_string(d3));
		break;

	case SMEM_EVENT_SS:
		printf("SMEM: EVENT SS  port %d  state = %s  event = %s",
			d1,
			get_stream_state_name(d2),
			get_stream_event_name(d3));
		break;

	case SMEM_EVENT_READ:
		if (d1 == 0) {
			printf("SMEM: EVENT READ enter port %d", d2);
		}
		else {
			printf("SMEM: EVENT READ exit port %d (%d)", d2, d1);
		}
		break;

	case SMEM_EVENT_WRITE:
		if (d1 == 0) {
			printf("SMEM: EVENT WRITE enter port %d", d2);
		}
		else {
			printf("SMEM: EVENT WRITE exit port %d (%d)", d2, d1);
		}
		break;

	case SMEM_EVENT_SIGS1:
		printf("SMEM: EVENT SIGS  port %d received %08x converted to %08x",
			d1, d2, d3);
		break;

	case SMEM_EVENT_SIGS2:
		printf("SMEM: EVENT_SIGS  port %d  old = %08x  new = %08x",
			d1, d2, d3);
		break;

	case SMEM_EVENT_RS:
		printf("SMEM: EVENT_REMOTE_STATE  port %d  old = %s  new = %s",
			d1,
			get_stream_state_name(d2),
			get_stream_state_name(d3));
		break;

		// --- Special event IDs handled in Perl's elsif ---
	case 0x2000:
		printf("SMEM: EVENT WRITE port %d  write_index = %d  byte_count = %d",
			d1, d2, d3);
		break;

	case 0xbcbc:
		printf("SMEM: SMDI INIT  Channel table entry = %d bytes",
			d1);
		break;

		// --- Default case for simple table lookups ---
	default:
		// Check if it's a simple, known event
		if (event < num_smem_print_table)
		{
			generic_print(SMEM_PRINT_TABLE, "SMEM", id, d1, d2, d3);
		}
		else
		{
			// Truly unknown event
			printf("SMEM: UNKNOWN [0x%04x] 0x%08x 0x%08x 0x%08x",
				event, d1, d2, d3);
		}
		break;
	}
}


/**
* @brief Enum for indices of the ERR_PRINT_TABLE.
* Based on the array initialization in the Perl BEGIN block.
*/
typedef enum {
	ERR_UNUSED = 0,
	ERR_EVENT_FATAL,
	ERR_EVENT_FATAL_TASK
} ErrEventId;

// --- Global Data Table (from Perl BEGIN) ---
const char *ERR_PRINT_TABLE[] = {
	"UNUSED",
	"EVENT_FATAL",      // Placeholder, handled by logic
	"EVENT_FATAL_TASK"  // Placeholder, handled by logic
};
const unsigned int num_err_print_table =
sizeof(ERR_PRINT_TABLE) / sizeof(ERR_PRINT_TABLE[0]);

/**
* @brief Replicates Perl's pack("I5", ...) and prints a file name.
* Safely copies 20 bytes from five uint32_t values into a
* null-terminated string.
*/
static void print_fatal_file(uint32_t d1_state, uint32_t d2_state, uint32_t d3_state,
	uint32_t d1_cont, uint32_t d2_cont, uint32_t d3_cont)
{
	// 5 * 4-byte integers = 20 bytes. +1 for null terminator.
	char name_buf[21];
	uint32_t data[5] = { d1_state, d2_state, d3_state, d1_cont, d2_cont };

	// Copy the 20 bytes of data
	memcpy(name_buf, data, 20);

	// Ensure null termination
	name_buf[20] = '\0';

	// The Perl script does s/\0.*//, which printing a C string handles implicitly.
	// d3_cont is the line number.
	printf(" file: \"%s\" line: %d", name_buf, d3_cont);
}

/**
* @brief Replicates Perl's pack("I5", ...) and prints a task name.
* Safely copies 20 bytes from five uint32_t values into a
* null-terminated string.
*/
static void print_fatal_task(uint32_t d1_state, uint32_t d2_state, uint32_t d3_state,
	uint32_t d1_cont, uint32_t d2_cont, uint32_t d3_cont)
{
	// 5 * 4-byte integers = 20 bytes. +1 for null terminator.
	char name_buf[21];
	uint32_t data[5] = { d1_state, d2_state, d3_state, d1_cont, d2_cont };

	// Copy the 20 bytes of data
	memcpy(name_buf, data, 20);

	// Ensure null termination
	name_buf[20] = '\0';

	// d3_cont is the tcb addr.
	printf(" task: \"%s\" tcb addr: 0x%08x", name_buf, d3_cont);
}


/**
* @brief Prints Error log messages.
* C conversion of err_print.
*/
void err_print(uint32_t id, uint32_t d1, uint32_t d2, uint32_t d3)
{
	// Replicates Perl's stateful globals for multi-part messages
	static uint32_t ERR_DATA1 = 0;
	static uint32_t ERR_DATA2 = 0;
	static uint32_t ERR_DATA3 = 0;

	uint32_t event = id & LSB_MASK;

	switch (event)
	{
	case ERR_EVENT_FATAL: // Event 1
		if ((id & CONTINUE_MASK) == 0)
		{
			// First ERR record
			printf("ERR: FATAL ");
			// Save state for the continuation record
			ERR_DATA1 = d1;
			ERR_DATA2 = d2;
			ERR_DATA3 = d3;
		}
		else
		{
			// Second (continuation) ERR record
			print_fatal_file(ERR_DATA1, ERR_DATA2, ERR_DATA3, d1, d2, d3);
		}
		break;

	case ERR_EVENT_FATAL_TASK: // Event 2
		if ((id & CONTINUE_MASK) == 0)
		{
			// First ERR record
			printf("ERR: FATAL ");
			// Save state for the continuation record
			ERR_DATA1 = d1;
			ERR_DATA2 = d2;
			ERR_DATA3 = d3;
		}
		else
		{
			// Second (continuation) ERR record
			print_fatal_task(ERR_DATA1, ERR_DATA2, ERR_DATA3, d1, d2, d3);
		}
		break;

	default:
		generic_print(ERR_PRINT_TABLE, "ERR", id, d1, d2, d3);
		break;
	}
}

/**
* @brief Enum for indices of the IPC_ROUTER_PRINT_TABLE.
*/
typedef enum {
	IPC_ROUTER_CALL = 0,
	IPC_ROUTER_REPLY,
	IPC_ROUTER_INDICATION,
	IPC_ROUTER_EVENT,
	IPC_ROUTER_CONNECT_NAME,
	IPC_ROUTER_CONNECT_ADDR,
	IPC_ROUTER_DISCONNECT_ADDR
} IpcRouterEventId;

// --- Data Table (from Perl BEGIN) ---
const char *IPC_ROUTER_PRINT_TABLE[] = {
	"CALL",
	"REPLY",
	"INDICATION",
	"EVENT",
	"CONNECT_NAME",
	"CONNECT_ADDR",
	"DISCONNECT_ADDR"
};
const unsigned int num_ipc_router_print_table =
sizeof(IPC_ROUTER_PRINT_TABLE) / sizeof(IPC_ROUTER_PRINT_TABLE[0]);

/**
* @brief Prints IPC Router log messages.
* C conversion of ipc_router_print.
*/
void ipc_router_print(uint32_t id, uint32_t d1, uint32_t d2, uint32_t d3)
{
	uint32_t event = id & LSB_MASK;
	uint32_t cont = id & CONTINUE_MASK;

	// Check if the event is a known simple index (0-6)
	if (event < num_ipc_router_print_table)
	{
		switch ((IpcRouterEventId)event)
		{
		case IPC_ROUTER_CALL:
			if (cont == 0) {
				printf("IPC_ROUTER: CALL   txn: %08x  svc: %08x  clnt: %08x",
					d1, d2, d3);
			}
			else {
				printf("  msg_id: %08x  data: %08x  data: %08x",
					d1, d2, d3);
			}
			break;

		case IPC_ROUTER_REPLY:
			if (cont == 0) {
				printf("IPC_ROUTER: REPLY  txn: %08x  svc: %08x  clnt: %08x",
					d1, d2, d3);
			}
			else {
				printf("  msg_id: %08x  data: %08x  data: %08x",
					d1, d2, d3);
			}
			break;

		case IPC_ROUTER_INDICATION:
			if (cont == 0) {
				printf("IPC_ROUTER: IND    svc: %08x  clnt: %08x  dest: %08x",
					d1, d2, d3);
			}
			else {
				printf("  msg_id: %08x  data: %08x  data: %08x",
					d1, d2, d3);
			}
			break;

		case IPC_ROUTER_EVENT:
			printf("IPC_ROUTER: EVENT  addr: %08x  type: %08x  data: %08x",
				d1, d2, d3);
			break;

		case IPC_ROUTER_CONNECT_NAME:
			printf("IPC_ROUTER: CONNECT_NAME  svc: %08x  inst: %08x  clnt: %08x",
				d1, d2, d3);
			break;

		case IPC_ROUTER_CONNECT_ADDR:
			printf("IPC_ROUTER: CONNECT_ADDR  addr: %08x  svc: %08x  clnt: %08x",
				d1, d2, d3);
			break;

		case IPC_ROUTER_DISCONNECT_ADDR:
			printf("IPC_ROUTER: DISCONNECT_ADDR  addr: %08x  svc: %08x  clnt: %08x",
				d1, d2, d3);
			break;

		default:
			// Should only happen if the logic is flawed, but safe fallback.
			generic_print(IPC_ROUTER_PRINT_TABLE, "IPC_ROUTER", id, d1, d2, d3);
			break;
		}
	}
	else
	{
		// Event ID is outside the known table range
		generic_print(IPC_ROUTER_PRINT_TABLE, "IPC_ROUTER", id, d1, d2, d3);
	}
}

/**
* @brief Enum for indices of the RPC_ROUTER_PRINT_TABLE.
*/
typedef enum {
	RPC_ROUTER_CALL = 0,
	RPC_ROUTER_REPLY,
	RPC_ROUTER_REPLY_ACK,
	RPC_ROUTER_INDICATION,
	RPC_ROUTER_ACK,
	RPC_ROUTER_EVENT,
	RPC_ROUTER_CALL_ASYNC,
	RPC_ROUTER_REPLY_ACK_ASYNC
} RpcRouterEventId;

// --- Data Table (from Perl BEGIN) ---
const char *RPC_ROUTER_PRINT_TABLE[] = {
	"CALL",
	"REPLY",
	"REPLY_ACK",
	"INDICATION",
	"ACK",
	"EVENT",
	"CALL_ASYNC",
	"REPLY_ACK_ASYNC"
};
const unsigned int num_rpc_router_print_table =
sizeof(RPC_ROUTER_PRINT_TABLE) / sizeof(RPC_ROUTER_PRINT_TABLE[0]);

enum ROUTER_PRINT_TABLE {
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

const char* string_ROUTER_PRINT_TABLE[] = {
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
		const char *cntrl = string_ROUTER_PRINT_TABLE[event];
		switch ((ROUTER_PRINT_TABLE)event)
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
		generic_print(RPC_ROUTER_PRINT_TABLE, "ROUTER:   ", id, d1, d2, d3);
	}
}

/**
* @brief Enum for indices of the CLK_PRINT_TABLE.
*/
typedef enum {
	CLK_EVENT_SET_FREQ = 0,
	CLK_EVENT_SET_FREQ_STATUS,
	CLK_EVENT_SET_FREQ_ERR,
	CLK_EVENT_ENABLE,
	CLK_EVENT_ENABLE_ERR,
	CLK_EVENT_DISABLE,
	CLK_EVENT_DISABLE_ERR,
	CLK_EVENT_ISR,
	CLK_EVENT_TASK,
	CLK_EVENT_RSP,
	CLK_EVENT_STATUS,
	CLK_EVENT_UNKNOWN
} ClkrgmEventId;

// --- Data Table (from Perl BEGIN) ---
const char *CLK_PRINT_TABLE[] = {
	"SET_FREQ",
	"SET_FREQ_STATUS",
	"SET_FREQ_ERR",
	"ENABLE",
	"ENABLE_ERR",
	"DISABLE",
	"DISABLE_ERR",
	"ISR",
	"TASK",
	"RSP",
	"STATUS",
	"UNKNOWN"
};
const unsigned int num_clk_print_table =
sizeof(CLK_PRINT_TABLE) / sizeof(CLK_PRINT_TABLE[0]);

/**
* @brief Prints Clock Regime log messages.
* C conversion of clkrgm_print.
*/
void clkrgm_print(uint32_t id, uint32_t d1, uint32_t d2, uint32_t d3)
{
	uint32_t event = id & LSB_MASK;

	// Check if the event is a known simple index (0-10)
	if (event < num_clk_print_table - 1) // -1 because 'UNKNOWN' is at the end
	{
		switch ((ClkrgmEventId)event)
		{
		case CLK_EVENT_SET_FREQ:
			printf("CLK: SET_FREQ  client: %08x  old_freq: %08x  new_freq: %08x",
				d1, d2, d3);
			break;

		case CLK_EVENT_SET_FREQ_STATUS:
			printf("CLK: SET_FREQ_STATUS  client: %08x  status: %08x  req_freq: %08x",
				d1, d2, d3);
			break;

		case CLK_EVENT_SET_FREQ_ERR:
			printf("CLK: SET_FREQ_ERR  client: %08x  status: %08x  req_freq: %08x",
				d1, d2, d3);
			break;

		case CLK_EVENT_ENABLE:
			printf("CLK: ENABLE  client: %08x  data: %08x  data: %08x",
				d1, d2, d3);
			break;

		case CLK_EVENT_ENABLE_ERR:
			printf("CLK: ENABLE_ERR  client: %08x  status: %08x  data: %08x",
				d1, d2, d3);
			break;

		case CLK_EVENT_DISABLE:
			printf("CLK: DISABLE  client: %08x  data: %08x  data: %08x",
				d1, d2, d3);
			break;

		case CLK_EVENT_DISABLE_ERR:
			printf("CLK: DISABLE_ERR  client: %08x  status: %08x  data: %08x",
				d1, d2, d3);
			break;

		case CLK_EVENT_ISR:
			printf("CLK: ISR  data: %08x  data: %08x  data: %08x",
				d1, d2, d3);
			break;

		case CLK_EVENT_TASK:
			printf("CLK: TASK  data: %08x  data: %08x  data: %08x",
				d1, d2, d3);
			break;

		case CLK_EVENT_RSP:
			printf("CLK: RSP  data: %08x  data: %08x  data: %08x",
				d1, d2, d3);
			break;

		case CLK_EVENT_STATUS:
			printf("CLK: STATUS  data: %08x  data: %08x  data: %08x",
				d1, d2, d3);
			break;

			// No default needed here, as we check bounds before the switch.
		default:
			break;
		}
	}
	else
	{
		// Event ID is outside the known table range or is explicitly 'UNKNOWN' (11)
		generic_print(CLK_PRINT_TABLE, "CLK", id, d1, d2, d3);
	}
}

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
	uint32_t event = id & LSB_MASK;

	// Check the continue mask to determine if it is the primary or continuation line
	if ((id & CONTINUE_MASK) == 0) {
		// Primary log message (prints name and event ID)
		printf("%s: [0x%04x] 0x%08x 0x%08x 0x%08x",
			name, event, d1, d2, d3);
	}
	else {
		// Continuation log message (only prints data payloads)
		printf("0x%08x 0x%08x 0x%08x", d1, d2, d3);
	}
}

/**
* @brief Enum for indices of the TMC_PRINT_TABLE.
*/
typedef enum {
	TMC_EVENT_WAIT = 0,
	TMC_EVENT_GOTO_INIT,
	TMC_EVENT_BOTH_INIT
} TmcEventId;

/**
* @brief Data table for TMC event names.
*/
const char *TMC_PRINT_TABLE[] = {
	"WAIT",
	"GOTO INIT",
	"BOTH INIT"
};

const unsigned int num_tmc_print_table =
sizeof(TMC_PRINT_TABLE) / sizeof(TMC_PRINT_TABLE[0]);

/**
* @brief Enum for indices of the TIMETICK_PRINT_TABLE.
*/
typedef enum {
	TIMETICK_EVENT_START = 0,
	TIMETICK_EVENT_GOTO_WAIT,
	TIMETICK_EVENT_GOTO_INIT
} TimetickEventId;

/**
* @brief Data table for TIMETICK event names.
*/
const char *TIMETICK_PRINT_TABLE[] = {
	"START",
	"GOTO WAIT",
	"GOTO INIT"
};

const unsigned int num_timetick_print_table =
sizeof(TIMETICK_PRINT_TABLE) / sizeof(TIMETICK_PRINT_TABLE[0]);


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
	bool ticks_flag)
{
	// The Perl logic checks if $$rec[0] (id) is not zero.
	if (rec->id != 0) {
		// if( $relative_time eq TRUE ) {
		if (*relative_time_ptr == TRUE) {
			// $base_time = $$rec[1];
			*base_time_ptr = rec->timestamp;

			// $relative_time = FALSE;
			*relative_time_ptr = FALSE;
		}

		// if( ($$rec[0] & $CONTINUE_MASK) eq 0 ) {
		// Check if this is the start of a new log entry (not a continuation event)
		if ((rec->id & CONTINUE_MASK) == 0) {
			// print_line_header( $$rec[0] & 0xC0000000, # modem/apps/q6 flag
			//                    $$rec[1] - $base_time, # time
			//                    $ticks );              # seconds/ticks flag

			print_line_header(
				rec->id & PROC_MASK,          // Processor flag (MODM/APPS/Q6)
				rec->timestamp - *base_time_ptr,   // Relative time
				ticks_flag                    // Ticks or Seconds flag
			);
		}

		// print_record( $rec );
		print_record(rec);
	}
}