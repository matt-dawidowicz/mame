// license:BSD-3-Clause
// copyright-holders:Ryan Holtz, Vincent Halver
/******************************************************************************


    CD-i Mono-I CDIC MCU simulation
    -------------------

*******************************************************************************

STATUS:

- Just enough for the Mono-I CD-i board to work somewhat properly.

TODO:

- Work out more low-level functionality.

*******************************************************************************/

#include "emu.h"
#include "cdicdic.h"
#include "cdiaudio.h"

#include "cdrom.h"
#include "sound/cdda.h"

#include <algorithm>

#define LOG_DECODES     (1U << 1)
#define LOG_SAMPLES     (1U << 2)
#define LOG_COMMANDS    (1U << 3)
#define LOG_SECTORS     (1U << 4)
#define LOG_IRQS        (1U << 5)
#define LOG_READS       (1U << 6)
#define LOG_WRITES      (1U << 7)
#define LOG_UNKNOWNS    (1U << 8)
#define LOG_RAM         (1U << 9)
#define LOG_ALL         (LOG_DECODES | LOG_SAMPLES | LOG_COMMANDS | LOG_SECTORS | LOG_IRQS | LOG_READS | LOG_WRITES | LOG_UNKNOWNS | LOG_RAM)

#define VERBOSE         (0)
#include "logmacro.h"


// device type definition
DEFINE_DEVICE_TYPE(CDI_CDIC, cdicdic_device, "cdicdic", "CD-i CDIC")

//**************************************************************************
//  STATIC MEMBERS
//**************************************************************************

const int32_t cdicdic_device::s_samples_per_sector = 18 * 28 * 2;

