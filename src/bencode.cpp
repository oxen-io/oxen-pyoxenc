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

handle type_caster<oxenc::bt_value>::cast(oxenc::bt_value val, return_value_policy rvp, handle parent) {
    if (auto* str = std::get_if<std::string>(&val))
        return py::bytes{*str}.release();
    if (auto* sv = std::get_if<std::string_view>(&val))
        return py::bytes{sv->data(), sv->size()}.release();
    if (auto* u64 = std::get_if<uint64_t>(&val))
        return py::int_{*u64}.release();
    if (auto* list = std::get_if<oxenc::bt_list>(&val)) {
        py::list l;
        for (auto& item : *list)
            l.append(cast(std::move(item), rvp, parent));
        return l.release();
    }
    if (auto* dict = std::get_if<oxenc::bt_dict>(&val)) {
        py::dict d;
        for (auto& [key, value] : *dict)
            d[py::bytes{key}] = cast(std::move(value), rvp, parent);
        return d.release();
    }
    return py::none{}.release();
}

} // namespace pybind11::detail


namespace oxenc {

void BEncode_Init(py::module& m) {
    using namespace pybind11::literals;

    m.def("bt_serialize", [](bt_value val) { return py::bytes(bt_serialize(val)); },
            "val"_a,
            "Returns the bencode value of the given value.  The bt_value val can be given as a "
            "bytes, str, int, list of bt_values, or dict of bytes/str -> bt_value pairs.  Note "
            "that str values will be encoded as utf-8 but will be *decoded* by bt_deserialize as "
            "as bytes.");

    m.def("bt_deserialize", [](py::bytes val) {
            char* buffer;
            ssize_t len;
            if (PYBIND11_BYTES_AS_STRING_AND_SIZE(val.ptr(), &buffer, &len))
                throw std::runtime_error{"Unable to extract bytes contents!"};
            std::string_view data{buffer, static_cast<size_t>(len)};
            if (data.empty()) throw std::invalid_argument{"empty byte string is not a valid bencoded value"};
            return bt_deserialize<bt_value>(data);
        },
        "val"_a,
        "Deserializes a bencoded value.  Deserialization produces a value of: `int`, `bytes`, `list`, "
        "or `dict`; lists contain 0 or more of these values (recursively), and dicts contain bytes "
        "keys each containing one of these values (again recursive).  Note that you always get "
        "`bytes` out, not `str`s: it is up to the caller to decide how to interpret these values."
    );
}

} // namespace oxenc
