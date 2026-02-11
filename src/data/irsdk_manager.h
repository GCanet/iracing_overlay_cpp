#ifndef IRSDK_MANAGER_H
#define IRSDK_MANAGER_H

#include <windows.h>
#include "irsdk/irsdk_defines.h"

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
    
    // Data access
    bool waitForData(int timeoutMS = 16);
    
    // Get values
    float getFloat(const char* name, float defaultValue = 0.0f) const;
    int getInt(const char* name, int defaultValue = 0) const;
    bool getBool(const char* name, bool defaultValue = false) const;
    
    // Array access
    const float* getFloatArray(const char* name, int& count) const;
    const int* getIntArray(const char* name, int& count) const;
    
    // Session info
    const char* getSessionInfo() const;
    int getSessionInfoUpdate() const;

    // Generic template (kept for future use)
    template<typename T>
    T getVar(const char* name, T defaultValue = T());

private:
    bool openSharedMemory();
    void closeSharedMemory();
    void updateLatestBufferIndex();
    int getLatestTickCount() const;
    const char* getDataPtr() const;
    const irsdk_varHeader* getVarHeader(const char* name) const;
    
    HANDLE m_hMemMapFile;
    HANDLE m_hDataValidEvent;
    const irsdk_header* m_pHeader;
    const char* m_pSharedMem;
    bool m_connected;
    int m_lastTickCount;
    int m_latestBufIndex;
    int m_sessionInfoUpdate;
};

} // namespace iracing

#endif // IRSDK_MANAGER_H