const uint16_t cdicdic_device::s_crc_ccitt_table[256] =
{
	0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
	0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
	0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
	0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
	0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
	0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
	0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
	0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
	0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
	0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
	0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
	0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
	0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
	0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
	0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
	0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
	0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
	0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
	0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
	0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
	0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
	0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
	0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
	0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
	0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
	0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
	0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
	0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
	0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
	0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
	0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
	0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

#define CRC_CCITT_ROUND(accum, data) (((accum << 8) | data) ^ s_crc_ccitt_table[accum >> 8])

const uint8_t cdicdic_device::s_sector_scramble[2448] =
{
	// Sector sync area is not scrambled
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	// Remaining data is scrambled
	0x01, 0x80, 0x00, 0x60, 0x00, 0x28, 0x00, 0x1e, 0x80, 0x08, 0x60, 0x06, 0xa8, 0x02, 0xfe, 0x81,
	0x80, 0x60, 0x60, 0x28, 0x28, 0x1e, 0x9e, 0x88, 0x68, 0x66, 0xae, 0xaa, 0xfc, 0x7f, 0x01, 0xe0,
	0x00, 0x48, 0x00, 0x36, 0x80, 0x16, 0xe0, 0x0e, 0xc8, 0x04, 0x56, 0x83, 0x7e, 0xe1, 0xe0, 0x48,
	0x48, 0x36, 0xb6, 0x96, 0xf6, 0xee, 0xc6, 0xcc, 0x52, 0xd5, 0xfd, 0x9f, 0x01, 0xa8, 0x00, 0x7e,
	0x80, 0x20, 0x60, 0x18, 0x28, 0x0a, 0x9e, 0x87, 0x28, 0x62, 0x9e, 0xa9, 0xa8, 0x7e, 0xfe, 0xa0,
	0x40, 0x78, 0x30, 0x22, 0x94, 0x19, 0xaf, 0x4a, 0xfc, 0x37, 0x01, 0xd6, 0x80, 0x5e, 0xe0, 0x38,
	0x48, 0x12, 0xb6, 0x8d, 0xb6, 0xe5, 0xb6, 0xcb, 0x36, 0xd7, 0x56, 0xde, 0xbe, 0xd8, 0x70, 0x5a,
	0xa4, 0x3b, 0x3b, 0x53, 0x53, 0x7d, 0xfd, 0xe1, 0x81, 0x88, 0x60, 0x66, 0xa8, 0x2a, 0xfe, 0x9f,
	0x00, 0x68, 0x00, 0x2e, 0x80, 0x1c, 0x60, 0x09, 0xe8, 0x06, 0xce, 0x82, 0xd4, 0x61, 0x9f, 0x68,
	0x68, 0x2e, 0xae, 0x9c, 0x7c, 0x69, 0xe1, 0xee, 0xc8, 0x4c, 0x56, 0xb5, 0xfe, 0xf7, 0x00, 0x46,
	0x80, 0x32, 0xe0, 0x15, 0x88, 0x0f, 0x26, 0x84, 0x1a, 0xe3, 0x4b, 0x09, 0xf7, 0x46, 0xc6, 0xb2,
	0xd2, 0xf5, 0x9d, 0x87, 0x29, 0xa2, 0x9e, 0xf9, 0xa8, 0x42, 0xfe, 0xb1, 0x80, 0x74, 0x60, 0x27,
	0x68, 0x1a, 0xae, 0x8b, 0x3c, 0x67, 0x51, 0xea, 0xbc, 0x4f, 0x31, 0xf4, 0x14, 0x47, 0x4f, 0x72,
	0xb4, 0x25, 0xb7, 0x5b, 0x36, 0xbb, 0x56, 0xf3, 0x7e, 0xc5, 0xe0, 0x53, 0x08, 0x3d, 0xc6, 0x91,
	0x92, 0xec, 0x6d, 0x8d, 0xed, 0xa5, 0x8d, 0xbb, 0x25, 0xb3, 0x5b, 0x35, 0xfb, 0x57, 0x03, 0x7e,
	0x81, 0xe0, 0x60, 0x48, 0x28, 0x36, 0x9e, 0x96, 0xe8, 0x6e, 0xce, 0xac, 0x54, 0x7d, 0xff, 0x61,
	0x80, 0x28, 0x60, 0x1e, 0xa8, 0x08, 0x7e, 0x86, 0xa0, 0x62, 0xf8, 0x29, 0x82, 0x9e, 0xe1, 0xa8,
	0x48, 0x7e, 0xb6, 0xa0, 0x76, 0xf8, 0x26, 0xc2, 0x9a, 0xd1, 0xab, 0x1c, 0x7f, 0x49, 0xe0, 0x36,
	0xc8, 0x16, 0xd6, 0x8e, 0xde, 0xe4, 0x58, 0x4b, 0x7a, 0xb7, 0x63, 0x36, 0xa9, 0xd6, 0xfe, 0xde,
	0xc0, 0x58, 0x50, 0x3a, 0xbc, 0x13, 0x31, 0xcd, 0xd4, 0x55, 0x9f, 0x7f, 0x28, 0x20, 0x1e, 0x98,
	0x08, 0x6a, 0x86, 0xaf, 0x22, 0xfc, 0x19, 0x81, 0xca, 0xe0, 0x57, 0x08, 0x3e, 0x86, 0x90, 0x62,
	0xec, 0x29, 0x8d, 0xde, 0xe5, 0x98, 0x4b, 0x2a, 0xb7, 0x5f, 0x36, 0xb8, 0x16, 0xf2, 0x8e, 0xc5,
	0xa4, 0x53, 0x3b, 0x7d, 0xd3, 0x61, 0x9d, 0xe8, 0x69, 0x8e, 0xae, 0xe4, 0x7c, 0x4b, 0x61, 0xf7,
	0x68, 0x46, 0xae, 0xb2, 0xfc, 0x75, 0x81, 0xe7, 0x20, 0x4a, 0x98, 0x37, 0x2a, 0x96, 0x9f, 0x2e,
	0xe8, 0x1c, 0x4e, 0x89, 0xf4, 0x66, 0xc7, 0x6a, 0xd2, 0xaf, 0x1d, 0xbc, 0x09, 0xb1, 0xc6, 0xf4,
	0x52, 0xc7, 0x7d, 0x92, 0xa1, 0xad, 0xb8, 0x7d, 0xb2, 0xa1, 0xb5, 0xb8, 0x77, 0x32, 0xa6, 0x95,
	0xba, 0xef, 0x33, 0x0c, 0x15, 0xc5, 0xcf, 0x13, 0x14, 0x0d, 0xcf, 0x45, 0x94, 0x33, 0x2f, 0x55,
	0xdc, 0x3f, 0x19, 0xd0, 0x0a, 0xdc, 0x07, 0x19, 0xc2, 0x8a, 0xd1, 0xa7, 0x1c, 0x7a, 0x89, 0xe3,
	0x26, 0xc9, 0xda, 0xd6, 0xdb, 0x1e, 0xdb, 0x48, 0x5b, 0x76, 0xbb, 0x66, 0xf3, 0x6a, 0xc5, 0xef,
	0x13, 0x0c, 0x0d, 0xc5, 0xc5, 0x93, 0x13, 0x2d, 0xcd, 0xdd, 0x95, 0x99, 0xaf, 0x2a, 0xfc, 0x1f,
	0x01, 0xc8, 0x00, 0x56, 0x80, 0x3e, 0xe0, 0x10, 0x48, 0x0c, 0x36, 0x85, 0xd6, 0xe3, 0x1e, 0xc9,
	0xc8, 0x56, 0xd6, 0xbe, 0xde, 0xf0, 0x58, 0x44, 0x3a, 0xb3, 0x53, 0x35, 0xfd, 0xd7, 0x01, 0x9e,
	0x80, 0x68, 0x60, 0x2e, 0xa8, 0x1c, 0x7e, 0x89, 0xe0, 0x66, 0xc8, 0x2a, 0xd6, 0x9f, 0x1e, 0xe8,
	0x08, 0x4e, 0x86, 0xb4, 0x62, 0xf7, 0x69, 0x86, 0xae, 0xe2, 0xfc, 0x49, 0x81, 0xf6, 0xe0, 0x46,
	0xc8, 0x32, 0xd6, 0x95, 0x9e, 0xef, 0x28, 0x4c, 0x1e, 0xb5, 0xc8, 0x77, 0x16, 0xa6, 0x8e, 0xfa,
	0xe4, 0x43, 0x0b, 0x71, 0xc7, 0x64, 0x52, 0xab, 0x7d, 0xbf, 0x61, 0xb0, 0x28, 0x74, 0x1e, 0xa7,
	0x48, 0x7a, 0xb6, 0xa3, 0x36, 0xf9, 0xd6, 0xc2, 0xde, 0xd1, 0x98, 0x5c, 0x6a, 0xb9, 0xef, 0x32,
	0xcc, 0x15, 0x95, 0xcf, 0x2f, 0x14, 0x1c, 0x0f, 0x49, 0xc4, 0x36, 0xd3, 0x56, 0xdd, 0xfe, 0xd9,
	0x80, 0x5a, 0xe0, 0x3b, 0x08, 0x13, 0x46, 0x8d, 0xf2, 0xe5, 0x85, 0x8b, 0x23, 0x27, 0x59, 0xda,
	0xba, 0xdb, 0x33, 0x1b, 0x55, 0xcb, 0x7f, 0x17, 0x60, 0x0e, 0xa8, 0x04, 0x7e, 0x83, 0x60, 0x61,
	0xe8, 0x28, 0x4e, 0x9e, 0xb4, 0x68, 0x77, 0x6e, 0xa6, 0xac, 0x7a, 0xfd, 0xe3, 0x01, 0x89, 0xc0,
	0x66, 0xd0, 0x2a, 0xdc, 0x1f, 0x19, 0xc8, 0x0a, 0xd6, 0x87, 0x1e, 0xe2, 0x88, 0x49, 0xa6, 0xb6,
	0xfa, 0xf6, 0xc3, 0x06, 0xd1, 0xc2, 0xdc, 0x51, 0x99, 0xfc, 0x6a, 0xc1, 0xef, 0x10, 0x4c, 0x0c,
	0x35, 0xc5, 0xd7, 0x13, 0x1e, 0x8d, 0xc8, 0x65, 0x96, 0xab, 0x2e, 0xff, 0x5c, 0x40, 0x39, 0xf0,
	0x12, 0xc4, 0x0d, 0x93, 0x45, 0xad, 0xf3, 0x3d, 0x85, 0xd1, 0xa3, 0x1c, 0x79, 0xc9, 0xe2, 0xd6,
	0xc9, 0x9e, 0xd6, 0xe8, 0x5e, 0xce, 0xb8, 0x54, 0x72, 0xbf, 0x65, 0xb0, 0x2b, 0x34, 0x1f, 0x57,
	0x48, 0x3e, 0xb6, 0x90, 0x76, 0xec, 0x26, 0xcd, 0xda, 0xd5, 0x9b, 0x1f, 0x2b, 0x48, 0x1f, 0x76,
	0x88, 0x26, 0xe6, 0x9a, 0xca, 0xeb, 0x17, 0x0f, 0x4e, 0x84, 0x34, 0x63, 0x57, 0x69, 0xfe, 0xae,
	0xc0, 0x7c, 0x50, 0x21, 0xfc, 0x18, 0x41, 0xca, 0xb0, 0x57, 0x34, 0x3e, 0x97, 0x50, 0x6e, 0xbc,
	0x2c, 0x71, 0xdd, 0xe4, 0x59, 0x8b, 0x7a, 0xe7, 0x63, 0x0a, 0xa9, 0xc7, 0x3e, 0xd2, 0x90, 0x5d,
	0xac, 0x39, 0xbd, 0xd2, 0xf1, 0x9d, 0x84, 0x69, 0xa3, 0x6e, 0xf9, 0xec, 0x42, 0xcd, 0xf1, 0x95,
	0x84, 0x6f, 0x23, 0x6c, 0x19, 0xed, 0xca, 0xcd, 0x97, 0x15, 0xae, 0x8f, 0x3c, 0x64, 0x11, 0xeb,
	0x4c, 0x4f, 0x75, 0xf4, 0x27, 0x07, 0x5a, 0x82, 0xbb, 0x21, 0xb3, 0x58, 0x75, 0xfa, 0xa7, 0x03,
	0x3a, 0x81, 0xd3, 0x20, 0x5d, 0xd8, 0x39, 0x9a, 0x92, 0xeb, 0x2d, 0x8f, 0x5d, 0xa4, 0x39, 0xbb,
	0x52, 0xf3, 0x7d, 0x85, 0xe1, 0xa3, 0x08, 0x79, 0xc6, 0xa2, 0xd2, 0xf9, 0x9d, 0x82, 0xe9, 0xa1,
	0x8e, 0xf8, 0x64, 0x42, 0xab, 0x71, 0xbf, 0x64, 0x70, 0x2b, 0x64, 0x1f, 0x6b, 0x48, 0x2f, 0x76,
	0x9c, 0x26, 0xe9, 0xda, 0xce, 0xdb, 0x14, 0x5b, 0x4f, 0x7b, 0x74, 0x23, 0x67, 0x59, 0xea, 0xba,
	0xcf, 0x33, 0x14, 0x15, 0xcf, 0x4f, 0x14, 0x34, 0x0f, 0x57, 0x44, 0x3e, 0xb3, 0x50, 0x75, 0xfc,
	0x27, 0x01, 0xda, 0x80, 0x5b, 0x20, 0x3b, 0x58, 0x13, 0x7a, 0x8d, 0xe3, 0x25, 0x89, 0xdb, 0x26,
	0xdb, 0x5a, 0xdb, 0x7b, 0x1b, 0x63, 0x4b, 0x69, 0xf7, 0x6e, 0xc6, 0xac, 0x52, 0xfd, 0xfd, 0x81,
	0x81, 0xa0, 0x60, 0x78, 0x28, 0x22, 0x9e, 0x99, 0xa8, 0x6a, 0xfe, 0xaf, 0x00, 0x7c, 0x00, 0x21,
	0xc0, 0x18, 0x50, 0x0a, 0xbc, 0x07, 0x31, 0xc2, 0x94, 0x51, 0xaf, 0x7c, 0x7c, 0x21, 0xe1, 0xd8,
	0x48, 0x5a, 0xb6, 0xbb, 0x36, 0xf3, 0x56, 0xc5, 0xfe, 0xd3, 0x00, 0x5d, 0xc0, 0x39, 0x90, 0x12,
	0xec, 0x0d, 0x8d, 0xc5, 0xa5, 0x93, 0x3b, 0x2d, 0xd3, 0x5d, 0x9d, 0xf9, 0xa9, 0x82, 0xfe, 0xe1,
	0x80, 0x48, 0x60, 0x36, 0xa8, 0x16, 0xfe, 0x8e, 0xc0, 0x64, 0x50, 0x2b, 0x7c, 0x1f, 0x61, 0xc8,
	0x28, 0x56, 0x9e, 0xbe, 0xe8, 0x70, 0x4e, 0xa4, 0x34, 0x7b, 0x57, 0x63, 0x7e, 0xa9, 0xe0, 0x7e,
	0xc8, 0x20, 0x56, 0x98, 0x3e, 0xea, 0x90, 0x4f, 0x2c, 0x34, 0x1d, 0xd7, 0x49, 0x9e, 0xb6, 0xe8,
	0x76, 0xce, 0xa6, 0xd4, 0x7a, 0xdf, 0x63, 0x18, 0x29, 0xca, 0x9e, 0xd7, 0x28, 0x5e, 0x9e, 0xb8,
	0x68, 0x72, 0xae, 0xa5, 0xbc, 0x7b, 0x31, 0xe3, 0x54, 0x49, 0xff, 0x76, 0xc0, 0x26, 0xd0, 0x1a,
	0xdc, 0x0b, 0x19, 0xc7, 0x4a, 0xd2, 0xb7, 0x1d, 0xb6, 0x89, 0xb6, 0xe6, 0xf6, 0xca, 0xc6, 0xd7,
	0x12, 0xde, 0x8d, 0x98, 0x65, 0xaa, 0xab, 0x3f, 0x3f, 0x50, 0x10, 0x3c, 0x0c, 0x11, 0xc5, 0xcc,
	0x53, 0x15, 0xfd, 0xcf, 0x01, 0x94, 0x00, 0x6f, 0x40, 0x2c, 0x30, 0x1d, 0xd4, 0x09, 0x9f, 0x46,
	0xe8, 0x32, 0xce, 0x95, 0x94, 0x6f, 0x2f, 0x6c, 0x1c, 0x2d, 0xc9, 0xdd, 0x96, 0xd9, 0xae, 0xda,
	0xfc, 0x5b, 0x01, 0xfb, 0x40, 0x43, 0x70, 0x31, 0xe4, 0x14, 0x4b, 0x4f, 0x77, 0x74, 0x26, 0xa7,
	0x5a, 0xfa, 0xbb, 0x03, 0x33, 0x41, 0xd5, 0xf0, 0x5f, 0x04, 0x38, 0x03, 0x52, 0x81, 0xfd, 0xa0,
	0x41, 0xb8, 0x30, 0x72, 0x94, 0x25, 0xaf, 0x5b, 0x3c, 0x3b, 0x51, 0xd3, 0x7c, 0x5d, 0xe1, 0xf9,
	0x88, 0x42, 0xe6, 0xb1, 0x8a, 0xf4, 0x67, 0x07, 0x6a, 0x82, 0xaf, 0x21, 0xbc, 0x18, 0x71, 0xca,
	0xa4, 0x57, 0x3b, 0x7e, 0x93, 0x60, 0x6d, 0xe8, 0x2d, 0x8e, 0x9d, 0xa4, 0x69, 0xbb, 0x6e, 0xf3,
	0x6c, 0x45, 0xed, 0xf3, 0x0d, 0x85, 0xc5, 0xa3, 0x13, 0x39, 0xcd, 0xd2, 0xd5, 0x9d, 0x9f, 0x29,
	0xa8, 0x1e, 0xfe, 0x88, 0x40, 0x66, 0xb0, 0x2a, 0xf4, 0x1f, 0x07, 0x48, 0x02, 0xb6, 0x81, 0xb6,
	0xe0, 0x76, 0xc8, 0x26, 0xd6, 0x9a, 0xde, 0xeb, 0x18, 0x4f, 0x4a, 0xb4, 0x37, 0x37, 0x56, 0x96,
	0xbe, 0xee, 0xf0, 0x4c, 0x44, 0x35, 0xf3, 0x57, 0x05, 0xfe, 0x83, 0x00, 0x61, 0xc0, 0x28, 0x50,
	0x1e, 0xbc, 0x08, 0x71, 0xc6, 0xa4, 0x52, 0xfb, 0x7d, 0x83, 0x61, 0xa1, 0xe8, 0x78, 0x4e, 0xa2,
	0xb4, 0x79, 0xb7, 0x62, 0xf6, 0xa9, 0x86, 0xfe, 0xe2, 0xc0, 0x49, 0x90, 0x36, 0xec, 0x16, 0xcd,
	0xce, 0xd5, 0x94, 0x5f, 0x2f, 0x78, 0x1c, 0x22, 0x89, 0xd9, 0xa6, 0xda, 0xfa, 0xdb, 0x03, 0x1b,
	0x41, 0xcb, 0x70, 0x57, 0x64, 0x3e, 0xab, 0x50, 0x7f, 0x7c, 0x20, 0x21, 0xd8, 0x18, 0x5a, 0x8a,
	0xbb, 0x27, 0x33, 0x5a, 0x95, 0xfb, 0x2f, 0x03, 0x5c, 0x01, 0xf9, 0xc0, 0x42, 0xd0, 0x31, 0x9c,
	0x14, 0x69, 0xcf, 0x6e, 0xd4, 0x2c, 0x5f, 0x5d, 0xf8, 0x39, 0x82, 0x92, 0xe1, 0xad, 0x88, 0x7d,
	0xa6, 0xa1, 0xba, 0xf8, 0x73, 0x02, 0xa5, 0xc1, 0xbb, 0x10, 0x73, 0x4c, 0x25, 0xf5, 0xdb, 0x07,
	0x1b, 0x42, 0x8b, 0x71, 0xa7, 0x64, 0x7a, 0xab, 0x63, 0x3f, 0x69, 0xd0, 0x2e, 0xdc, 0x1c, 0x59,
	0xc9, 0xfa, 0xd6, 0xc3, 0x1e, 0xd1, 0xc8, 0x5c, 0x56, 0xb9, 0xfe, 0xf2, 0xc0, 0x45, 0x90, 0x33,
	0x2c, 0x15, 0xdd, 0xcf, 0x19, 0x94, 0x0a, 0xef, 0x47, 0x0c, 0x32, 0x85, 0xd5, 0xa3, 0x1f, 0x39,
	0xc8, 0x12, 0xd6, 0x8d, 0x9e, 0xe5, 0xa8, 0x4b, 0x3e, 0xb7, 0x50, 0x76, 0xbc, 0x26, 0xf1, 0xda,
	0xc4, 0x5b, 0x13, 0x7b, 0x4d, 0xe3, 0x75, 0x89, 0xe7, 0x26, 0xca, 0x9a, 0xd7, 0x2b, 0x1e, 0x9f,
	0x48, 0x68, 0x36, 0xae, 0x96, 0xfc, 0x6e, 0xc1, 0xec, 0x50, 0x4d, 0xfc, 0x35, 0x81, 0xd7, 0x20,
	0x5e, 0x98, 0x38, 0x6a, 0x92, 0xaf, 0x2d, 0xbc, 0x1d, 0xb1, 0xc9, 0xb4, 0x56, 0xf7, 0x7e, 0xc6,
	0xa0, 0x52, 0xf8, 0x3d, 0x82, 0x91, 0xa1, 0xac, 0x78, 0x7d, 0xe2, 0xa1, 0x89, 0xb8, 0x66, 0xf2,
	0xaa, 0xc5, 0xbf, 0x13, 0x30, 0x0d, 0xd4, 0x05, 0x9f, 0x43, 0x28, 0x31, 0xde, 0x94, 0x58, 0x6f,
	0x7a, 0xac, 0x23, 0x3d, 0xd9, 0xd1, 0x9a, 0xdc, 0x6b, 0x19, 0xef, 0x4a, 0xcc, 0x37, 0x15, 0xd6,
	0x8f, 0x1e, 0xe4, 0x08, 0x4b, 0x46, 0xb7, 0x72, 0xf6, 0xa5, 0x86, 0xfb, 0x22, 0xc3, 0x59, 0x91,
	0xfa, 0xec, 0x43, 0x0d, 0xf1, 0xc5, 0x84, 0x53, 0x23, 0x7d, 0xd9, 0xe1, 0x9a, 0xc8, 0x6b, 0x16,
	0xaf, 0x4e, 0xfc, 0x34, 0x41, 0xd7, 0x70, 0x5e, 0xa4, 0x38, 0x7b, 0x52, 0xa3, 0x7d, 0xb9, 0xe1,
	0xb2, 0xc8, 0x75, 0x96, 0xa7, 0x2e, 0xfa, 0x9c, 0x43, 0x29, 0xf1, 0xde, 0xc4, 0x58, 0x53, 0x7a,
	0xbd, 0xe3, 0x31, 0x89, 0xd4, 0x66, 0xdf, 0x6a, 0xd8, 0x2f, 0x1a, 0x9c, 0x0b, 0x29, 0xc7, 0x5e,
	0xd2, 0xb8, 0x5d, 0xb2, 0xb9, 0xb5, 0xb2, 0xf7, 0x35, 0x86, 0x97, 0x22, 0xee, 0x99, 0x8c, 0x6a,
	0xe5, 0xef, 0x0b, 0x0c, 0x07, 0x45, 0xc2, 0xb3, 0x11, 0xb5, 0xcc, 0x77, 0x15, 0xe6, 0x8f, 0x0a,
	0xe4, 0x07, 0x0b, 0x42, 0x87, 0x71, 0xa2, 0xa4, 0x79, 0xbb, 0x62, 0xf3, 0x69, 0x85, 0xee, 0xe3,
	0x0c, 0x49, 0xc5, 0xf6, 0xd3, 0x06, 0xdd, 0xc2, 0xd9, 0x91, 0x9a, 0xec, 0x6b, 0x0d, 0xef, 0x45,
	0x8c, 0x33, 0x25, 0xd5, 0xdb, 0x1f, 0x1b, 0x48, 0x0b, 0x76, 0x87, 0x66, 0xe2, 0xaa, 0xc9, 0xbf,
	0x16, 0xf0, 0x0e, 0xc4, 0x04, 0x53, 0x43, 0x7d, 0xf1, 0xe1, 0x84, 0x48, 0x63, 0x76, 0xa9, 0xe6,
	0xfe, 0xca, 0xc0, 0x57, 0x10, 0x3e, 0x8c, 0x10, 0x65, 0xcc, 0x2b, 0x15, 0xdf, 0x4f, 0x18, 0x34,
	0x0a, 0x97, 0x47, 0x2e, 0xb2, 0x9c, 0x75, 0xa9, 0xe7, 0x3e, 0xca, 0x90, 0x57, 0x2c, 0x3e, 0x9d,
	0xd0, 0x69, 0x9c, 0x2e, 0xe9, 0xdc, 0x4e, 0xd9, 0xf4, 0x5a, 0xc7, 0x7b, 0x12, 0xa3, 0x4d, 0xb9,
	0xf5, 0xb2, 0xc7, 0x35, 0x92, 0x97, 0x2d, 0xae, 0x9d, 0xbc, 0x69, 0xb1, 0xee, 0xf4, 0x4c, 0x47,
	0x75, 0xf2, 0xa7, 0x05, 0xba, 0x83, 0x33, 0x21, 0xd5, 0xd8, 0x5f, 0x1a, 0xb8, 0x0b, 0x32, 0x87,
	0x55, 0xa2, 0xbf, 0x39, 0xb0, 0x12, 0xf4, 0x0d, 0x87, 0x45, 0xa2, 0xb3, 0x39, 0xb5, 0xd2, 0xf7,
	0x1d, 0x86, 0x89, 0xa2, 0xe6, 0xf9, 0x8a, 0xc2, 0xe7, 0x11, 0x8a, 0x8c, 0x67, 0x25, 0xea, 0x9b,
	0x0f, 0x2b, 0x44, 0x1f, 0x73, 0x48, 0x25, 0xf6, 0x9b, 0x06, 0xeb, 0x42, 0xcf, 0x71, 0x94, 0x24,
	0x6f, 0x5b, 0x6c, 0x3b, 0x6d, 0xd3, 0x6d, 0x9d, 0xed, 0xa9, 0x8d, 0xbe, 0xe5, 0xb0, 0x4b, 0x34,
	0x37, 0x57, 0x56, 0xbe, 0xbe, 0xf0, 0x70, 0x44, 0x24, 0x33, 0x5b, 0x55, 0xfb, 0x7f, 0x03, 0x60,
	0x01, 0xe8, 0x00, 0x4e, 0x80, 0x34, 0x60, 0x17, 0x68, 0x0e, 0xae, 0x84, 0x7c, 0x63, 0x61, 0xe9,
	0xe8, 0x4e, 0xce, 0xb4, 0x54, 0x77, 0x7f, 0x66, 0xa0, 0x2a, 0xf8, 0x1f, 0x02, 0x88, 0x01, 0xa6,
	0x80, 0x7a, 0xe0, 0x23, 0x08, 0x19, 0xc6, 0x8a, 0xd2, 0xe7, 0x1d, 0x8a, 0x89, 0xa7, 0x26, 0xfa,
	0x9a, 0xc3, 0x2b, 0x11, 0xdf, 0x4c, 0x58, 0x35, 0xfa, 0x97, 0x03, 0x2e, 0x81, 0xdc, 0x60, 0x59,
	0xe8, 0x3a, 0xce, 0x93, 0x14, 0x6d, 0xcf, 0x6d, 0x94, 0x2d, 0xaf, 0x5d, 0xbc, 0x39, 0xb1, 0xd2,
	0xf4, 0x5d, 0x87, 0x79, 0xa2, 0xa2, 0xf9, 0xb9, 0x82, 0xf2, 0xe1, 0x85, 0x88, 0x63, 0x26, 0xa9,
	0xda, 0xfe, 0xdb, 0x00, 0x5b, 0x40, 0x3b, 0x70, 0x13, 0x64, 0x0d, 0xeb, 0x45, 0x8f, 0x73, 0x24,
	0x25, 0xdb, 0x5b, 0x1b, 0x7b, 0x4b, 0x63, 0x77, 0x69, 0xe6, 0xae, 0xca, 0xfc, 0x57, 0x01, 0xfe,
	0x80, 0x40, 0x60, 0x30, 0x28, 0x14, 0x1e, 0x8f, 0x48, 0x64, 0x36, 0xab, 0x56, 0xff, 0x7e, 0xc0,
	0x20, 0x50, 0x18, 0x3c, 0x0a, 0x91, 0xc7, 0x2c, 0x52, 0x9d, 0xfd, 0xa9, 0x81, 0xbe, 0xe0, 0x70,
	0x48, 0x24, 0x36, 0x9b, 0x56, 0xeb, 0x7e, 0xcf, 0x60, 0x54, 0x28, 0x3f, 0x5e, 0x90, 0x38, 0x6c,
	0x12, 0xad, 0xcd, 0xbd, 0x95, 0xb1, 0xaf, 0x34, 0x7c, 0x17, 0x61, 0xce, 0xa8, 0x54, 0x7e, 0xbf,
	0x60, 0x70, 0x28, 0x24, 0x1e, 0x9b, 0x48, 0x6b, 0x76, 0xaf, 0x66, 0xfc, 0x2a, 0xc1, 0xdf, 0x10,
	0x58, 0x0c, 0x3a, 0x85, 0xd3, 0x23, 0x1d, 0xd9, 0xc9, 0x9a, 0xd6, 0xeb, 0x1e, 0xcf, 0x48, 0x54,
	0x36, 0xbf, 0x56, 0xf0, 0x3e, 0xc4, 0x10, 0x53, 0x4c, 0x3d, 0xf5, 0xd1, 0x87, 0x1c, 0x62, 0x89,
	0xe9, 0xa6, 0xce, 0xfa, 0xd4, 0x43, 0x1f, 0x71, 0xc8, 0x24, 0x56, 0x9b, 0x7e, 0xeb, 0x60, 0x4f,
	0x68, 0x34, 0x2e, 0x97, 0x5c, 0x6e, 0xb9, 0xec, 0x72, 0xcd, 0xe5, 0x95, 0x8b, 0x2f, 0x27, 0x5c,
	0x1a, 0xb9, 0xcb, 0x32, 0xd7, 0x55, 0x9e, 0xbf, 0x28, 0x70, 0x1e, 0xa4, 0x08, 0x7b, 0x46, 0xa3,
	0x72, 0xf9, 0xe5, 0x82, 0xcb, 0x21, 0x97, 0x58, 0x6e, 0xba, 0xac, 0x73, 0x3d, 0xe5, 0xd1, 0x8b,
	0x1c, 0x67, 0x49, 0xea, 0xb6, 0xcf, 0x36, 0xd4, 0x16, 0xdf, 0x4e, 0xd8, 0x34, 0x5a, 0x97, 0x7b,
	0x2e, 0xa3, 0x5c, 0x79, 0xf9, 0xe2, 0xc2, 0xc9, 0x91, 0x96, 0xec, 0x6e, 0xcd, 0xec, 0x55, 0x8d,
	0xff, 0x25, 0x80, 0x1b, 0x20, 0x0b, 0x58, 0x07, 0x7a, 0x82, 0xa3, 0x21, 0xb9, 0xd8, 0x72, 0xda,
	0xa5, 0x9b, 0x3b, 0x2b, 0x53, 0x5f, 0x7d, 0xf8, 0x21, 0x82, 0x98, 0x61, 0xaa, 0xa8, 0x7f, 0x3e,
	0xa0, 0x10, 0x78, 0x0c, 0x22, 0x85, 0xd9, 0xa3, 0x1a, 0xf9, 0xcb, 0x02, 0xd7, 0x41, 0x9e, 0xb0,
	0x68, 0x74, 0x2e, 0xa7, 0x5c, 0x7a, 0xb9, 0xe3, 0x32, 0xc9, 0xd5, 0x96, 0xdf, 0x2e, 0xd8, 0x1c,
	0x5a, 0x89, 0xfb, 0x26, 0xc3, 0x5a, 0xd1, 0xfb, 0x1c, 0x43, 0x49, 0xf1, 0xf6, 0xc4, 0x46, 0xd3,
	0x72, 0xdd, 0xe5, 0x99
};


//**************************************************************************
//  MEMBER FUNCTIONS
//**************************************************************************

void cdicdic_device::play_xa_group(const cdic_hle::xa_coding &coding, const uint8_t *data, const uint16_t idx)
{
	const cdic_hle::xa_group_decode_result result = cdic_hle::decode_xa_group(
		coding.bits_per_sample,
		coding.channels,
		data,
		m_xa_last,
		&m_samples[0][idx],
		&m_samples[1][idx]);
	if (result.parameters.copy_mismatch)
	{
		LOGMASKED(LOG_SECTORS,
			"XA sound-parameter copies disagree at sample %u (width %u, units %02x); using Mono-I-selected copies\n",
			unsigned(idx), unsigned(coding.bits_per_sample), unsigned(result.parameters.copy_mismatch));
	}
	if (!result.valid())
	{
		LOGMASKED(LOG_SECTORS,
			"Invalid selected XA sound parameter at sample %u (width %u, filter %02x, range %02x), substituting silence\n",
			unsigned(idx), unsigned(coding.bits_per_sample),
			unsigned(result.parameters.reserved_filter),
			unsigned(result.parameters.reserved_range));
	}
}

void cdicdic_device::play_cdda_sector(const uint8_t *data)
{
	m_dmadac[0]->set_frequency(44100);
	m_dmadac[1]->set_frequency(44100);
	m_dmadac[0]->set_volume(0x100);
	m_dmadac[1]->set_volume(0x100);

	const uint16_t NUM_SAMPLES = SECTOR_SIZE / 4;
	for (uint16_t i = 0; i < NUM_SAMPLES; i++)
	{
		m_samples[0][i] = int16_t((data[(i * 4) + 1] << 8) | data[(i * 4) + 0]);
		m_samples[1][i] = int16_t((data[(i * 4) + 3] << 8) | data[(i * 4) + 2]);
	}

	m_dmadac[0]->transfer(0, 1, 1, NUM_SAMPLES, &m_samples[0][0]);
	m_dmadac[1]->transfer(0, 1, 1, NUM_SAMPLES, &m_samples[1][0]);
}

void cdicdic_device::play_audio_sector(const uint8_t coding, const uint8_t *data)
{
	const cdic_hle::xa_coding coding_info = cdic_hle::decode_xa_coding(coding);
	if (!coding_info.valid())
	{
		LOGMASKED(LOG_SECTORS, "Invalid/reserved coding (%02x, status %u), ignoring\n",
			unsigned(coding), unsigned(coding_info.status));
		return;
	}

	if (coding_info.emphasis)
	{
		LOGMASKED(LOG_SECTORS, "Emphasis is not implemented (%02x), ignoring\n", unsigned(coding));
		// TODO: Emphasis is commonly used. Do not throw a fatal error.
	}

	const int32_t sample_frequency = cdic_hle::xa_sample_rate(coding_info, clock2());

	LOGMASKED(LOG_SECTORS, "Coding %02x, %u channels, %u bits, %08x frequency\n",
		unsigned(coding), unsigned(coding_info.channels), unsigned(coding_info.bits_per_sample), unsigned(sample_frequency));

	m_dmadac[0]->set_frequency(sample_frequency);
	m_dmadac[1]->set_frequency(sample_frequency);
	m_dmadac[0]->set_volume(0x100);
	m_dmadac[1]->set_volume(0x100);

	const uint16_t num_samples = cdic_hle::xa_samples_per_sector_per_channel(coding_info) / (18 * 28);

	uint16_t offset = 0;
	for (uint16_t i = 0; i < SECTOR_AUDIO_SIZE; i += 128, data += 128)
	{
		play_xa_group(coding_info, data, offset);
		offset += 28 * num_samples;
	}

	int16_t sampleL = 0, sampleR = 0, outL = 0, outR = 0;
	// Green Book nominal curve.  Board-family quantization and the documented
	// ADPCM high-attenuation anomaly remain outside this compatibility model.
	const double scaleLL = cdi_audio::nominal_attenuation_gain(m_atten[0]);
	const double scaleLR = cdi_audio::nominal_attenuation_gain(m_atten[1]);
	const double scaleRR = cdi_audio::nominal_attenuation_gain(m_atten[2]);
	const double scaleRL = cdi_audio::nominal_attenuation_gain(m_atten[3]);
	for (uint16_t i = 0; i < 18 * 28 * num_samples; i++)
	{
		sampleL = m_samples[0][i];
		sampleR = m_samples[coding_info.channels - 1][i];

		outL = (sampleL * scaleLL + sampleR * scaleRL) * 0.25;
		outR = (sampleL * scaleLR + sampleR * scaleRR) * 0.25;
		m_dmadac[0]->transfer(0, 1, 1, 1, &outL);
		m_dmadac[1]->transfer(0, 1, 1, 1, &outR);
	}
}

void cdicdic_device::receive_cdda_sector(const uint8_t *data)
{
	switch (cdic_hle::classify_cdda_receive(m_realtime_audio, m_audio_map.active, m_cdda_pending))
	{
	case cdic_hle::cdda_receive_action::play:
		play_cdda_sector(data);
		break;

	case cdic_hle::cdda_receive_action::buffer:
		memcpy(m_cdda_pending_data.get(), data, SECTOR_SIZE);
		m_cdda_pending = true;
		break;

	case cdic_hle::cdda_receive_action::retain_buffered:
		// Retaining the first pre-start sector is the deterministic HLE model
		// used to bridge the measured first-buffer IRQ to the later bit-11
		// start.  Later sectors still produce their measured 75 Hz
		// buffer/subcode events without overwriting that startup sector.
		break;
	}
}

void cdicdic_device::try_play_realtime_audio()
{
	if (m_audio_map.active || m_disc_mode == DISC_CDDA)
		return;

	const uint8_t index = cdic_hle::take_realtime_audio_buffer(m_realtime_audio);
	if (index == cdic_hle::NO_AUDIO_BUFFER)
		return;

	uint8_t *const ram = &m_ram[(4 + index) * 0x0a00];
	const uint8_t coding = ram[(SECTOR_CODING2 - SECTOR_HEADER) ^ 1];
	const uint8_t periods = get_sector_count_for_coding(coding);
	if (!periods)
	{
		LOGMASKED(LOG_SECTORS, "Buffered XA sector has invalid coding %02x; dropping\n", unsigned(coding));
		return;
	}

	uint8_t swapped_data[SECTOR_AUDIO_SIZE];
	const uint8_t *const encoded = ram + (SECTOR_DATA - SECTOR_HEADER);
	for (uint16_t i = 0; i < SECTOR_AUDIO_SIZE; i++)
		swapped_data[i ^ 1] = encoded[i];

	cdic_hle::begin_realtime_audio_buffer(m_realtime_audio, periods);
	play_audio_sector(coding, swapped_data);
}

void cdicdic_device::start_realtime_audio()
{
	cdic_hle::start_realtime_audio(m_realtime_audio);
	if (m_disc_mode == DISC_CDDA)
	{
		if (m_cdda_pending)
		{
			play_cdda_sector(m_cdda_pending_data.get());
			m_cdda_pending = false;
		}
		return;
	}

	try_play_realtime_audio();
}

void cdicdic_device::stop_realtime_audio()
{
	cdic_hle::stop_realtime_audio(m_realtime_audio);
	m_cdda_pending = false;
}

TIMER_CALLBACK_MEMBER( cdicdic_device::audio_tick )
{
	if (cdic_hle::advance_realtime_audio(m_realtime_audio))
		try_play_realtime_audio();

	switch (cdic_hle::advance_audio_map(m_audio_map))
	{
	case cdic_hle::audio_map_tick_action::consume_buffer:
		process_audio_map();
		break;

	case cdic_hle::audio_map_tick_action::abort_complete:
		// A cleared bit 13 masks the completion interrupt, but Mono-I leaves
		// ABUF bit 15 set after the current transfer interval finishes.
		m_audio_buffer |= 0x8000;
		update_interrupt_state();
		break;

	case cdic_hle::audio_map_tick_action::abort_before_buffer:
	case cdic_hle::audio_map_tick_action::none:
		break;
	}
}

void cdicdic_device::process_audio_map()
{
	if (m_audio_map.next_address == 0xffff)
		return;

	LOGMASKED(LOG_SAMPLES, "Processing audio map from %04x\n", m_audio_map.next_address);

	uint8_t *ram = &m_ram[m_audio_map.next_address & 0x3ffe];

	const uint8_t coding = ram[(SECTOR_CODING2 - SECTOR_HEADER) ^ 1];
	const cdic_hle::audio_map_buffer_result result = cdic_hle::consume_audio_map_buffer(m_audio_map, coding);
	LOGMASKED(LOG_SAMPLES, "Coding is %02x\n", coding);
	if (!result.terminated)
	{
		if (result.coding_valid)
		{
			ram += SECTOR_DATA - SECTOR_HEADER;
			uint8_t swapped_data[SECTOR_AUDIO_SIZE];
			for (uint16_t i = 0; i < SECTOR_AUDIO_SIZE; i++)
				swapped_data[i ^ 1] = ram[i];
			play_audio_sector(coding, swapped_data);
		}
		else
		{
			LOGMASKED(LOG_SECTORS, "Sound-map buffer has invalid coding %02x; retaining buffer cadence only\n", unsigned(coding));
		}
	}
	else
	{
		// Bit 0 reports decoder termination; bit 11 is playback enable.
		m_z_buffer = (m_z_buffer & ~cdic_hle::AUDCTL_PLAY) | cdic_hle::AUDCTL_TERMINATED;
		cdic_hle::stop_realtime_audio(m_realtime_audio);
	}

	if (result.previous_buffer_complete)
	{
		m_audio_buffer |= 0x8000;
		update_interrupt_state();
	}
}

void cdicdic_device::update_interrupt_state()
{
	const bool interrupt_active = cdic_hle::interrupt_asserted(
		m_x_buffer, m_data_buffer, m_audio_buffer, m_z_buffer);
	if (!interrupt_active)
		LOGMASKED(LOG_SECTORS, "%s: Clearing CDIC interrupt line\n", machine().describe_context());
	m_intreq_callback(interrupt_active ? ASSERT_LINE : CLEAR_LINE);
}

void cdicdic_device::descramble_sector(uint8_t *buffer)
{
	for (uint32_t i = 12; i < SECTOR_SIZE; i++)
	{
		buffer[i] ^= s_sector_scramble[i];
	}
}

bool cdicdic_device::is_valid_sector(const uint8_t *buffer)
{
	const uint32_t real_lba = m_curr_lba + 150;
	const uint8_t mins = real_lba / (60 * 75);
	const uint8_t secs = (real_lba / 75) % 60;
	const uint8_t frac = real_lba % 75;
	const uint8_t mins_bcd = ((mins / 10) << 4) | (mins % 10);
	const uint8_t secs_bcd = ((secs / 10) << 4) | (secs % 10);
	const uint8_t frac_bcd = ((frac / 10) << 4) | (frac % 10);

	// Verify MSF
	if (mins_bcd != buffer[SECTOR_MINUTES] || secs_bcd != buffer[SECTOR_SECONDS] || frac_bcd != buffer[SECTOR_FRACS])
	{
		LOGMASKED(LOG_SECTORS, "Not valid sector, MSF (%02x:%02x:%02x vs. %02x:%02x:%02x\n", mins_bcd, secs_bcd, frac_bcd, buffer[SECTOR_MINUTES], buffer[SECTOR_SECONDS], buffer[SECTOR_FRACS]);
		return false;
	}

	// Verify mode
	if (buffer[SECTOR_MODE] != 1 && buffer[SECTOR_MODE] != 2)
	{
		LOGMASKED(LOG_SECTORS, "Not valid sector, mode %02x\n", buffer[SECTOR_MODE]);
		return false;
	}

	if (buffer[SECTOR_MODE] != 2)
		return true;

	// Mode 2 subheader data is double-written for integrity.  Raw-image reads
	// do not expose the CIRC reliability flags needed to select one copy, so a
	// contradiction must not be guessed into a valid sector.
	const cdic_hle::mode2_sector first =
		{ buffer[SECTOR_FILE1], buffer[SECTOR_CHAN1], buffer[SECTOR_SUBMODE1], buffer[SECTOR_CODING1] };
	const cdic_hle::mode2_sector second =
		{ buffer[SECTOR_FILE2], buffer[SECTOR_CHAN2], buffer[SECTOR_SUBMODE2], buffer[SECTOR_CODING2] };
	const uint8_t mismatch = cdic_hle::subheader_mismatch(first, second);
	if (mismatch & cdic_hle::SUBHEADER_MISMATCH_FILE)
	{
		LOGMASKED(LOG_SECTORS, "Not valid sector, file %02x vs. %02x\n", buffer[SECTOR_FILE1], buffer[SECTOR_FILE2]);
	}
	if (mismatch & cdic_hle::SUBHEADER_MISMATCH_CHANNEL)
	{
		LOGMASKED(LOG_SECTORS, "Not valid sector, channel %02x vs. %02x\n", buffer[SECTOR_CHAN1], buffer[SECTOR_CHAN2]);
	}
	if (mismatch & cdic_hle::SUBHEADER_MISMATCH_SUBMODE)
	{
		LOGMASKED(LOG_SECTORS, "Not valid sector, submode %02x vs. %02x\n", buffer[SECTOR_SUBMODE1], buffer[SECTOR_SUBMODE2]);
	}
	if (mismatch & cdic_hle::SUBHEADER_MISMATCH_CODING)
	{
		LOGMASKED(LOG_SECTORS, "Not valid sector, coding %02x vs. %02x\n", buffer[SECTOR_CODING1], buffer[SECTOR_CODING2]);
	}

	return mismatch == 0;
}

cdic_hle::sector_decision cdicdic_device::mode2_sector_decision(const uint8_t *buffer) const
{
	return cdic_hle::select_mode2_sector(
		m_file,
		m_channel,
		m_audio_channel,
		{ buffer[SECTOR_FILE2], buffer[SECTOR_CHAN2], buffer[SECTOR_SUBMODE2], buffer[SECTOR_CODING2] });
}

TIMER_CALLBACK_MEMBER( cdicdic_device::sector_tick )
{
	if (m_disc_command == 0)
	{
		m_disc_state = uint8_t(cdic_hle::disc_state::idle);
		return;
	}

	if (m_disc_spinup_counter != 0)
	{
		LOGMASKED(LOG_SECTORS, "Sector tick, waiting on spinup\n");
		m_disc_spinup_counter--;
		return;
	}

	LOGMASKED(LOG_SECTORS, "About to process a disc sector\n");

	m_disc_state = uint8_t(cdic_hle::disc_state::reading);
	process_disc_sector();

	// Reset commands stop after the next physical sector.  Keep this in the
	// scheduler rather than process_sector_data(), because filtering may cause
	// a sector to return before it is copied to a CPU-visible buffer.
	if (cdic_hle::stops_after_physical_sector(m_command))
	{
		LOGMASKED(LOG_SECTORS, "Reset command observed after sector; stopping disc read.\n");
		cancel_disc_read();
		return;
	}

	if (m_disc_command == 0)
	{
		LOGMASKED(LOG_SECTORS, "Disc command has been reset after processing; stopping processing.\n");
		cancel_disc_read();
		return;
	}

	m_curr_lba++;
}

uint8_t cdicdic_device::get_sector_count_for_coding(uint8_t coding)
{
	return cdic_hle::xa_sector_count(coding);
}

void cdicdic_device::process_disc_sector()
{
	const cdic_hle::disc_operation operation = cdic_hle::disc_operation(m_disc_mode);
	const uint32_t real_lba = m_curr_lba + 150;
	const uint8_t mins = real_lba / (60 * 75);
	const uint8_t secs = (real_lba / 75) % 60;
	const uint8_t frac = real_lba % 75;
	const uint8_t mins_bcd = ((mins / 10) << 4) | (mins % 10);
	const uint8_t secs_bcd = ((secs / 10) << 4) | (secs % 10);
	const uint8_t frac_bcd = ((frac / 10) << 4) | (frac % 10);

	LOGMASKED(LOG_SECTORS, "Disc sector, current LBA: %08x, MSF: %02x %02x %02x\n", real_lba, mins_bcd, secs_bcd, frac_bcd);

	uint8_t buffer[2560] = { 0 };
	bool const read_ok =
			m_cdrom->read_data(
					m_curr_lba, buffer, cdrom_file::CD_TRACK_RAW_DONTCARE);

	logerror(
			"CDIC_TRACE sector disc_cmd=%02x live_cmd=%04x mode=%u "
			"lba=%u real_lba=%u msf=%02x:%02x:%02x read_ok=%u "
			"hdr=%02x%02x%02x%02x sector_mode=%02x file=%02x "
			"chan=%02x sub=%02x coding=%02x ctx=%s\n",
			unsigned(m_disc_command), unsigned(m_command),
			unsigned(m_disc_mode), unsigned(m_curr_lba),
			unsigned(real_lba), unsigned(mins_bcd),
			unsigned(secs_bcd), unsigned(frac_bcd),
			read_ok ? 1U : 0U,
			unsigned(buffer[0]), unsigned(buffer[1]),
			unsigned(buffer[2]), unsigned(buffer[3]),
			unsigned(buffer[SECTOR_MODE]),
			unsigned(buffer[SECTOR_FILE2]),
			unsigned(buffer[SECTOR_CHAN2]),
			unsigned(buffer[SECTOR_SUBMODE2]),
			unsigned(buffer[SECTOR_CODING2]),
			machine().describe_context());

	if (!read_ok)
	{
		// No status bit is known for end-of-disc.  Terminating the HLE
		// operation is safer than delivering a fabricated all-zero sector.
		LOGMASKED(LOG_SECTORS, "Disc read failed at LBA %u; terminating command\n", m_curr_lba);
		cancel_disc_read();
		return;
	}

	// Detect (badly) if we're dealing with a byteswapped loose-bin image
	if (buffer[0] == 0xff && buffer[1] == 0x00)
	{
		LOGMASKED(LOG_SECTORS, "Byteswapping\n");
		m_cd_byteswap = true;
	}

	if (m_cd_byteswap)
	{
		for (uint16_t i = 0; i < 2560; i += 2)
		{
			std::swap(buffer[i], buffer[i + 1]);
		}
	}

	// CD-DA sectors contain headerless PCM and must never be fed through the
	// Mode 1/2 validation or descrambling path.
	if (cdic_hle::validates_sector_header(operation) && !is_valid_sector(buffer))
	{
		uint8_t descramble_buffer[2560];
		memcpy(descramble_buffer, buffer, sizeof(descramble_buffer));
		LOGMASKED(LOG_SECTORS, "Sector seems to be encoded, attempting to apply descrambling\n");
		descramble_sector(descramble_buffer);

		if (!is_valid_sector(descramble_buffer))
		{
			if (cdic_hle::discards_invalid_sector(operation))
			{
				LOGMASKED(LOG_SECTORS, "Mode 2 sector remains invalid after descrambling; dropping without delivery\n");
				return;
			}
			LOGMASKED(LOG_SECTORS, "Sector remains invalid after descrambling; retaining compatibility delivery\n");
		}
		else
		{
			memcpy(buffer, descramble_buffer, sizeof(descramble_buffer));
		}
	}

	if (m_disc_mode == DISC_MODE2 && buffer[SECTOR_MODE] != 2)
	{
		LOGMASKED(LOG_SECTORS, "Mode 2 read encountered sector mode %02x; dropping without delivery\n", buffer[SECTOR_MODE]);
		return;
	}

	LOGMASKED(LOG_SECTORS, "Sector header data: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
		buffer[ 0], buffer[ 1], buffer[ 2], buffer[ 3], buffer[ 4], buffer[ 5], buffer[ 6], buffer[ 7], buffer[ 8], buffer[ 9],
		buffer[10], buffer[11], buffer[12], buffer[13], buffer[14], buffer[15], buffer[16], buffer[17], buffer[18], buffer[19],
		buffer[20], buffer[21], buffer[22], buffer[23]);

	bool audio_sector = false;
	if (buffer[SECTOR_MODE] == 2 && m_disc_mode == DISC_MODE2)
	{
		const cdic_hle::sector_decision decision = mode2_sector_decision(buffer);
		if (decision.target == cdic_hle::sector_target::malformed)
		{
			LOGMASKED(LOG_SECTORS, "Malformed Mode 2 sector dropped (file %02x, channel %02x, submode %02x, coding %02x, status %u)\n",
				buffer[SECTOR_FILE2], buffer[SECTOR_CHAN2], buffer[SECTOR_SUBMODE2], buffer[SECTOR_CODING2],
				unsigned(decision.format));
			return;
		}
		if (decision.target == cdic_hle::sector_target::filtered)
		{
			LOGMASKED(LOG_SECTORS, "Mode 2 sector filtered (file %02x, channel %02x, submode %02x)\n",
				buffer[SECTOR_FILE2], buffer[SECTOR_CHAN2], buffer[SECTOR_SUBMODE2]);
			return;
		}

		if (decision.trigger)
			LOGMASKED(LOG_SECTORS, "Mode 2 Trigger sector accepted\n");
		if (decision.end_record)
			LOGMASKED(LOG_SECTORS, "Mode 2 EOR sector accepted on a selected channel\n");

		if (decision.end_read)
		{
			LOGMASKED(LOG_SECTORS, "Mode 2 EOF sector accepted; terminating after delivery\n");
			m_disc_command = 0;
		}

		audio_sector = decision.target == cdic_hle::sector_target::audio;
		if (audio_sector)
			LOGMASKED(LOG_SECTORS, "Audio is selected\n");
	}
	else if (m_disc_mode == DISC_CDDA)
	{
		// Byteswap if not already detected as byteswapped
		if (!m_cd_byteswap)
		{
			uint8_t swapped_buffer[SECTOR_SIZE];
			for (uint16_t i = 0; i < SECTOR_SIZE; i += 2)
			{
				swapped_buffer[i + 1] = buffer[i + 0];
				swapped_buffer[i + 0] = buffer[i + 1];
			}
			receive_cdda_sector(swapped_buffer);
		}
		else
		{
			receive_cdda_sector(buffer);
		}
	}

	// Calculate subcode data
	uint8_t subcode_buffer[96];
	memset(subcode_buffer, 0, sizeof(subcode_buffer));

	if (m_disc_mode == DISC_TOC)
	{
		uint8_t *toc_buffer = buffer;
		const cdrom_file::toc &toc = m_cdrom->get_toc();
		uint32_t entry_count = 0;

		// Determine total frame count for data, and total audio track count
		uint32_t frames = toc.tracks[0].pregap;
		int audio_tracks = 0;
		int other_tracks = 0;
		uint32_t audio_starts[cdrom_file::MAX_TRACKS];
		for (uint32_t i = 0; i < toc.numtrks; i++)
		{
			if (toc.tracks[i].trktype != cdrom_file::CD_TRACK_AUDIO)
			{
				frames += toc.tracks[i].frames + toc.tracks[i].extraframes;
			}
			else
			{
				audio_starts[audio_tracks++] = toc.tracks[i].logframeofs;
			}
		}

		// Determine last-frame MSF
		const uint8_t total_mins = frames / (60 * 75);
		const uint8_t total_secs = (frames / 75) % 60;
		const uint8_t total_frac = frames % 75;

		// Specify any audio tracks first
		for (int i = 0; i < audio_tracks; i++)
		{
			const uint8_t audio_mins = audio_starts[i] / (60 * 75);
			const uint8_t audio_secs = (audio_starts[i] / 75) % 60;
			const uint8_t audio_frac = audio_starts[i] % 75;
			const uint8_t audio_mins_bcd = ((audio_mins / 10) << 4) | (audio_mins % 10);
			const uint8_t audio_secs_bcd = ((audio_secs / 10) << 4) | (audio_secs % 10);
			const uint8_t audio_frac_bcd = ((audio_frac / 10) << 4) | (audio_frac % 10);

			const uint8_t track_bcd = (((i + 1) / 10) << 4) | ((i + 1) % 10);

			for (int j = 0; j < 3; j++)
			{
				*toc_buffer++ = 0x01;       // Track type (CD-DA)
				*toc_buffer++ = track_bcd;  // Track number
				*toc_buffer++ = audio_mins_bcd;
				*toc_buffer++ = audio_secs_bcd;
				*toc_buffer++ = audio_frac_bcd;
				entry_count++;
			}
		}

		// Packet A0 (lead-in)
		for (int i = 0; i < 3; i++)
		{
			*toc_buffer++ = (other_tracks > 0) ? 0x41 : 0x01;
			*toc_buffer++ = 0xa0;
			*toc_buffer++ = 0x01;
			*toc_buffer++ = (other_tracks > 0) ? 0x10 : 0x00;
			*toc_buffer++ = 0x00;
			entry_count++;
		}

		// Packet A1
		for (int i = 0; i < 3; i++)
		{
			*toc_buffer++ = (audio_tracks > 0) ? 0x01 : 0x41;
			*toc_buffer++ = 0xa1;
			if (audio_tracks > 0)
			{
				uint8_t last_audio_track = (uint8_t)(audio_tracks - 1);
				*toc_buffer++ = ((last_audio_track / 10) << 4) | (last_audio_track % 10);
			}
			else
			{
				*toc_buffer++ = 0x00;
			}
			*toc_buffer++ = 0x00;
			*toc_buffer++ = 0x00;
			entry_count++;
		}

		// Packet A2 (lead-out)
		for (int i = 0; i < 3; i++)
		{
			*toc_buffer++ = (audio_tracks > 0) ? 0x01 : 0x41;
			*toc_buffer++ = 0xa2;
			*toc_buffer++ = ((total_mins / 10) << 4) | (total_mins % 10);
			*toc_buffer++ = ((total_secs / 10) << 4) | (total_secs % 10);
			*toc_buffer++ = ((total_frac / 10) << 4) | (total_frac % 10);
			entry_count++;
		}

		uint8_t *toc_data = &buffer[(m_curr_lba % entry_count) * 5];

		subcode_buffer[SUBCODE_Q_CONTROL] = toc_data[0];
		subcode_buffer[SUBCODE_Q_TRACK] = 0x00;
		subcode_buffer[SUBCODE_Q_INDEX] = toc_data[1];
		subcode_buffer[SUBCODE_Q_MODE1_MINS] = 0xa0;
		subcode_buffer[SUBCODE_Q_MODE1_SECS] = secs_bcd;
		subcode_buffer[SUBCODE_Q_MODE1_FRAC] = frac_bcd;
		subcode_buffer[SUBCODE_Q_MODE1_ZERO] = 0x00;
		subcode_buffer[SUBCODE_Q_MODE1_AMINS] = toc_data[2];
		subcode_buffer[SUBCODE_Q_MODE1_ASECS] = toc_data[3];
		subcode_buffer[SUBCODE_Q_MODE1_AFRAC] = toc_data[4];
		subcode_buffer[SUBCODE_Q_CRC0] = 0xff;
		subcode_buffer[SUBCODE_Q_CRC1] = 0xff;
	}
	else
	{
		subcode_buffer[SUBCODE_Q_CONTROL] = (m_disc_mode == DISC_CDDA ? 0x01 : 0x41);
		subcode_buffer[SUBCODE_Q_TRACK] = 0x01;
		subcode_buffer[SUBCODE_Q_INDEX] = 0x01;
		subcode_buffer[SUBCODE_Q_MODE1_MINS] = mins_bcd;
		subcode_buffer[SUBCODE_Q_MODE1_SECS] = secs_bcd;
		subcode_buffer[SUBCODE_Q_MODE1_FRAC] = frac_bcd;
		subcode_buffer[SUBCODE_Q_MODE1_ZERO] = 0x00;
		subcode_buffer[SUBCODE_Q_MODE1_AMINS] = mins_bcd;
		subcode_buffer[SUBCODE_Q_MODE1_ASECS] = secs_bcd;
		subcode_buffer[SUBCODE_Q_MODE1_AFRAC] = frac_bcd;
		subcode_buffer[SUBCODE_Q_CRC0] = 0xff;
		subcode_buffer[SUBCODE_Q_CRC1] = 0xff;
	}

	uint16_t crc_accum = 0;
	for (int i = 0; i < 12; i++)
		crc_accum = CRC_CCITT_ROUND(crc_accum, subcode_buffer[SUBCODE_Q_CONTROL + i]);

	subcode_buffer[SUBCODE_Q_CRC0] = (uint8_t)(crc_accum >> 8);
	subcode_buffer[SUBCODE_Q_CRC1] = (uint8_t)crc_accum;

	process_sector_data(buffer, subcode_buffer, audio_sector);
}

void cdicdic_device::process_sector_data(const uint8_t *buffer, const uint8_t *subcode_buffer, bool audio_sector)
{
	const cdic_hle::buffer_completion completion = cdic_hle::complete_buffer(
		m_data_buffer, audio_sector, m_next_data_buffer, m_next_audio_buffer);
	m_data_buffer = completion.data_buffer;
	m_next_data_buffer = completion.next_data_buffer;
	m_next_audio_buffer = completion.next_audio_buffer;
	uint16_t *dev_buffer = reinterpret_cast<uint16_t *>(&m_ram[completion.byte_offset]);

	if (!cdic_hle::stores_sector_payload_in_ram(cdic_hle::disc_operation(m_disc_mode)))
	{
		// Mono-I captures show that CD-DA PCM bypasses CDIC RAM.  Only its
		// subcode is written at byte offset $924 in the alternating buffers.
		dev_buffer += cdic_hle::CDIC_SUBCODE_BYTE_OFFSET / 2;
	}
	else
	{
		for (int i = SECTOR_HEADER; i < SECTOR_FILE2; i += 2)
			*dev_buffer++ = ((uint16_t)buffer[i] << 8) | buffer[i + 1];

		for (int i = SECTOR_FILE2; i < SECTOR_SIZE; i += 2)
			*dev_buffer++ = ((uint16_t)buffer[i] << 8) | buffer[i + 1];
	}

	for (int i = SUBCODE_Q_CONTROL; i <= SUBCODE_Q_CRC1; i++)
		*dev_buffer++ = subcode_buffer[i];

	m_x_buffer |= 0x8000;
	update_interrupt_state();

	if (audio_sector)
	{
		cdic_hle::mark_realtime_audio_ready(m_realtime_audio, completion.data_buffer & 1);
		try_play_realtime_audio();
	}

}

uint16_t cdicdic_device::regs_r(offs_t offset, uint16_t mem_mask)
{
	uint32_t addr = offset + 0x3c00/2;

	switch (addr)
	{
		case 0x3c00/2: // Command register
			LOGMASKED(LOG_READS, "%s: cdic_r: Command Register = %04x & %04x\n", machine().describe_context(), m_command, mem_mask);
			return m_command;

		case 0x3c02/2: // Time register (MSW)
			LOGMASKED(LOG_READS, "%s: cdic_r: Time Register (MSW) = %04x & %04x\n", machine().describe_context(), m_time >> 16, mem_mask);
			return m_time >> 16;

		case 0x3c04/2: // Time register (LSW)
			LOGMASKED(LOG_READS, "%s: cdic_r: Time Register (LSW) = %04x & %04x\n", machine().describe_context(), (uint16_t)(m_time & 0x0000ffff), mem_mask);
			return m_time & 0x0000ffff;

		case 0x3c06/2: // File register
			LOGMASKED(LOG_READS, "%s: cdic_r: File Register = %04x & %04x\n", machine().describe_context(), m_file, mem_mask);
			return m_file;

		case 0x3c08/2: // Channel register (MSW)
			LOGMASKED(LOG_READS, "%s: cdic_r: Channel Register (MSW) = %04x & %04x\n", machine().describe_context(), m_channel >> 16, mem_mask);
			return m_channel >> 16;

		case 0x3c0a/2: // Channel register (LSW)
			LOGMASKED(LOG_READS, "%s: cdic_r: Channel Register (LSW) = %04x & %04x\n", machine().describe_context(), m_channel & 0x0000ffff, mem_mask);
			return m_channel & 0x0000ffff;

		case 0x3c0c/2: // Audio Channel register
			LOGMASKED(LOG_READS, "%s: cdic_r: Audio Channel Register = %04x & %04x\n", machine().describe_context(), m_audio_channel, mem_mask);
			return m_audio_channel;

		case 0x3c80/2: // DSEL
			LOGMASKED(LOG_READS, "%s: cdic_r: Data Select Register = %04x & %04x\n", machine().describe_context(), m_data_select, mem_mask);
			return m_data_select;

		case 0x3ff4/2: // ABUF
		{
			uint16_t temp = m_audio_buffer;
			LOGMASKED(LOG_READS, "%s: cdic_r: Audio Buffer Register = %04x & %04x\n", machine().describe_context(), temp, mem_mask);
			m_audio_buffer = cdic_hle::acknowledge_interrupt_source(m_audio_buffer);
			update_interrupt_state();
			return temp;
		}

		case 0x3ff6/2: // XBUF
		{
			uint16_t temp = m_x_buffer;
			LOGMASKED(LOG_READS, "%s: cdic_r: X-Buffer Register = %04x & %04x\n", machine().describe_context(), temp, mem_mask);
			m_x_buffer = cdic_hle::acknowledge_interrupt_source(m_x_buffer);
			update_interrupt_state();
			return temp;
		}

		case 0x3ff8/2: // DMACTL
			LOGMASKED(LOG_READS, "%s: cdic_r: DMA Control Register = %04x & %04x\n", machine().describe_context(), m_dma_control, mem_mask);
			return m_dma_control;

		case 0x3ffa/2: // AUDCTL
		{
			const uint16_t temp = m_z_buffer;
			LOGMASKED(LOG_READS, "%s: cdic_r: Audio Control Register Read: %04x & %04x\n", machine().describe_context(), temp, mem_mask);
			// Decoder-termination status is read-to-clear, not a synthetic toggle.
			m_z_buffer = cdic_hle::acknowledge_audio_termination(m_z_buffer);
			return temp;
		}

		case 0x3ffc/2: // IVEC
			LOGMASKED(LOG_READS, "%s: cdic_r: Interrupt Vector Register = %04x & %04x\n", machine().describe_context(), m_interrupt_vector, mem_mask);
			return m_interrupt_vector;

		case 0x3ffe/2:
			logerror(
					"CDIC_DBUF_TRACE read value=%04x mask=%04x "
					"cmd=%04x disc_cmd=%02x mode=%u lba=%u ctx=%s\\n",
					unsigned(m_data_buffer), unsigned(mem_mask),
					unsigned(m_command), unsigned(m_disc_command),
					unsigned(m_disc_mode), unsigned(m_curr_lba),
					machine().describe_context());
			LOGMASKED(LOG_READS, "%s: cdic_r: Data buffer Register = %04x & %04x\n", machine().describe_context(), m_data_buffer, mem_mask);
			return m_data_buffer;

		default:
			LOGMASKED(LOG_READS | LOG_UNKNOWNS, "%s: cdic_r: Unknown address: %04x & %04x\n", machine().describe_context(), addr*2, mem_mask);
			return 0;
	}
}

void cdicdic_device::regs_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	uint32_t addr = offset + 0x3c00/2;

	switch (addr)
	{
		case 0x3c00/2: // Command register
			LOGMASKED(LOG_WRITES, "%s: cdic_w: Command Register = %04x & %04x\n", machine().describe_context(), data, mem_mask);
			COMBINE_DATA(&m_command);
			break;

		case 0x3c02/2: // Time register (MSW)
			m_time &= ~(mem_mask << 16);
			m_time |= (data & mem_mask) << 16;
			LOGMASKED(LOG_WRITES, "%s: cdic_w: Time Register (MSW) = %04x & %04x\n", machine().describe_context(), data, mem_mask);
			break;

		case 0x3c04/2: // Time register (LSW)
			m_time &= ~mem_mask;
			m_time |= data & mem_mask;
			LOGMASKED(LOG_WRITES, "%s: cdic_w: Time Register (LSW) = %04x & %04x\n", machine().describe_context(), data, mem_mask);
			break;

		case 0x3c06/2: // File register
			LOGMASKED(LOG_WRITES, "%s: cdic_w: File Register = %04x & %04x\n", machine().describe_context(), data, mem_mask);
			COMBINE_DATA(&m_file);
			break;

		case 0x3c08/2: // Channel register (MSW)
			m_channel &= ~(mem_mask << 16);
			m_channel |= (data & mem_mask) << 16;
			LOGMASKED(LOG_WRITES, "%s: cdic_w: Channel Register (MSW) = %04x & %04x\n", machine().describe_context(), data, mem_mask);
			break;

		case 0x3c0a/2: // Channel register (LSW)
			m_channel &= ~mem_mask;
			m_channel |= data & mem_mask;
			LOGMASKED(LOG_WRITES, "%s: cdic_w: Channel Register (LSW) = %04x & %04x\n", machine().describe_context(), data, mem_mask);
			break;

		case 0x3c0c/2: // Audio Channel register
			LOGMASKED(LOG_WRITES, "%s: cdic_w: Audio Channel Register = %04x & %04x\n", machine().describe_context(), data, mem_mask);
			COMBINE_DATA(&m_audio_channel);
			break;

		case 0x3c80/2: // DSEL
			LOGMASKED(LOG_WRITES, "%s: cdic_w: Data Select Register = %04x & %04x\n", machine().describe_context(), data, mem_mask);
			COMBINE_DATA(&m_data_select);
			break;

		case 0x3ff4/2:
			LOGMASKED(LOG_WRITES, "%s: cdic_w: Audio Buffer Register = %04x & %04x\n", machine().describe_context(), data, mem_mask);
			COMBINE_DATA(&m_audio_buffer);
			update_interrupt_state();
			break;

		case 0x3ff6/2:
			LOGMASKED(LOG_WRITES, "%s: cdic_w: X Buffer Register = %04x & %04x\n", machine().describe_context(), data, mem_mask);
			COMBINE_DATA(&m_x_buffer);
			update_interrupt_state();
			break;

		case 0x3ff8/2:
		{
			LOGMASKED(LOG_WRITES, "%s: cdic_w: DMA Control Register = %04x & %04x\n", machine().describe_context(), data, mem_mask);
			COMBINE_DATA(&m_dma_control);

			uint32_t device_index = (m_dma_control & 0x3fff) >> 1;
			uint16_t *ram = (uint16_t *)m_ram.get();

			// SCC68070 channel 1 owns the memory-side DMA cycle.
			// CDIC supplies or consumes only the device-side operand.
			while (m_scc->dma_channel1_active())
			{
				uint16_t operand = ram[device_index];

				if (!m_scc->dma_channel1_transfer(operand))
					break;

				ram[device_index++] = operand;
			}
			break;
		}

		case 0x3ffa/2:
		{
			LOGMASKED(LOG_WRITES, "%s: cdic_w: Z-Buffer Register Write: %04x & %04x\n", machine().describe_context(), data, mem_mask);
			m_z_buffer = cdic_hle::merge_audio_control(m_z_buffer, data, mem_mask);
			switch (cdic_hle::classify_audio_control(m_z_buffer, m_audio_map.active))
			{
			case cdic_hle::audio_control_action::stop:
				stop_realtime_audio();
				cdic_hle::request_audio_map_stop(m_audio_map);
				break;

			case cdic_hle::audio_control_action::start_sound_map:
				if (cdic_hle::start_audio_map(m_audio_map, m_z_buffer))
					cdic_hle::reset_xa_history(m_xa_last);
				break;

			case cdic_hle::audio_control_action::start_realtime:
				start_realtime_audio();
				break;

			case cdic_hle::audio_control_action::none:
				break;
			}
			update_interrupt_state();
			break;
		}

		case 0x3ffc/2:
			LOGMASKED(LOG_WRITES, "%s: cdic_w: Interrupt Vector Register = %04x & %04x\n", machine().describe_context(), data, mem_mask);
			COMBINE_DATA(&m_interrupt_vector);
			break;

		case 0x3ffe/2:
		{
			uint16_t const old_data_buffer = m_data_buffer;

			LOGMASKED(LOG_WRITES, "%s: cdic_w: Data Buffer Register = %04x & %04x\n", machine().describe_context(), data, mem_mask);
			COMBINE_DATA(&m_data_buffer);

			logerror(
					"CDIC_DBUF_TRACE write data=%04x mask=%04x old=%04x "
					"combined=%04x cmd=%04x disc_cmd=%02x mode=%u "
					"lba=%u ctx=%s\\n",
					unsigned(data), unsigned(mem_mask),
					unsigned(old_data_buffer), unsigned(m_data_buffer),
					unsigned(m_command), unsigned(m_disc_command),
					unsigned(m_disc_mode), unsigned(m_curr_lba),
					machine().describe_context());

			if (m_data_buffer & 0x8000)
			{
				LOGMASKED(LOG_WRITES, "%s: cdic_w: Data Buffer high-bit set, beginning command processing\n", machine().describe_context());
				handle_cdic_command();
			}
			if (!(m_data_buffer & 0x4000))
			{
				m_disc_state = uint8_t(cdic_hle::disc_state::idle);
				m_disc_command = 0;
				m_disc_mode = 0;
				m_disc_spinup_counter = 0;
				m_curr_lba = 0;
				m_next_data_buffer = cdic_hle::RESET_NEXT_DATA_BUFFER;
				m_next_audio_buffer = cdic_hle::RESET_NEXT_AUDIO_BUFFER;
			}
			update_interrupt_state();

			logerror(
					"CDIC_DBUF_TRACE write-complete value=%04x "
					"cmd=%04x disc_cmd=%02x mode=%u lba=%u ctx=%s\\n",
					unsigned(m_data_buffer), unsigned(m_command),
					unsigned(m_disc_command), unsigned(m_disc_mode),
					unsigned(m_curr_lba), machine().describe_context());
			break;
		}

		default:
			LOGMASKED(LOG_WRITES | LOG_UNKNOWNS, "%s: cdic_w: Unknown address: %04x = %04x & %04x\n", machine().describe_context(), addr*2, data, mem_mask);
			break;
	}
}


