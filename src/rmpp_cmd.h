// --------------------------------------------------------
// Copyright (c) 2025 rapid4mifu 
//
// These codes are licensed under MIT license.
// https://opensource.org/licenses/mit-license.php
// --------------------------------------------------------

// This header file is related to communication commands
//  for the Railway Model Power Pack (RMPP)

#ifndef __RMPP_CMD_H
#define __RMPP_CMD_H

// number of command id bytes
#define RMPP_BYTES_CMDID		(1) 
// number of data bytes
#define RMPP_BYTES_DAT_MAX		(15) 
// number of checksum bytes
#define RMPP_BYTES_CHECKSUM		(1) 
// number of cobs overhead bytes
#define RMPP_BYTES_OVERHEAD		(1)
// number of cobs delimiter bytes
#define RMPP_BYTES_DELIMITER	(1)
// number of cobs(Consistent Overhead Byte Stuffing) bytes
#define RMPP_BYTES_COBS			(RMPP_BYTES_OVERHEAD + RMPP_BYTES_DELIMITER) 
// number of checksum and cobs last code bytes
#define RMPP_BYTES_SUM_DELIM	(RMPP_BYTES_CHECKSUM + RMPP_BYTES_DELIMITER)

// minimum number of bytes per command
#define RMPP_CMD_LEN_MIN		(RMPP_BYTES_CMDID + RMPP_BYTES_CHECKSUM)
// maximum number of bytes per command
#define RMPP_CMD_LEN_MAX		(RMPP_CMD_LEN_MIN + RMPP_BYTES_DAT_MAX)
// maximum number of bytes per encoded packet
#define RMPP_PACKET_LEN_MAX		(RMPP_CMD_LEN_MAX + RMPP_BYTES_COBS)

// number of data bytes for RD_STATUS command
#define RMPP_BYTES_DAT_RD_STATUS	(4)
// number of data bytes for WR_OUTPUT command
#define RMPP_BYTES_DAT_WR_OUTPUT	(2)

// number of commad length for RD_STATUS command
#define RMPP_CMD_LEN_RD_STATUS		(RMPP_CMD_LEN_MIN + RMPP_BYTES_DAT_RD_STATUS)
// number of commad length for WR_OUTPUT command
#define RMPP_CMD_LEN_WR_OUTPUT		(RMPP_CMD_LEN_MIN + RMPP_BYTES_DAT_WR_OUTPUT)

// command id for RD_STATUS command
#define RMPP_CMDID_RD_STATUS		(0x00 | RMPP_BYTES_DAT_RD_STATUS)
// command id for WR_OUTPUT command
#define RMPP_CMDID_WR_OUTPUT		(0x10 | RMPP_BYTES_DAT_WR_OUTPUT)

// get number of data bytes from command id
#define RMPP_GET_BYTES_DAT(byte)	((byte) & 0x0F)
// get number of command length from command id
#define RMPP_GET_CMD_LEN(byte)		(RMPP_GET_BYTES_DAT(byte) + RMPP_CMD_LEN_MIN)

#endif /* __RMPP_CMD_H */
