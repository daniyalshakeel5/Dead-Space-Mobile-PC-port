def _jni_names():
    n = ["reserved0", "reserved1", "reserved2", "reserved3", "GetVersion", "DefineClass",
         "FindClass", "FromReflectedMethod", "FromReflectedField", "ToReflectedMethod",
         "GetSuperclass", "IsAssignableFrom", "ToReflectedField", "Throw", "ThrowNew",
         "ExceptionOccurred", "ExceptionDescribe", "ExceptionClear", "FatalError",
         "PushLocalFrame", "PopLocalFrame", "NewGlobalRef", "DeleteGlobalRef",
         "DeleteLocalRef", "IsSameObject", "NewLocalRef", "EnsureLocalCapacity",
         "AllocObject", "NewObject", "NewObjectV", "NewObjectA", "GetObjectClass",
         "IsInstanceOf", "GetMethodID"]
    types = ["Object", "Boolean", "Byte", "Char", "Short", "Int", "Long", "Float", "Double", "Void"]
    for t in types: n += [f"Call{t}Method", f"Call{t}MethodV", f"Call{t}MethodA"]
    for t in types: n += [f"CallNonvirtual{t}Method", f"CallNonvirtual{t}MethodV", f"CallNonvirtual{t}MethodA"]
    n += ["GetFieldID"]
    ft = types[:-1]
    n += [f"Get{t}Field" for t in ft] + [f"Set{t}Field" for t in ft]
    n += ["GetStaticMethodID"]
    for t in types: n += [f"CallStatic{t}Method", f"CallStatic{t}MethodV", f"CallStatic{t}MethodA"]
    n += ["GetStaticFieldID"]
    n += [f"GetStatic{t}Field" for t in ft] + [f"SetStatic{t}Field" for t in ft]
    n += ["NewString", "GetStringLength", "GetStringChars", "ReleaseStringChars", "NewStringUTF",
          "GetStringUTFLength", "GetStringUTFChars", "ReleaseStringUTFChars", "GetArrayLength",
          "NewObjectArray", "GetObjectArrayElement", "SetObjectArrayElement"]
    pt = types[1:-1]
    n += [f"New{t}Array" for t in pt] + [f"Get{t}ArrayElements" for t in pt]
    n += [f"Release{t}ArrayElements" for t in pt] + [f"Get{t}ArrayRegion" for t in pt]
    n += [f"Set{t}ArrayRegion" for t in pt]
    n += ["RegisterNatives", "UnregisterNatives", "MonitorEnter", "MonitorExit", "GetJavaVM",
          "GetStringRegion", "GetStringUTFRegion", "GetPrimitiveArrayCritical",
          "ReleasePrimitiveArrayCritical", "GetStringCritical", "ReleaseStringCritical",
          "NewWeakGlobalRef", "DeleteWeakGlobalRef", "ExceptionCheck", "NewDirectByteBuffer",
          "GetDirectBufferAddress", "GetDirectBufferCapacity", "GetObjectRefType"]
    return n
JNI_NAMES = _jni_names()