void cdicdic_device::atten_w(uint32_t state)
{
	m_atten[0] = (state & 0xff000000) >> 24;
	m_atten[1] = (state & 0x00ff0000) >> 16;
	m_atten[2] = (state & 0x0000ff00) >> 8;
	m_atten[3] = (state & 0x000000ff);
}

void cdicdic_device::init_disc_read(uint8_t disc_mode)
{
	m_disc_command = m_command;
	m_disc_mode = disc_mode;
	m_disc_state = uint8_t(cdic_hle::disc_state::seeking);
	m_curr_lba = lba_from_time();
	if (disc_mode == DISC_MODE2)
		cdic_hle::reset_realtime_audio_buffers(m_realtime_audio);
	else if (disc_mode == DISC_CDDA)
		m_cdda_pending = false;
	logerror(
			"CDIC_TRACE begin cmd=%04x mode=%u time=%08x lba=%u "
			"file=%04x channel=%08x audio=%04x dsel=%04x data=%04x ctx=%s\n",
			unsigned(m_command), unsigned(disc_mode), unsigned(m_time),
			unsigned(m_curr_lba), unsigned(m_file), unsigned(m_channel),
			unsigned(m_audio_channel), unsigned(m_data_select),
			unsigned(m_data_buffer), machine().describe_context());
	// Compatibility timing: the real seek delay depends on disc position, but
	// the HLE has no servo feedback.  Six sectors is firmware-observed only.
	m_disc_spinup_counter = 6; // Bugfix #14462: 6 or higher is required to prevent some softlocks.
}

