#ifndef IRSDK_DEFINES_H
#define IRSDK_DEFINES_H

/*
 * iRacing SDK struct definitions – must match the shared-memory layout
 * written by iRacing (32-bit, 16-byte-aligned structs).
 *
 * CRITICAL: Every struct here uses explicit padding fields so that
 * sizeof() and member offsets are identical to the official SDK.
 * Removing or reordering ANY field will silently break data reads.
 */

#include <cstring>   // memset (used by varHeader::clear)

// ─── Constants ───────────────────────────────────────────────
#define IRSDK_MAX_BUFS   4
#define IRSDK_MAX_STRING 32
#define IRSDK_MAX_DESC   64

// Shared memory / event names
#define IRSDK_MEMMAPFILENAME    "Local\\IRSDKMemMapFileName"
#define IRSDK_DATAVALIDEVENTNAME "Local\\IRSDKDataValidEvent"

// Session constants
#define IRSDK_UNLIMITED_LAPS 32767
#define IRSDK_UNLIMITED_TIME 604800.0f

// ─── Status flags ────────────────────────────────────────────
enum irsdk_StatusField {
    irsdk_stConnected = 1
};

// ─── Variable types ──────────────────────────────────────────
enum irsdk_VarType {
    irsdk_char     = 0,   // 1 byte
    irsdk_bool     = 1,   // 1 byte  (stored as int in the buffer though)
    irsdk_int      = 2,   // 4 bytes
    irsdk_bitField = 3,   // 4 bytes
    irsdk_float    = 4,   // 4 bytes
    irsdk_double   = 5    // 8 bytes
};

// ─── Variable header  (144 bytes) ────────────────────────────
// Official layout (all 16-byte aligned):
//   int   type          +0
//   int   offset        +4
//   int   count         +8
//   bool  countAsTime   +12
//   char  pad[3]        +13   ← 16-byte align
//   char  name[32]      +16
//   char  desc[64]      +48
//   char  unit[32]      +112
//                       =144
struct irsdk_varHeader {
    int   type;                      // irsdk_VarType
    int   offset;                    // offset from start of buffer row
    int   count;                     // number of entries (array)
    bool  countAsTime;
    char  pad[3];                    // (16 byte align)

    char  name[IRSDK_MAX_STRING];    // variable name
    char  desc[IRSDK_MAX_DESC];      // description
    char  unit[IRSDK_MAX_STRING];    // unit, e.g. "kg/m^2"

    void clear()
    {
        type = 0;
        offset = 0;
        count = 0;
        countAsTime = false;
        memset(pad, 0, sizeof(pad));
        memset(name, 0, sizeof(name));
        memset(desc, 0, sizeof(desc));
        memset(unit, 0, sizeof(unit));
    }
};

// ─── Data-buffer descriptor  (16 bytes) ──────────────────────
// Official layout:
//   int tickCount   +0
//   int bufOffset   +4
//   int pad[2]      +8   ← 16-byte align
//                   =16
struct irsdk_varBuf {
    int tickCount;       // used to detect changes in data
    int bufOffset;       // offset from header
    int pad[2];          // (16 byte align)
};

// ─── Main header  (112 bytes + 4*16 = 176 bytes total) ──────
// Official layout:
//   int ver                    +0
//   int status                 +4
//   int tickRate               +8
//   int sessionInfoUpdate      +12
//   int sessionInfoLen         +16
//   int sessionInfoOffset      +20
//   int numVars                +24
//   int varHeaderOffset        +28
//   int numBuf                 +32
//   int bufLen                 +36
//   int pad1[2]                +40   ← 16-byte align
//   irsdk_varBuf varBuf[4]    +48   (4 × 16 = 64)
//                              =112  (header portion before varBuf = 48)
struct irsdk_header {
    int ver;                 // API version (currently 2)
    int status;              // bitfield using irsdk_StatusField
    int tickRate;            // ticks per second (60 or 360 etc.)

    // Session information, updated periodically
    int sessionInfoUpdate;   // incremented when session info changes
    int sessionInfoLen;      // length in bytes of session info string
    int sessionInfoOffset;   // offset to session info (YAML)

    // State data, output at tickRate
    int numVars;             // length of irsdk_varHeader array
    int varHeaderOffset;     // offset to irsdk_varHeader[numVars]

    int numBuf;              // <= IRSDK_MAX_BUFS (3 for now)
    int bufLen;              // length in bytes for one line
    int pad1[2];             // (16 byte align)

    irsdk_varBuf varBuf[IRSDK_MAX_BUFS];  // data buffers
};

#endif // IRSDK_DEFINES_H
