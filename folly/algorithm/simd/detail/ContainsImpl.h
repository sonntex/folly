/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <algorithm>
#include <cstring>
#include <cwchar>
#include <type_traits>

#include <folly/CPortability.h>
#include <folly/algorithm/simd/detail/SimdAnyOf.h>
#include <folly/algorithm/simd/detail/SimdCharPlatform.h>
#include <folly/container/span.h>

namespace folly::simd::detail {

/*
 * The functions in this file are FOLLY_ERASE to make sure
 * that the only place behind a call boundary is the explicit one.
 */

template <typename T>
FOLLY_ERASE bool containsImplStd(folly::span<const T> haystack, T needle) {
  static_assert(
      std::is_unsigned_v<T>, "we should only get here for uint8/16/32/64");
  if constexpr (sizeof(T) == 1) {
    auto* ptr = reinterpret_cast<const char*>(haystack.data());
    auto castNeedle = static_cast<char>(needle);
    if (haystack.empty()) { // memchr requires not null
      return false;
    }
    return std::memchr(ptr, castNeedle, haystack.size()) != nullptr;
  } else if constexpr (sizeof(T) == sizeof(wchar_t)) {
    auto* ptr = reinterpret_cast<const wchar_t*>(haystack.data());
    auto castNeedle = static_cast<wchar_t>(needle);
    if (haystack.empty()) { // wmemchr requires not null
      return false;
    }
    return std::wmemchr(ptr, castNeedle, haystack.size()) != nullptr;
  } else {
    // Using find instead of any_of on an off chance that the standard library
    // will add some custom vectorization.
    // That wouldn't be possible for any_of because of the predicates.
    return std::find(haystack.begin(), haystack.end(), needle) !=
        haystack.end();
  }
}

template <typename T>
constexpr bool hasHandwrittenContains() {
  return std::is_same_v<T, std::uint8_t> &&
      !std::is_same_v<SimdCharPlatform, void>;
}

template <typename T>
FOLLY_ERASE bool containsImplHandwritten(
    folly::span<const T> haystack, T needle) {
  static_assert(std::is_same_v<T, std::uint8_t>, "");
  auto as_chars = folly::reinterpret_span_cast<const char>(haystack);
  return simdAnyOf<SimdCharPlatform, 4>(
      as_chars.data(),
      as_chars.data() + as_chars.size(),
      [&](SimdCharPlatform::reg_t x) {
        return SimdCharPlatform::equal(x, static_cast<char>(needle));
      });
}

template <typename T>
FOLLY_ERASE bool containsImpl(folly::span<const T> haystack, T needle) {
  if constexpr (hasHandwrittenContains<T>()) {
    return containsImplHandwritten(haystack, needle);
  } else {
    return containsImplStd(haystack, needle);
  }
}

} // namespace folly::simd::detail