void cdicdic_device::cancel_disc_read()
{
	logerror(
			"CDIC_TRACE cancel disc_cmd=%02x live_cmd=%04x mode=%u "
			"time=%08x lba=%u ctx=%s\n",
			unsigned(m_disc_command), unsigned(m_command),
			unsigned(m_disc_mode), unsigned(m_time),
			unsigned(m_curr_lba), machine().describe_context());
	m_disc_state = uint8_t(cdic_hle::disc_state::idle);
	m_disc_command = 0;
	m_disc_mode = 0;
	m_curr_lba = 0;
	m_disc_spinup_counter = 0;
}

void cdicdic_device::handle_cdic_command()
{
	logerror(
			"CDIC_TRACE command cmd=%04x time=%08x disc_cmd=%02x "
			"mode=%u lba=%u file=%04x channel=%08x audio=%04x "
			"dsel=%04x data=%04x ctx=%s\n",
			unsigned(m_command), unsigned(m_time),
			unsigned(m_disc_command), unsigned(m_disc_mode),
			unsigned(m_curr_lba), unsigned(m_file),
			unsigned(m_channel), unsigned(m_audio_channel),
			unsigned(m_data_select), unsigned(m_data_buffer),
			machine().describe_context());
	const cdic_hle::command_descriptor descriptor = cdic_hle::describe_command(m_command);
	switch (descriptor.kind)
	{
		case cdic_hle::command::reset_mode1:
			LOGMASKED(LOG_WRITES, "%s: cdic_w: Reset Mode 1 command\n", machine().describe_context());
			// Reset commands do not initiate a disc read.  While a read is
			// active, sector_tick() observes the live command register and
			// stops after the next physical sector.
			break;
		case cdic_hle::command::reset_mode2:
			LOGMASKED(LOG_WRITES, "%s: cdic_w: Reset Mode 2 command\n", machine().describe_context());
			// Same stop-after-sector behavior as Reset Mode 1.
			break;
		case cdic_hle::command::stop_cdda:
			LOGMASKED(LOG_WRITES, "%s: cdic_w: Stop CDDA command\n", machine().describe_context());
			stop_realtime_audio();
			cancel_disc_read();
			break;
		case cdic_hle::command::update:
			LOGMASKED(LOG_WRITES, "%s: cdic_w: Update command\n", machine().describe_context());
			break;
		case cdic_hle::command::fetch_toc:
			init_disc_read(DISC_TOC);
			break;
		case cdic_hle::command::play_cdda:
			init_disc_read(DISC_CDDA);
			break;
		case cdic_hle::command::read_mode1:
		case cdic_hle::command::seek:
			init_disc_read(DISC_MODE1);
			break;
		case cdic_hle::command::read_mode2:
			init_disc_read(DISC_MODE2);
			break;
		case cdic_hle::command::unknown:
			LOGMASKED(LOG_COMMANDS | LOG_UNKNOWNS, "%s: Unknown CDIC command %04x\n", machine().describe_context(), m_command);
			break;
	}

	m_data_buffer &= ~0x8000;
}

