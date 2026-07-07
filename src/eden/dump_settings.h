#include <iostream>
#include <fstream>
#include "common/settings.h"

inline void DumpSettingsInfo(const char* context) {
    std::ofstream df("debug_log.txt", std::ios::app);
    df << context << ": sizeof(Values)=" << sizeof(Settings::Values)
       << ", offsetof(external_content_dirs)=" << offsetof(Settings::Values, external_content_dirs)
       << ", addr=" << (void*)&Settings::values << "\n";
    df.flush();
}
