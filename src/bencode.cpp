#include "common.hpp"
#include "oxenc/bt_serialize.h"
#include "oxenc/bt_value.h"
#include <pybind11/cast.h>
#include <pybind11/stl.h>
#include <stdexcept>

namespace pybind11::detail {
    template<> struct type_caster<oxenc::bt_value> {
    public:
        PYBIND11_TYPE_CASTER(oxenc::bt_value, _("bencode_value"));

        // void pointer because we need type erasure to avoid instantiating pybind's
        // variant_caster<bt_variant> until we have instantiated this class, because we are one of the
        // variants that variant_caster internally instantiates.
        std::shared_ptr<void> var_caster;

        bool load(handle src, bool conv);
        static handle cast(oxenc::bt_value src, return_value_policy policy, handle parent);
    };

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

    handle type_caster<oxenc::bt_value>::cast(oxenc::bt_value, return_value_policy, handle) {
        throw std::logic_error{"Python casting of bt_value not supported"};
    }
}

namespace oxenc {

void build_list(py::list& l, bt_list_consumer c);

void build_dict(py::dict& d, bt_dict_consumer c) {
    while (!c.is_finished()) {
        auto key_sv = c.key();
        py::bytes key{key_sv.data(), key_sv.size()};
        if (c.is_string()) {
            auto s = c.consume_string_view();
            d[key] = py::bytes{s.data(), s.size()};
        } else if (c.is_negative_integer())
            d[key] = py::int_{c.consume_integer<int64_t>()};
        else if (c.is_integer())
            d[key] = py::int_{c.consume_integer<uint64_t>()};
        else if (c.is_dict()) {
            py::dict subdict{};
            d[key] = subdict;
            build_dict(subdict, c.consume_dict_consumer());
        } else if (c.is_list()) {
            py::list sublist{};
            d[key] = sublist;
            build_list(sublist, c.consume_list_consumer());
        } else {
            throw std::invalid_argument{"Invalid encoding inside dict"};
        }
    }
}

void build_list(py::list& l, bt_list_consumer c) {
    while (!c.is_finished()) {
        if (c.is_string()) {
            auto s = c.consume_string_view();
            l.append(py::bytes{s.data(), s.size()});
        } else if (c.is_negative_integer())
            l.append(py::int_{c.consume_integer<int64_t>()});
        else if (c.is_integer())
            l.append(py::int_{c.consume_integer<uint64_t>()});
        else if (c.is_dict()) {
            py::dict subdict{};
            l.append(subdict);
            build_dict(subdict, c.consume_dict_consumer());
        } else if (c.is_list()) {
            py::list sublist{};
            l.append(sublist);
            build_list(sublist, c.consume_list_consumer());
        } else {
            throw std::invalid_argument{"Invalid encoding inside list"};
        }
    }
}

void BEncode_Init(py::module& m) {
    using namespace pybind11::literals;

    m.def("bt_serialize", [](bt_value val) { return py::bytes(bt_serialize(val)); },
            "val"_a,
            "Returns the bencode value of the given value.  The bt_value val can be given as a "
            "bytes, str, int, list of bt_values, or dict of bytes/str -> bt_value pairs.  Note "
            "that str values will be encoded as utf-8 and will be *decoded* by bt_deserialize as "
            "as bytes.");

    m.def("bt_deserialize", [](py::bytes val) -> py::object {
        char* buffer;
        ssize_t len;
        if (PYBIND11_BYTES_AS_STRING_AND_SIZE(val.ptr(), &buffer, &len))
            throw std::runtime_error{"Unable to extract bytes contents!"};
        std::string_view data{buffer, static_cast<size_t>(len)};
        if (data.empty()) return py::none{};

        if (data[0] == 'i') {
            try {
                return py::int_{bt_deserialize<uint64_t>(data)};
            } catch (...) {
                return py::int_{bt_deserialize<int64_t>(data)};
            }
        }
        if (data[0] >= '0' && data[0] <= '9') {
            auto str = bt_deserialize<std::string_view>(data);
            return py::bytes{str.data(), str.size()};
        }
        if (data[0] == 'd') {
            py::dict d;
            build_dict(d, bt_dict_consumer{data});
            return d;
        }
        if (data[0] == 'l') {
            py::list l;
            build_list(l, bt_list_consumer{data});
            return l;
        }
        throw std::invalid_argument{"The given value is not a valid bencoded value"};
    },
    "val"_a,
    "Deserializes a bencoded value.  Deserialization produces a value of: `int`, `bytes`, `list`, "
    "or `dict`; lists contain 0 or more of these values (recursively), and dicts contain bytes "
    "keys each containing one of these values (again recursive).  Note that you always get "
    "`bytes` out, not `str`s: it is up to the caller to decide how to interpret these values."
    );
}

}