uint32_t cdicdic_device::lba_from_time()
{
	return cdic_hle::lba_from_time(m_time);
}

//**************************************************************************
//  LIVE DEVICE
//**************************************************************************

//-------------------------------------------------
//  cdicdic_device - constructor
//-------------------------------------------------

cdicdic_device::cdicdic_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, CDI_CDIC, tag, owner, clock)
	, m_intreq_callback(*this)
	, m_dmadac(*this, ":dac%u", 1U)
	, m_scc(*this, ":maincpu")
	, m_cdrom(*this, ":cdrom")
	, m_clock2(clock)
{
}

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void cdicdic_device::device_start()
{
	m_ram = std::make_unique<uint8_t[]>(0x4000);
	m_cdda_pending_data = std::make_unique<uint8_t[]>(SECTOR_SIZE);
	m_samples[0] = std::make_unique<int16_t[]>(s_samples_per_sector * 8 + 16);
	m_samples[1] = std::make_unique<int16_t[]>(s_samples_per_sector * 8 + 16);

	save_pointer(NAME(m_ram), 0x4000);
	save_pointer(NAME(m_cdda_pending_data), SECTOR_SIZE);
	save_pointer(NAME(m_samples[0]), s_samples_per_sector * 8 + 16);
	save_pointer(NAME(m_samples[1]), s_samples_per_sector * 8 + 16);

	save_item(NAME(m_command));
	save_item(NAME(m_time));
	save_item(NAME(m_file));
	save_item(NAME(m_channel));
	save_item(NAME(m_audio_channel));
	save_item(NAME(m_data_select));
	save_item(NAME(m_audio_buffer));
	save_item(NAME(m_x_buffer));
	save_item(NAME(m_dma_control));
	save_item(NAME(m_z_buffer));
	save_item(NAME(m_interrupt_vector));
	save_item(NAME(m_data_buffer));
	save_item(NAME(m_cd_byteswap));

	save_item(NAME(m_disc_command));
	save_item(NAME(m_disc_state));
	save_item(NAME(m_disc_mode));
	save_item(NAME(m_disc_spinup_counter));
	save_item(NAME(m_curr_lba));
	save_item(NAME(m_next_data_buffer));
	save_item(NAME(m_next_audio_buffer));

	save_item(NAME(m_audio_map.periods_remaining));
	save_item(NAME(m_audio_map.format_periods));
	save_item(NAME(m_audio_map.active));
	save_item(NAME(m_audio_map.stop_requested));
	save_item(NAME(m_audio_map.next_address));
	save_item(NAME(m_realtime_audio.ready));
	save_item(NAME(m_realtime_audio.next_play));
	save_item(NAME(m_realtime_audio.periods_remaining));
	save_item(NAME(m_realtime_audio.enabled));
	save_item(NAME(m_cdda_pending));

	save_item(NAME(m_atten));
	save_item(NAME(m_xa_last));

	m_audio_timer = timer_alloc(FUNC(cdicdic_device::audio_tick), this);
	m_audio_timer->adjust(attotime::never);

	m_sector_timer = timer_alloc(FUNC(cdicdic_device::sector_tick), this);
	m_sector_timer->adjust(attotime::never);
}

