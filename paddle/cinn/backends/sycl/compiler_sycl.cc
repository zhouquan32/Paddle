// Copyright (c) 2021 CINN Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "paddle/cinn/backends/sycl/compiler_sycl.h"
#include <sys/stat.h>  // for mkdir
#include <fstream>
#include "paddle/cinn/runtime/sycl/sycl_backend_api.h"
using cinn::runtime::sycl::SYCLBackendAPI;

namespace cinn {
namespace backends {
namespace syclrtc {

Compiler::Compiler() {}

Compiler* Compiler::Global() {
  static Compiler* inst = new Compiler();
  return inst;
}

std::string Compiler::operator()(const std::string& code) {
  Arch gpu_type = cinn::common::HygonDCUArchSYCL{};
  return CompileToSo(code, gpu_type);
}

std::vector<std::string> Compiler::FindCINNRuntimeIncludePaths() {
  return {Context::Global().runtime_include_dir()};
}

std::string Compiler::CompileToSo(const std::string& source_code,
                                  const Arch gpu_type) {
  // create the folder to store sycl temporary files
  if (access(prefix_dir.c_str(), F_OK) == -1) {
    PADDLE_ENFORCE_NE(mkdir(prefix_dir.c_str(), 7),
                      -1,
                      ::common::errors::Fatal(
                          "SYCL backends error ! Fail to mkdir " + prefix_dir));
  }
  // get unique file_path
  compile_num++;
  std::string filename = prefix_dir + "/" + common::UniqName("sycl");
  source_file_path = filename + ".cc";
  shared_lib_path = filename + ".so";
  // write source file
  std::ofstream ofs(source_file_path.c_str(), std::ios::out);
  PADDLE_ENFORCE_EQ(
      ofs.is_open(),
      true,
      ::common::errors::Fatal("SYCL backends error ! Fail to open file %s",
                              source_file_path));
  ofs << source_code;
  ofs.close();
  // set compile command
  std::string command = compiler_path;
  // prepare include headers
  auto cinn_headers = FindCINNRuntimeIncludePaths();
  for (auto& header : cinn_headers) {
    command += " -I " + header;
  }
  SetDeviceArchOptions(gpu_type);
  command += " " + device_arch_options + " " + cxx_compile_options + " " +
             source_file_path + " -o " + shared_lib_path;
  // compile
  VLOG(2) << "compile command: " << command;
  PADDLE_ENFORCE_EQ(system(command.c_str()),
                    0,
                    ::common::errors::External(
                        "Following compile command failed:\n%s", command));
  return shared_lib_path;
}

void Compiler::SetDeviceArchOptions(const Arch gpu_type) {
  std::string gpu_version = SYCLBackendAPI::Global()->GetGpuVersion();
  gpu_type.Match(
      [&](common::HygonDCUArchSYCL) {
        device_arch_options = "-fsycl";
        device_arch_options += " -fsycl-targets=amdgcn-amd-amdhsa";
        device_arch_options +=
            " -Xsycl-target-backend --offload-arch=" + gpu_version;
      },
      [&](auto) {
        PADDLE_THROW(::common::errors::Fatal(
            "SYCL backends error ! NOT support this arch ! "));
      });
}

}  // namespace syclrtc
}  // namespace backends
}  // namespace cinn
