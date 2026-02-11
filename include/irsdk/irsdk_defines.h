#ifndef IRSDK_DEFINES_H
#define IRSDK_DEFINES_H

// iRacing SDK Constants
#define IRSDK_MAX_BUFS 4
#define IRSDK_MAX_STRING 32
#define IRSDK_MAX_DESC 64

// Shared memory names
#define IRSDK_MEMMAPFILENAME "Local\\IRSDKMemMapFileName"
#define IRSDK_DATAVALIDEVENTNAME "Local\\IRSDKDataValidEvent"

// Session string max length
#define IRSDK_UNLIMITED_LAPS 32767
#define IRSDK_UNLIMITED_TIME 604800.0f

// Status flags
enum irsdk_StatusField {
    irsdk_stConnected = 1
};

// Variable types
enum irsdk_VarType {
    irsdk_char = 0,
    irsdk_bool = 1,
    irsdk_int = 2,
    irsdk_bitField = 3,
    irsdk_float = 4,
    irsdk_double = 5
};

// Variable header
struct irsdk_varHeader {
    int type;
    int offset;
    int count;
    char name[IRSDK_MAX_STRING];
    char desc[IRSDK_MAX_DESC];
    char unit[IRSDK_MAX_STRING];
};

// Data buffer header
struct irsdk_varBuf {
    int tickCount;
    int bufOffset;
};

// Main header
struct irsdk_header {
    int ver;
    int status;
    int tickRate;
    int sessionInfoUpdate;
    int sessionInfoLen;
    int sessionInfoOffset;
    int numVars;
    int varHeaderOffset;
    int numBuf;
    int bufLen;
    irsdk_varBuf varBuf[IRSDK_MAX_BUFS];
};

#endif // IRSDK_DEFINES_H
