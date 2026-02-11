#include "data/irsdk_manager.h"
#include <cstring>
#include <iostream>
#include <algorithm>  // std::max

namespace iracing {

IRSDKManager::IRSDKManager()
    : m_hMemMapFile(nullptr),
      m_hDataValidEvent(nullptr),
      m_pHeader(nullptr),
      m_pSharedMem(nullptr),
      m_connected(false),
      m_lastTickCount(-1),
      m_latestBufIndex(-1),
      m_sessionInfoUpdate(0)
{
}

IRSDKManager::~IRSDKManager() {
    shutdown();
}

bool IRSDKManager::startup() {
    if (m_connected && m_pHeader && (m_pHeader->status & irsdk_stConnected)) {
        return true;
    }

    if (m_connected) {
        std::cout << "iRacing session ended, reconnecting...\n";
        closeSharedMemory();
        m_connected = false;
        m_lastTickCount = -1;
        m_latestBufIndex = -1;
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
    m_lastTickCount = -1;
    m_latestBufIndex = -1;
}

bool IRSDKManager::isConnected() const {
    return m_connected && m_pHeader != nullptr;
}

bool IRSDKManager::isSessionActive() const {
    return m_connected && m_pHeader != nullptr && (m_pHeader->status & irsdk_stConnected);
}

bool IRSDKManager::openSharedMemory() {
    m_hMemMapFile = OpenFileMapping(FILE_MAP_READ, FALSE, IRSDK_MEMMAPFILENAME);
    if (!m_hMemMapFile) return false;

    m_pSharedMem = (const char*)MapViewOfFile(m_hMemMapFile, FILE_MAP_READ, 0, 0, 0);
    if (!m_pSharedMem) {
        CloseHandle(m_hMemMapFile);
        m_hMemMapFile = nullptr;
        return false;
    }

    m_pHeader = (const irsdk_header*)m_pSharedMem;
    if (!m_pHeader || m_pHeader->ver != 2) {
        UnmapViewOfFile(m_pSharedMem);
        CloseHandle(m_hMemMapFile);
        m_pSharedMem = nullptr;
        m_hMemMapFile = nullptr;
        m_pHeader = nullptr;
        return false;
    }

    // Try to open the event (optional – we have polling fallback)
    m_hDataValidEvent = OpenEvent(SYNCHRONIZE, FALSE, IRSDK_DATAVALIDEVENTNAME);
    // ↑ Failure is OK, we don't return false here anymore

    // Important: read initial buffer so we have data immediately on connect
    updateLatestBufferIndex();
    if (m_latestBufIndex >= 0) {
        m_lastTickCount = m_pHeader->varBuf[m_latestBufIndex].tickCount;
    }
    m_sessionInfoUpdate = m_pHeader->sessionInfoUpdate;

    std::cout << "iRacing memory map opened (event " 
              << (m_hDataValidEvent ? "OK" : "not available – using polling") << ")\n";

    return true;
}

void IRSDKManager::closeSharedMemory() {
    if (m_pSharedMem) UnmapViewOfFile(m_pSharedMem);
    if (m_hMemMapFile) CloseHandle(m_hMemMapFile);
    if (m_hDataValidEvent) CloseHandle(m_hDataValidEvent);

    m_pSharedMem = nullptr;
    m_hMemMapFile = nullptr;
    m_hDataValidEvent = nullptr;
    m_pHeader = nullptr;
}

bool IRSDKManager::waitForData(int timeoutMS) {
    if (!m_pHeader || !(m_pHeader->status & irsdk_stConnected)) {
        return false;
    }

    bool newData = false;

    // Prefer event when available (low CPU usage)
    if (m_hDataValidEvent) {
        DWORD result = WaitForSingleObject(m_hDataValidEvent, timeoutMS);
        if (result == WAIT_OBJECT_0) {
            newData = true;
        }
    }

    // Always double-check with tick count (makes it reliable like irdashies)
    int currentTick = getLatestTickCount();
    if (currentTick > m_lastTickCount || m_lastTickCount < 0) {
        newData = true;
    }

    if (newData) {
        updateLatestBufferIndex();
        m_lastTickCount = m_pHeader->varBuf[m_latestBufIndex].tickCount;
        // Optional: update session info version if changed
        if (m_pHeader->sessionInfoUpdate != m_sessionInfoUpdate) {
            m_sessionInfoUpdate = m_pHeader->sessionInfoUpdate;
        }
        return true;
    }

    return false;
}

void IRSDKManager::updateLatestBufferIndex() {
    m_latestBufIndex = 0;
    int maxTick = m_pHeader->varBuf[0].tickCount;

    for (int i = 1; i < m_pHeader->numBuf; ++i) {
        if (m_pHeader->varBuf[i].tickCount > maxTick) {
            maxTick = m_pHeader->varBuf[i].tickCount;
            m_latestBufIndex = i;
        }
    }
}

int IRSDKManager::getLatestTickCount() const {
    if (!m_pHeader) return -1;
    int maxTick = -1;
    for (int i = 0; i < m_pHeader->numBuf; ++i) {
        maxTick = std::max(maxTick, m_pHeader->varBuf[i].tickCount);
    }
    return maxTick;
}

const char* IRSDKManager::getDataPtr() const {
    if (!m_pHeader || m_latestBufIndex < 0) return nullptr;
    return m_pSharedMem + m_pHeader->varBuf[m_latestBufIndex].bufOffset;
}

const irsdk_varHeader* IRSDKManager::getVarHeader(const char* name) const {
    if (!m_pHeader) return nullptr;

    const auto* headers = reinterpret_cast<const irsdk_varHeader*>(
        m_pSharedMem + m_pHeader->varHeaderOffset);

    for (int i = 0; i < m_pHeader->numVars; ++i) {
        if (strcmp(headers[i].name, name) == 0) {
            return &headers[i];
        }
    }
    // Opcional: descomentar para debug
    // std::cerr << "Variable no encontrada: " << name << "\n";
    return nullptr;
}

float IRSDKManager::getFloat(const char* name, float defaultValue) const {
    const auto* header = getVarHeader(name);
    if (!header || (header->type != irsdk_float && header->type != irsdk_double)) {
        return defaultValue;
    }

    const char* data = getDataPtr();
    if (!data) return defaultValue;

    // Double-check: tick no cambió durante lectura (muy raro pero posible)
    int tickBefore = m_pHeader->varBuf[m_latestBufIndex].tickCount;
    float value = (header->type == irsdk_double)
        ? static_cast<float>(*(const double*)(data + header->offset))
        : *(const float*)(data + header->offset);
    int tickAfter = m_pHeader->varBuf[m_latestBufIndex].tickCount;

    return (tickBefore == tickAfter) ? value : defaultValue;
}

int IRSDKManager::getInt(const char* name, int defaultValue) const {
    const auto* header = getVarHeader(name);
    if (!header || (header->type != irsdk_int && header->type != irsdk_bool && header->type != irsdk_bitField)) {
        return defaultValue;
    }

    const char* data = getDataPtr();
    if (!data) return defaultValue;

    int tickBefore = m_pHeader->varBuf[m_latestBufIndex].tickCount;
    int value = *(const int*)(data + header->offset);
    int tickAfter = m_pHeader->varBuf[m_latestBufIndex].tickCount;

    return (tickBefore == tickAfter) ? value : defaultValue;
}

bool IRSDKManager::getBool(const char* name, bool defaultValue) const {
    return getInt(name, defaultValue ? 1 : 0) != 0;
}

const float* IRSDKManager::getFloatArray(const char* name, int& count) const {
    count = 0;
    const auto* header = getVarHeader(name);
    if (!header || header->type != irsdk_float) return nullptr;

    count = header->count;
    const char* data = getDataPtr();
    if (!data) {
        count = 0;
        return nullptr;
    }

    return reinterpret_cast<const float*>(data + header->offset);
}

const int* IRSDKManager::getIntArray(const char* name, int& count) const {
    count = 0;
    const auto* header = getVarHeader(name);
    if (!header || (header->type != irsdk_int && header->type != irsdk_bool && header->type != irsdk_bitField)) {
        return nullptr;
    }

    count = header->count;
    const char* data = getDataPtr();
    if (!data) {
        count = 0;
        return nullptr;
    }

    return reinterpret_cast<const int*>(data + header->offset);
}

const char* IRSDKManager::getSessionInfo() const {
    if (!m_pHeader) return nullptr;
    return m_pSharedMem + m_pHeader->sessionInfoOffset;
}

int IRSDKManager::getSessionInfoUpdate() const {
    return m_pHeader ? m_pHeader->sessionInfoUpdate : 0;
}

} // namespace iracing
