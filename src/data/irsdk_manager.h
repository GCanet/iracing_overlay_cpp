#ifndef IRSDK_MANAGER_H
#define IRSDK_MANAGER_H

#include <Windows.h>
#include <string>
#include <vector>
#include <memory>
#include "irsdk/irsdk_defines.h"   // asumiendo que tienes este header con las definiciones oficiales

namespace iracing {

class IRSDKManager {
public:
    IRSDKManager();
    ~IRSDKManager();

    // Connection
    bool startup();
    void shutdown();
    bool isConnected() const;
    bool isSessionActive() const;

    // Data wait & update
    bool waitForData(int timeoutMS = 16);

    // Get values (const donde posible)
    float getFloat(const char* name, float defaultValue = 0.0f) const;
    int getInt(const char* name, int defaultValue = 0) const;
    bool getBool(const char* name, bool defaultValue = false) const;

    // Array access
    const float* getFloatArray(const char* name, int& count) const;
    const int* getIntArray(const char* name, int& count) const;

    // Session info
    const char* getSessionInfo() const;
    int getSessionInfoUpdate() const;

private:
    bool openSharedMemory();
    void closeSharedMemory();
    void updateLatestBufferIndex();          // ← nuevo método
    int getLatestTickCount() const;          // ← helper
    const char* getDataPtr() const;          // ← puntero al buffer latest

    const irsdk_varHeader* getVarHeader(const char* name) const;

    HANDLE m_hMemMapFile = nullptr;
    HANDLE m_hDataValidEvent = nullptr;
    const irsdk_header* m_pHeader = nullptr;
    const char* m_pSharedMem = nullptr;
    bool m_connected = false;
    int m_lastTickCount = -1;
    int m_latestBufIndex = -1;               // ← NUEVO: cache del buffer más reciente
    int m_sessionInfoUpdate = 0;
};

} // namespace iracing

#endif // IRSDK_MANAGER_H
