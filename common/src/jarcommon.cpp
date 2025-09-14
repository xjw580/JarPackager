/**************************************************************************

Author:肖嘉威

Version:1.0.0

Date:2025/9/13 17:20

Description:

**************************************************************************/


#include <jni.h>
#include "jarcommon.h"
import std;

static std::unordered_map<std::string, unsigned int> getJavaVersionMap() {
    std::unordered_map<std::string, unsigned int> javaVersionMap;
    javaVersionMap["1.1"] = JNI_VERSION_1_1;
    javaVersionMap["1.2"] = JNI_VERSION_1_2;
    javaVersionMap["1.4"] = JNI_VERSION_1_4;
    javaVersionMap["1.6"] = JNI_VERSION_1_6;
    javaVersionMap["1.8"] = JNI_VERSION_1_8;
    javaVersionMap["9"] = JNI_VERSION_9;
    javaVersionMap["10"] = JNI_VERSION_10;
    javaVersionMap["19"] = JNI_VERSION_19;
    javaVersionMap["20"] = JNI_VERSION_20;
    javaVersionMap["21"] = JNI_VERSION_21;
    return javaVersionMap;
}

namespace JarCommon {

    const std::unordered_map<std::string, unsigned int> JAVA_VERSION_MAP = getJavaVersionMap();

} // namespace JarCommon