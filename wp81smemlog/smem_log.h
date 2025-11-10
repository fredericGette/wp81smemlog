#pragma once

/**
* @brief Assumed data structure for a single log entry.
* Based on the Perl script's use of $rec->[0] through $rec->[4].
*/
typedef struct {
	uint32_t id;         // $$rec[0]
	uint32_t timestamp;  // $$rec[1] 
	uint32_t d1;         // $$rec[2]
	uint32_t d2;         // $$rec[3]
	uint32_t d3;         // $$rec[4]
} SmemLogRecord;

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
	bool ticks_flag);