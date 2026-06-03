# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# Warning policy (architecture §8.6.5: `/W4 /WX`, warnings as errors).
#
# Populates the `vox_project_warnings` interface target. Link it into first-party
# targets only — third-party code (vcpkg, GoogleTest) must not inherit `/WX`.

option(VOX_WARNINGS_AS_ERRORS "Treat compiler warnings as errors." ON)

if(MSVC)
  set(_vox_warnings
    /W4
    /w14242  # 'identifier': conversion, possible loss of data
    /w14254  # 'operator': conversion, possible loss of data
    /w14263  # member function does not override any base class virtual
    /w14265  # class has virtual functions but non-virtual destructor
    /w14287  # unsigned/negative constant mismatch
    /w14296  # expression is always true/false
    /w14311  # pointer truncation
    /w14545  # expression before comma has no effect
    /w14546  # function call before comma missing argument list
    /w14547  # operator before comma has no effect
    /w14549  # operator before comma has no effect
    /w14555  # expression has no effect
    /w14619  # pragma warning: there is no warning number
    /w14640  # construction of local static object is not thread-safe
    /w14826  # conversion is sign-extended
    /w14905  # wide string literal cast to 'LPSTR'
    /w14906  # string literal cast to 'LPWSTR'
    /w14928) # illegal copy-initialization
  if(VOX_WARNINGS_AS_ERRORS)
    list(APPEND _vox_warnings /WX)
  endif()
else()
  set(_vox_warnings
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Wcast-align
    -Wunused
    -Woverloaded-virtual
    -Wconversion
    -Wsign-conversion
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
    -Wimplicit-fallthrough)
  if(VOX_WARNINGS_AS_ERRORS)
    list(APPEND _vox_warnings -Werror)
  endif()
endif()

target_compile_options(vox_project_warnings INTERFACE ${_vox_warnings})
