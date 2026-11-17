// xobf - a zero-dependency, header-only compile-time obfuscation library for C++.
//
// Umbrella header: include this to get every obfuscation primitive.
//
//   #include <xobf/xobf.hpp>
//
//   std::puts(XOBF_STR("secret message").decrypt());   // encrypted string
//   int port = XOBF_INT(8080);                          // encrypted constant
//   int s = xobf::mba::add(a, b);                       // MBA-substituted a+b
//   XOBF_BOGUS(do_work());                              // opaque-predicate guard
//
// Requires C++17. See README.md for details and the mapping to the research
// report's techniques.
#pragma once

#include "constant.hpp"
#include "inline.hpp"
#include "mba.hpp"
#include "predicate.hpp"
#include "random.hpp"
#include "string.hpp"

#define XOBF_VERSION_MAJOR 1
#define XOBF_VERSION_MINOR 0
#define XOBF_VERSION_PATCH 0
