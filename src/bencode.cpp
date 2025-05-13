#include "common.hpp"
#include "oxenc/bt_serialize.h"
#include "oxenc/bt_value.h"
#include <pybind11/cast.h>
#include <pybind11/stl.h>
#include <stdexcept>

namespace pybind11::detail {

// Pybind type caster for bt_value that lets us load a bt_value from arbitrary Python data and vice
// versa; the caller provides whatever data and pybind takes care of all of the loading into the
// variant (or fails it something is provided anywhere in the data that can't be stuffed into a
// bt_value).
template<> struct type_caster<oxenc::bt_value> {
  public:
    PYBIND11_TYPE_CASTER(oxenc::bt_value, _("bencode_value"));

    // void pointer because we need type erasure to avoid instantiating pybind's
    // variant_caster<bt_variant> until we have instantiated this class (because variant_caster
    // instantiates casters of each variant type, and we are one of those types).
    std::shared_ptr<void> var_caster;

    bool load(handle src, bool conv);
    static handle cast(oxenc::bt_value src, return_value_policy policy, handle parent);
};

// Attempts to load a bt_value from a Python parameter:
using var_caster_t = variant_caster<oxenc::bt_variant>;
bool type_caster<oxenc::bt_value>::load(handle src, bool conv) {
    if (!var_caster)
        var_caster = std::make_shared<var_caster_t>();
    auto* vc = static_cast<var_caster_t*>(var_caster.get());

    if (!vc->load(src, conv))
        return false;
    value = std::move(*vc).operator oxenc::bt_variant&&();
    return true;
}

handle type_caster<oxenc::bt_value>::cast(oxenc::bt_value val, return_value_policy rvp, handle /*parent*/) {
    return std::visit(
            []<typename T>(T&& v) -> handle {
                if constexpr (std::same_as<T, std::string>)
                    return py::bytes{std::move(v)}.release();
                else if constexpr (std::same_as<T, std::string_view>)
                    return py::bytes{v.data(), v.size()}.release();
                else if constexpr (std::same_as<T, uint64_t> || std::same_as<T, int64_t>)
                    return py::int_{v};
                else if constexpr (std::same_as<T, oxenc::bt_list>) {
                    py::list l;
                    for (auto& item : v)
                        l.append(std::move(item));
                    return l.release();
                } else {
                    static_assert(std::same_as<T, oxenc::bt_dict>);
                    py::dict d;
                    for (auto& [key, value] : v)
                        d[py::bytes{key}] = std::move(value);
                    return d.release();
                }
            },
            std::move(val));
}

} // namespace pybind11::detail

namespace {

void delete_buffer(Py_buffer* b) {
    PyBuffer_Release(b);
    delete b;
}

}

namespace oxenc {

void BEncode_Init(py::module& m) {
    using namespace pybind11::literals;

    m.def("bt_serialize", [](bt_value val) { return py::bytes(bt_serialize(val)); },
            "val"_a,
            "Returns the bencode value of the given value.  The bt_value val can be given as a "
            "bytes, str, int, list of bt_values, or dict of bytes/str -> bt_value pairs.  Note "
            "that str values will be encoded as utf-8 but will be *decoded* by bt_deserialize as "
            "as bytes.");

    m.def("bt_deserialize", [](py::buffer val) {
            auto* b = new Py_buffer();
            if (PyObject_GetBuffer(val.ptr(), b, PyBUF_SIMPLE) != 0) {
                delete b;
                throw py::error_already_set();
            }
            std::unique_ptr<Py_buffer, decltype(&delete_buffer)> buf{b, &delete_buffer};
            std::string_view data{static_cast<const char*>(buf->buf), static_cast<size_t>(buf->len)};
            if (data.empty()) throw std::invalid_argument{"empty byte string is not a valid bencoded value"};
            return bt_deserialize<bt_value>(data);
        },
        "val"_a,
        "Deserializes a bencoded value from a buffer-supporting value (such as a bytes or "
        "memoryview).  Deserialization produces a value of: `int`, `bytes`, `list`, or `dict`; "
        "lists contain 0 or more of these values (recursively), and dicts contain bytes keys each "
        "containing one of these values (again recursive).  Note that you always get `bytes` out, "
        "not `str`s: it is up to the caller to decide how to interpret these values."
    );
}

} // namespace oxenc
