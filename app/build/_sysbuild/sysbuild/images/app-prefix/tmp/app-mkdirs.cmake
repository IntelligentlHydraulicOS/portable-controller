# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/binbin/ncs-workspace/moving_controler/app"
  "/home/binbin/ncs-workspace/moving_controler/app/build/app"
  "/home/binbin/ncs-workspace/moving_controler/app/build/_sysbuild/sysbuild/images/app-prefix"
  "/home/binbin/ncs-workspace/moving_controler/app/build/_sysbuild/sysbuild/images/app-prefix/tmp"
  "/home/binbin/ncs-workspace/moving_controler/app/build/_sysbuild/sysbuild/images/app-prefix/src/app-stamp"
  "/home/binbin/ncs-workspace/moving_controler/app/build/_sysbuild/sysbuild/images/app-prefix/src"
  "/home/binbin/ncs-workspace/moving_controler/app/build/_sysbuild/sysbuild/images/app-prefix/src/app-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/binbin/ncs-workspace/moving_controler/app/build/_sysbuild/sysbuild/images/app-prefix/src/app-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/binbin/ncs-workspace/moving_controler/app/build/_sysbuild/sysbuild/images/app-prefix/src/app-stamp${cfgdir}") # cfgdir has leading slash
endif()
