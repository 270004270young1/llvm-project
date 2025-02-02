# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/youngminghei/Documents/Fyp/llvm/llvm-project/llvm/../runtimes"
  "/home/youngminghei/Documents/Fyp/llvm/llvm-project/compiler-rt/build/lib/fuzzer/libcxx_fuzzer_x86_64/build"
  "/home/youngminghei/Documents/Fyp/llvm/llvm-project/compiler-rt/build/lib/fuzzer/libcxx_fuzzer_x86_64"
  "/home/youngminghei/Documents/Fyp/llvm/llvm-project/compiler-rt/build/lib/fuzzer/libcxx_fuzzer_x86_64/tmp"
  "/home/youngminghei/Documents/Fyp/llvm/llvm-project/compiler-rt/build/lib/fuzzer/libcxx_fuzzer_x86_64/src/libcxx_fuzzer_x86_64-stamp"
  "/home/youngminghei/Documents/Fyp/llvm/llvm-project/compiler-rt/build/lib/fuzzer/libcxx_fuzzer_x86_64/src"
  "/home/youngminghei/Documents/Fyp/llvm/llvm-project/compiler-rt/build/lib/fuzzer/libcxx_fuzzer_x86_64/src/libcxx_fuzzer_x86_64-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/youngminghei/Documents/Fyp/llvm/llvm-project/compiler-rt/build/lib/fuzzer/libcxx_fuzzer_x86_64/src/libcxx_fuzzer_x86_64-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/youngminghei/Documents/Fyp/llvm/llvm-project/compiler-rt/build/lib/fuzzer/libcxx_fuzzer_x86_64/src/libcxx_fuzzer_x86_64-stamp${cfgdir}") # cfgdir has leading slash
endif()
