set(SOURCES test_sets.cpp)
	
set(STRUCT_DIR ${PROJECT_SOURCE_DIR}/structures)
set(FLOCK_BENCH "list_OL" "list_HOH")

function(add_benchmark NAME STRUCT_NAME DEFS)
  add_executable(${NAME} ${SOURCES})
  target_compile_definitions(${NAME} PRIVATE ${DEFS})
  target_link_libraries(${NAME} PRIVATE flock)
  target_include_directories(${NAME} PRIVATE ${STRUCT_NAME})
endfunction()

foreach(bench ${FLOCK_BENCH})
  add_benchmark(${bench} ${STRUCT_DIR}/${bench} "")
endforeach()