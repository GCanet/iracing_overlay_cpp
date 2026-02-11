// irsdk_manager.h
#ifndef IRSDK_MANAGER_H
#define IRSDK_MANAGER_H

#include <windows.h>
#include <cstddef>
#include <cstdint>
#include "irsdk/irsdk_defines.h"

namespace iracing {

class IRSDKManager {
public:
    IRSDKManager();
    ~IRSDKManager();

    // Connection
    bool startup();
    void shutdown();
    bool isConnected() const { return m_connected; }
    bool isSessionActive() const;

    // Data access
    bool waitForData(int timeoutMS = 16);

    // Get values
    template <typename T>
    T getVar(const char* name, T defaultValue = T());

    float getFloat(const char* name, float defaultValue = 0.0f);
    int getInt(const char* name, int defaultValue = 0);
    bool getBool(const char* name, bool defaultValue = false);

    // Array access
    const float* getFloatArray(const char* name, int& count);
    const int* getIntArray(const char* name, int& count);

    // Session info
    const char* getSessionInfo();
    int getSessionInfoUpdate() const;

private:
    bool openSharedMemory();
    void closeSharedMemory();
    const irsdk_varHeader* getVarHeader(const char* name);
    void updateLatestBufferIndex();
    int getLatestTickCount() const;
    const char* getDataPtr() const;

    HANDLE m_hMemMapFile = nullptr;
    HANDLE m_hDataValidEvent = nullptr;
    const irsdk_header* m_pHeader = nullptr;
    const char* m_pSharedMem = nullptr;
    bool m_connected = false;
    int m_lastTickCount = -1;
    int m_latestBufIndex = -1;
    int m_sessionInfoUpdate = 0;
};

} // namespace iracing

#endif // IRSDK_MANAGER_H
