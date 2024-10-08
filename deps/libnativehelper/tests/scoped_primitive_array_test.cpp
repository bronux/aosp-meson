/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "nativehelper/scoped_primitive_array.h"

// This is a test that scoped headers work independently.

void TestScopedPrimitiveArrayRO(JNIEnv* env, jbooleanArray array) {
    ScopedBooleanArrayRO sba(env, array);
    sba.reset(nullptr);
    sba.get();
    sba.size();
}

void TestCompilationRW(JNIEnv* env, jintArray array) {
    ScopedIntArrayRW sba(env, array);
    sba.reset(nullptr);
    sba.get();
    sba.size();
    sba[3] = 3;
}

void TestCompilationCriticalRO(JNIEnv* env, jfloatArray array) {
    ScopedFloatCriticalArrayRO sfa(env, array);
    sfa.reset(nullptr);
    sfa.get();
    sfa.size();
}

void TestCompilationCriticalRW(JNIEnv* env, jdoubleArray array) {
    ScopedDoubleCriticalArrayRW sda(env, array);
    sda.reset(nullptr);
    sda.get();
    sda.size();
    sda[3] = 3.0;
}
