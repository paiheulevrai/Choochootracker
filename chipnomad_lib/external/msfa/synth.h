/*
 * Copyright 2012 Google Inc.
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
// Modified for ChooChooTracker: namespace isolation and standalone scalar adapter.
// See PATCHES.md for the complete changes and dependency boundary.


#pragma once
#include <cstdint>
namespace choochoo_msfa {
constexpr int LG_N = 6;
constexpr int N = 1 << LG_N;
template<class T> inline T min(T a,T b) { return a < b ? a : b; }
template<class T> inline T max(T a,T b) { return a > b ? a : b; }
}
