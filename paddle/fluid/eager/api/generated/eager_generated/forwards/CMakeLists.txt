cc_library(
  eager_scale
  SRCS scale.cc
  DEPS phi autograd_meta scale_node)

if(NOT (NOT WITH_PYTHON AND ON_INFER))
  cc_library(
    final_dygraph_function
    SRCS dygraph_functions.cc dygraph_grad_functions.cc
         ${eager_manual_functions}
    DEPS ${eager_deps} final_dygraph_node)
  add_dependencies(final_dygraph_function eager_codegen)
endif()