//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void cdicdic_device::device_reset()
{
	m_command = 0;
	m_time = 0;
	m_file = 0;
	m_channel = 0xffffffff;
	m_audio_channel = 0xffff;
	m_data_select = 0;
	m_audio_buffer = 0;
	m_x_buffer = 0;
	m_dma_control = 0;
	m_z_buffer = cdic_hle::AUDCTL_RESET_READBACK;
	m_interrupt_vector = 0x0f;
	m_data_buffer = 0;

	m_cd_byteswap = false;

	m_disc_state = uint8_t(cdic_hle::disc_state::idle);
	m_disc_command = 0;
	m_disc_mode = 0;
	m_disc_spinup_counter = 0;
	m_curr_lba = 0;
	m_next_data_buffer = cdic_hle::RESET_NEXT_DATA_BUFFER;
	m_next_audio_buffer = cdic_hle::RESET_NEXT_AUDIO_BUFFER;

	m_audio_map = {};
	m_realtime_audio = {};
	m_cdda_pending = false;
	std::fill_n(m_cdda_pending_data.get(), SECTOR_SIZE, 0);

	m_audio_timer->adjust(attotime::from_hz(75), 0, attotime::from_hz(75));
	m_sector_timer->adjust(attotime::from_hz(75), 0, attotime::from_hz(75));

	m_intreq_callback(CLEAR_LINE);

	m_dmadac[0]->enable(1);
	m_dmadac[1]->enable(1);

	std::fill_n(m_atten, 4, 0);
	cdic_hle::reset_xa_history(m_xa_last);
}

void cdicdic_device::ram_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	LOGMASKED(LOG_RAM, "%s: ram_w: %04x = %04x & %04x\n", machine().describe_context(), offset << 1, data, mem_mask);
	COMBINE_DATA((uint16_t *)&m_ram[offset << 1]);
}

uint16_t cdicdic_device::ram_r(offs_t offset, uint16_t mem_mask)
{
	const uint16_t data = ((uint16_t)m_ram[(offset << 1) + 1] << 8) | m_ram[offset << 1];
	LOGMASKED(LOG_RAM, "%s: ram_r: %04x : %04x & %04x\n", machine().describe_context(), offset << 1, data, mem_mask);
	return data;
}

uint8_t cdicdic_device::intack_r()
{
	return m_interrupt_vector & 0xff;
}
