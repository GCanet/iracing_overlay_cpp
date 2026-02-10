#ifndef IRSDK_MANAGER_H
#define IRSDK_MANAGER_H

#include <Windows.h>
#include <string>
#include <vector>
#include <memory>
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
    
    // Data access
    bool waitForData(int timeoutMS = 16);
    
    // Get values
    template<typename T>
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
    
    HANDLE m_hMemMapFile;
    HANDLE m_hDataValidEvent;
    const irsdk_header* m_pHeader;
    const char* m_pSharedMem;
    bool m_connected;
    int m_lastTickCount;
    int m_sessionInfoUpdate;
};

} // namespace iracing

#endif // IRSDK_MANAGER_H
