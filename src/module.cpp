#include "common.hpp"

PYBIND11_MODULE(oxenc, m) {
    oxenc::Bases_Init(m);
    oxenc::BEncode_Init(m);
}
