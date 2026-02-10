#include "data/irsdk_manager.h"
#include <cstring>
#include <iostream>

namespace iracing {

IRSDKManager::IRSDKManager()
    : m_hMemMapFile(nullptr)
    , m_hDataValidEvent(nullptr)
    , m_pHeader(nullptr)
    , m_pSharedMem(nullptr)
    , m_connected(false)
    , m_lastTickCount(0)
    , m_sessionInfoUpdate(0)
{
}

IRSDKManager::~IRSDKManager() {
    shutdown();
}

bool IRSDKManager::startup() {
    if (m_connected) {
        // Already have shared memory open - check if iRacing is still there
        if (m_pHeader && (m_pHeader->status & irsdk_stConnected)) {
            return true;  // Still connected and active
        }
        // Shared memory open but iRacing disconnected (session ended, sim closed)
        // Close everything and try to reconnect
        std::cout << "iRacing session ended, reconnecting..." << std::endl;
        closeSharedMemory();
        m_connected = false;
        m_lastTickCount = 0;
    }
    
    if (openSharedMemory()) {
        m_connected = true;
        return true;
    }
    
    return false;
}

void IRSDKManager::shutdown() {
    closeSharedMemory();
    m_connected = false;
}

bool IRSDKManager::isConnected() const {
    return m_connected && m_pHeader != nullptr;
}

bool IRSDKManager::isSessionActive() const {
    // Both conditions must be true:
    // 1) We have the shared memory open (m_connected + valid header)
    // 2) iRacing reports the session as connected via status flag
    return m_connected && m_pHeader != nullptr 
        && (m_pHeader->status & irsdk_stConnected);
}

bool IRSDKManager::openSharedMemory() {
    // Open memory mapped file
    m_hMemMapFile = OpenFileMapping(
        FILE_MAP_READ,
        FALSE,
        IRSDK_MEMMAPFILENAME
    );
    
    if (!m_hMemMapFile) {
        // Not an error - iRacing just isn't running yet
        return false;
    }
    
    // Map view
    m_pSharedMem = (const char*)MapViewOfFile(
        m_hMemMapFile,
        FILE_MAP_READ,
        0, 0, 0
    );
    
    if (!m_pSharedMem) {
        std::cerr << "Failed to map view of iRacing shared memory" << std::endl;
        CloseHandle(m_hMemMapFile);
        m_hMemMapFile = nullptr;
        return false;
    }
    
    m_pHeader = (const irsdk_header*)m_pSharedMem;
    
    // Verify header pointer
    if (!m_pHeader) {
        std::cerr << "Invalid header pointer" << std::endl;
        closeSharedMemory();
        return false;
    }
    
    // Version 0 means iRacing has the shared memory file open but hasn't
    // initialized it yet (e.g. still loading, no session active).
    // Don't treat this as an error - just release and retry next time.
    if (m_pHeader->ver != 2) {
        // Clean up properly to avoid memory leak
        UnmapViewOfFile(m_pSharedMem);
        m_pSharedMem = nullptr;
        m_pHeader = nullptr;
        CloseHandle(m_hMemMapFile);
        m_hMemMapFile = nullptr;
        return false;
    }
    
    // Open data valid event
    m_hDataValidEvent = OpenEvent(
        SYNCHRONIZE,
        FALSE,
        IRSDK_DATAVALIDEVENTNAME
    );
    
    if (!m_hDataValidEvent) {
        std::cerr << "Failed to open data valid event (non-fatal, will poll shared memory)" << std::endl;
    }
    
    std::cout << "iRacing SDK opened successfully" << std::endl;
    std::cout << "   Version: " << m_pHeader->ver << std::endl;
    std::cout << "   Tick rate: " << m_pHeader->tickRate << " Hz" << std::endl;
    std::cout << "   Num variables: " << m_pHeader->numVars << std::endl;
    std::cout << "   Num buffers: " << m_pHeader->numBuf << std::endl;
    
    return true;
}

void IRSDKManager::closeSharedMemory() {
    if (m_pSharedMem) {
        UnmapViewOfFile(m_pSharedMem);
        m_pSharedMem = nullptr;
        m_pHeader = nullptr;
    }
    
    if (m_hMemMapFile) {
        CloseHandle(m_hMemMapFile);
        m_hMemMapFile = nullptr;
    }
    
    if (m_hDataValidEvent) {
        CloseHandle(m_hDataValidEvent);
        m_hDataValidEvent = nullptr;
    }
}

bool IRSDKManager::waitForData(int timeoutMS) {
    if (!m_connected || !m_pHeader) return false;
    
    // Check if session is still active
    if (!(m_pHeader->status & irsdk_stConnected)) {
        return false;
    }
    
    // If we have the event handle, use it for efficient waiting
    if (m_hDataValidEvent) {
        DWORD result = WaitForSingleObject(m_hDataValidEvent, timeoutMS);
        
        if (result == WAIT_OBJECT_0) {
            if (m_pHeader->status & irsdk_stConnected) {
                int latestBuf = 0;
                int latestTick = 0;
                
                for (int i = 0; i < m_pHeader->numBuf; i++) {
                    if (m_pHeader->varBuf[i].tickCount > latestTick) {
                        latestTick = m_pHeader->varBuf[i].tickCount;
                        latestBuf = i;
                    }
                }
                
                if (latestTick != m_lastTickCount) {
                    m_lastTickCount = latestTick;
                    return true;
                }
            }
        }
    } else {
        // No event handle - poll shared memory directly
        if (m_pHeader->status & irsdk_stConnected) {
            int latestTick = 0;
            for (int i = 0; i < m_pHeader->numBuf; i++) {
                if (m_pHeader->varBuf[i].tickCount > latestTick) {
                    latestTick = m_pHeader->varBuf[i].tickCount;
                }
            }
            
            if (latestTick != m_lastTickCount) {
                m_lastTickCount = latestTick;
                return true;
            }
        }
    }
    
    return false;
}

const irsdk_varHeader* IRSDKManager::getVarHeader(const char* name) {
    if (!m_pHeader) return nullptr;
    
    const irsdk_varHeader* pVarHeader = 
        (const irsdk_varHeader*)(m_pSharedMem + m_pHeader->varHeaderOffset);
    
    for (int i = 0; i < m_pHeader->numVars; i++) {
        if (strcmp(pVarHeader[i].name, name) == 0) {
            return &pVarHeader[i];
        }
    }
    
    return nullptr;
}

float IRSDKManager::getFloat(const char* name, float defaultValue) {
    const irsdk_varHeader* header = getVarHeader(name);
    if (!header) return defaultValue;
    
    // Accept float and double types
    if (header->type != irsdk_float && header->type != irsdk_double) return defaultValue;
    
    // Get latest buffer
    int latestBuf = 0;
    for (int i = 1; i < m_pHeader->numBuf; i++) {
        if (m_pHeader->varBuf[i].tickCount > m_pHeader->varBuf[latestBuf].tickCount) {
            latestBuf = i;
        }
    }
    
    const char* data = m_pSharedMem + m_pHeader->varBuf[latestBuf].bufOffset;
    
    if (header->type == irsdk_double) {
        return static_cast<float>(*(const double*)(data + header->offset));
    }
    return *(const float*)(data + header->offset);
}

int IRSDKManager::getInt(const char* name, int defaultValue) {
    const irsdk_varHeader* header = getVarHeader(name);
    if (!header) return defaultValue;
    
    // Accept int, bool, and bitField types (all stored as 32-bit integers)
    if (header->type != irsdk_int && header->type != irsdk_bool && header->type != irsdk_bitField) {
        return defaultValue;
    }
    
    int latestBuf = 0;
    for (int i = 1; i < m_pHeader->numBuf; i++) {
        if (m_pHeader->varBuf[i].tickCount > m_pHeader->varBuf[latestBuf].tickCount) {
            latestBuf = i;
        }
    }
    
    const char* data = m_pSharedMem + m_pHeader->varBuf[latestBuf].bufOffset;
    return *(const int*)(data + header->offset);
}

bool IRSDKManager::getBool(const char* name, bool defaultValue) {
    return getInt(name, defaultValue ? 1 : 0) != 0;
}

const float* IRSDKManager::getFloatArray(const char* name, int& count) {
    const irsdk_varHeader* header = getVarHeader(name);
    if (!header || header->type != irsdk_float) {
        count = 0;
        return nullptr;
    }
    
    count = header->count;
    
    int latestBuf = 0;
    for (int i = 1; i < m_pHeader->numBuf; i++) {
        if (m_pHeader->varBuf[i].tickCount > m_pHeader->varBuf[latestBuf].tickCount) {
            latestBuf = i;
        }
    }
    
    const char* data = m_pSharedMem + m_pHeader->varBuf[latestBuf].bufOffset;
    return (const float*)(data + header->offset);
}

const int* IRSDKManager::getIntArray(const char* name, int& count) {
    const irsdk_varHeader* header = getVarHeader(name);
    if (!header || (header->type != irsdk_int && header->type != irsdk_bool && header->type != irsdk_bitField)) {
        count = 0;
        return nullptr;
    }
    
    count = header->count;
    
    int latestBuf = 0;
    for (int i = 1; i < m_pHeader->numBuf; i++) {
        if (m_pHeader->varBuf[i].tickCount > m_pHeader->varBuf[latestBuf].tickCount) {
            latestBuf = i;
        }
    }
    
    const char* data = m_pSharedMem + m_pHeader->varBuf[latestBuf].bufOffset;
    return (const int*)(data + header->offset);
}

const char* IRSDKManager::getSessionInfo() {
    if (!m_pHeader || !m_pSharedMem) return nullptr;
    
    return m_pSharedMem + m_pHeader->sessionInfoOffset;
}

int IRSDKManager::getSessionInfoUpdate() const {
    if (!m_pHeader) return 0;
    return m_pHeader->sessionInfoUpdate;
}

} // namespace iracing
