# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/arjith/dev/northpole/payment_handler/build-debug/_deps/doctest-src")
  file(MAKE_DIRECTORY "/home/arjith/dev/northpole/payment_handler/build-debug/_deps/doctest-src")
endif()
file(MAKE_DIRECTORY
  "/home/arjith/dev/northpole/payment_handler/build-debug/_deps/doctest-build"
  "/home/arjith/dev/northpole/payment_handler/build-debug/_deps/doctest-subbuild/doctest-populate-prefix"
  "/home/arjith/dev/northpole/payment_handler/build-debug/_deps/doctest-subbuild/doctest-populate-prefix/tmp"
  "/home/arjith/dev/northpole/payment_handler/build-debug/_deps/doctest-subbuild/doctest-populate-prefix/src/doctest-populate-stamp"
  "/home/arjith/dev/northpole/payment_handler/build-debug/_deps/doctest-subbuild/doctest-populate-prefix/src"
  "/home/arjith/dev/northpole/payment_handler/build-debug/_deps/doctest-subbuild/doctest-populate-prefix/src/doctest-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/arjith/dev/northpole/payment_handler/build-debug/_deps/doctest-subbuild/doctest-populate-prefix/src/doctest-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/arjith/dev/northpole/payment_handler/build-debug/_deps/doctest-subbuild/doctest-populate-prefix/src/doctest-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
