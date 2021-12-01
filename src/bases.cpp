#include "common.hpp"
#include <oxenc/base32z.h>
#include <oxenc/base64.h>
/*
#include <pybind11/attr.h>
#include <pybind11/chrono.h>
#include <pybind11/detail/common.h>
#include <pybind11/functional.h>
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>
#include <pybind11/stl.h>
*/

namespace oxenc {

void
Bases_Init(py::module& m)
{
    using namespace pybind11::literals;

    m.def("to_base32z", [](py::bytes b) {
            char* buffer;
            ssize_t len;
            if (PYBIND11_BYTES_AS_STRING_AND_SIZE(b.ptr(), &buffer, &len))
                throw std::runtime_error{"Unable to extract bytes contents!"};

            return to_base32z(buffer, buffer + len);
        },
        "b"_a,
        "Encodes a byte string as base32z."
    );

    m.def("from_base32z", [](std::string_view b32z, bool check) {
            if (check && !is_base32z(b32z))
                throw std::invalid_argument{"Value is not base32z encoded"};

            return from_base32z(b32z);
        },
        "b32z"_a, "check"_a = true,
        "Decodes a base32z-encoded string to a byte string.  Raises a ValueError if the string is "
        "not base32z encoded; the check can be skipped by specifying check=False if the content "
        "already known to be a valid base32z string."
    );

    m.def("is_base32z", [](std::string_view b32z) { return is_base32z(b32z); },
        "b32z"_a,
        "Returns True if the string is a valid base32z encoded string, false otherwise."
    );

    m.def("to_base64", [](py::bytes b, bool padded) {
            char* buffer;
            ssize_t len;
            if (PYBIND11_BYTES_AS_STRING_AND_SIZE(b.ptr(), &buffer, &len))
                throw std::runtime_error{"Unable to extract bytes contents!"};

            return to_base64(buffer, buffer + len);
        },
        "b"_a, "padded"_a = true,
        "Encodes a byte string as base64, optionally omitting the trailing '=' padding, if present"
    );

    m.def("from_base64", [](std::string_view b64, bool check) {
            if (check && !is_base64(b64))
                throw std::invalid_argument{"Value is not base64 encoded"};

            return from_base64(b64);
        },
        "b64"_a, "check"_a = true,
        "Decodes a base64-encoded string to a byte string, with or without padding.  Raises a "
        "ValueError if the string is not base64 encoded; the check can be skipped by specifying "
        "check=False if the content already known to be a valid base64 string.\n\nUnlike base64, "
        "this does not accept and skip random garbage in the middle of the base64 data, does not "
        "that padding be present, and only accepts valid padding (instead of arbitrary amounts of "
        "invalid padding)."
    );

    m.def("is_base64", [](std::string_view b64) { return is_base64(b64); },
        "b64"_a,
        "Returns True if the string is a valid base64 encoded string, false otherwise."
    );
}

} // namespace oxenc
