//
// Created by cheesema on 2019-07-08.
//

#ifndef LIBAPR_GPUAPRTEST_HPP
#define LIBAPR_GPUAPRTEST_HPP

#include "data_structures/APR/access/GPUAccess.hpp"

void compute_spatial_info_gpu(GPUAccessHelper& access, std::vector<uint64_t>& temp);

void run_simple_test(std::vector<uint64_t>& temp, uint64_t size);

void check_access_vectors(GPUAccessHelper& access, std::vector<uint16_t>& y_vec_out, std::vector<uint64_t>& xz_end_vec_out, std::vector<uint64_t>& level_xz_vec_out);

#endif //LIBAPR_GPUAPRTEST_CUH_HPP
