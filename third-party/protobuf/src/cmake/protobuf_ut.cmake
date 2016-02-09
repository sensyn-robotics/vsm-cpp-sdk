set(COMMON_TEST_SOURCES
  google/protobuf/test_util.cc                                 
  google/protobuf/test_util.h                                  
  google/protobuf/testing/googletest.cc                        
  google/protobuf/testing/googletest.h                         
  google/protobuf/testing/file.cc                              
  google/protobuf/testing/file.h)

set(PROTOBUF_TEST_SOURCES
  google/protobuf/stubs/common_unittest.cc                     
  google/protobuf/stubs/once_unittest.cc                       
  google/protobuf/stubs/strutil_unittest.cc                    
  google/protobuf/stubs/structurally_valid_unittest.cc         
  google/protobuf/stubs/stringprintf_unittest.cc               
  google/protobuf/stubs/template_util_unittest.cc              
  google/protobuf/stubs/type_traits_unittest.cc                
  google/protobuf/descriptor_database_unittest.cc              
  google/protobuf/descriptor_unittest.cc                       
  google/protobuf/dynamic_message_unittest.cc                  
  google/protobuf/extension_set_unittest.cc                    
  google/protobuf/generated_message_reflection_unittest.cc     
  google/protobuf/message_unittest.cc                          
  google/protobuf/reflection_ops_unittest.cc                   
  google/protobuf/repeated_field_unittest.cc                   
  google/protobuf/repeated_field_reflection_unittest.cc        
  google/protobuf/text_format_unittest.cc                      
  google/protobuf/unknown_field_set_unittest.cc                
  google/protobuf/wire_format_unittest.cc                      
  google/protobuf/io/coded_stream_unittest.cc                  
  google/protobuf/io/printer_unittest.cc                       
  google/protobuf/io/tokenizer_unittest.cc                     
  google/protobuf/io/zero_copy_stream_unittest.cc              
  google/protobuf/compiler/command_line_interface_unittest.cc  
  google/protobuf/compiler/importer_unittest.cc                
  google/protobuf/compiler/mock_code_generator.cc              
  google/protobuf/compiler/mock_code_generator.h               
  google/protobuf/compiler/parser_unittest.cc                  
  google/protobuf/compiler/cpp/cpp_bootstrap_unittest.cc       
  google/protobuf/compiler/cpp/cpp_unittest.h                  
  google/protobuf/compiler/cpp/cpp_unittest.cc                 
  google/protobuf/compiler/cpp/cpp_plugin_unittest.cc          
  google/protobuf/compiler/java/java_plugin_unittest.cc        
  google/protobuf/compiler/java/java_doc_comment_unittest.cc   
  google/protobuf/compiler/python/python_plugin_unittest.cc
  ${COMMON_TEST_SOURCES})
  
set(PROTOBUF_LAZY_DESCRIPTOR_TEST_SOURCES
    google/protobuf/compiler/cpp/cpp_unittest.cc
    ${COMMON_TEST_SOURCES})
    
set(PROTOBUF_LITE_TEST_SOURCES
  google/protobuf/lite_unittest.cc
  google/protobuf/test_util_lite.cc
  google/protobuf/test_util_lite.h)
  
set(PROTOC_INPUTS
  google/protobuf/unittest.proto                               
  google/protobuf/unittest_empty.proto                         
  google/protobuf/unittest_import.proto                        
  google/protobuf/unittest_import_public.proto                 
  google/protobuf/unittest_mset.proto                          
  google/protobuf/unittest_optimize_for.proto                  
  google/protobuf/unittest_embed_optimize_for.proto            
  google/protobuf/unittest_custom_options.proto                
  google/protobuf/unittest_lite.proto                          
  google/protobuf/unittest_import_lite.proto                   
  google/protobuf/unittest_import_public_lite.proto            
  google/protobuf/unittest_lite_imports_nonlite.proto          
  google/protobuf/unittest_no_generic_services.proto
  google/protobuf/compiler/cpp/cpp_test_bad_identifiers.proto)
  
set(TEST_PLUGIN_SOURCES
  google/protobuf/compiler/mock_code_generator.cc              
  google/protobuf/testing/file.cc                              
  google/protobuf/testing/file.h                               
  google/protobuf/compiler/test_plugin.cc)
  
# Files to be copied to the bin dir. Some unit tests need even C++ source files as
# resources.
set(PROTOBUF_RESOURCE_LIST
  ${PROTOC_INPUTS}
  google/protobuf/io/gzip_stream.h                             
  google/protobuf/io/gzip_stream_unittest.sh                   
  google/protobuf/testdata/golden_message                      
  google/protobuf/testdata/golden_packed_fields_message        
  google/protobuf/testdata/text_format_unittest_data.txt       
  google/protobuf/testdata/text_format_unittest_extensions_data.txt  
  google/protobuf/package_info.h                               
  google/protobuf/io/package_info.h                            
  google/protobuf/compiler/package_info.h                      
  google/protobuf/compiler/zip_output_unittest.sh              
  google/protobuf/unittest_enormous_descriptor.proto
  google/protobuf/descriptor.proto
  google/protobuf/compiler/plugin.proto
  google/protobuf/stubs/atomicops.h                             
  google/protobuf/stubs/atomicops_internals_arm_gcc.h           
  google/protobuf/stubs/atomicops_internals_arm_qnx.h           
  google/protobuf/stubs/atomicops_internals_atomicword_compat.h 
  google/protobuf/stubs/atomicops_internals_macosx.h            
  google/protobuf/stubs/atomicops_internals_mips_gcc.h          
  google/protobuf/stubs/atomicops_internals_pnacl.h             
  google/protobuf/stubs/atomicops_internals_x86_gcc.h           
  google/protobuf/stubs/atomicops_internals_x86_msvc.h          
  google/protobuf/stubs/common.h                                
  google/protobuf/stubs/platform_macros.h                       
  google/protobuf/stubs/once.h                                  
  google/protobuf/stubs/template_util.h                         
  google/protobuf/stubs/type_traits.h                           
  google/protobuf/descriptor.h                                  
  google/protobuf/descriptor.pb.h                               
  google/protobuf/descriptor_database.h                         
  google/protobuf/dynamic_message.h                             
  google/protobuf/extension_set.h                               
  google/protobuf/generated_enum_reflection.h                   
  google/protobuf/generated_message_util.h                      
  google/protobuf/generated_message_reflection.h                
  google/protobuf/message.h                                     
  google/protobuf/message_lite.h                                
  google/protobuf/reflection_ops.h                              
  google/protobuf/repeated_field.h                              
  google/protobuf/service.h                                     
  google/protobuf/text_format.h                                 
  google/protobuf/unknown_field_set.h                           
  google/protobuf/wire_format.h                                 
  google/protobuf/wire_format_lite.h                            
  google/protobuf/wire_format_lite_inl.h                        
  google/protobuf/io/coded_stream.h                             
  google/protobuf/io/printer.h                                  
  google/protobuf/io/tokenizer.h                                
  google/protobuf/io/zero_copy_stream.h                         
  google/protobuf/io/zero_copy_stream_impl.h                    
  google/protobuf/io/zero_copy_stream_impl_lite.h               
  google/protobuf/compiler/code_generator.h                     
  google/protobuf/compiler/command_line_interface.h             
  google/protobuf/compiler/importer.h                           
  google/protobuf/compiler/parser.h                             
  google/protobuf/compiler/plugin.h                             
  google/protobuf/compiler/plugin.pb.h                          
  google/protobuf/compiler/cpp/cpp_generator.h                  
  google/protobuf/compiler/java/java_generator.h                
  google/protobuf/compiler/python/python_generator.h
  google/protobuf/descriptor.pb.cc
  google/protobuf/compiler/plugin.pb.cc)


foreach(RES ${PROTOBUF_RESOURCE_LIST})
    configure_file(${RES} ${CMAKE_BINARY_DIR}/protobuf/${RES} COPYONLY)
endforeach()

add_custom_target(PROTOBUF_RESOURCES DEPENDS ${PROTOBUF_RESOURCE_LIST})

Compile_protobuf_definitions(
    "${PROTOC_INPUTS}"
    ${CMAKE_SOURCE_DIR}/../../third-party/protobuf/src
    protobuf_ut.h)

add_executable(protobuf_test ${PROTOBUF_TEST_SOURCES} $<TARGET_OBJECTS:protobuf_objlib>
    ${PROTOBUF_AUTO_SOURCES})

target_link_libraries(protobuf_test gtest gtest_main)

add_executable(test_plugin ${TEST_PLUGIN_SOURCES} $<TARGET_OBJECTS:protobuf_objlib>)
target_link_libraries(test_plugin gtest gtest_main)

add_executable(protobuf_lazy_descriptor_test ${PROTOBUF_LAZY_DESCRIPTOR_TEST_SOURCES}
    $<TARGET_OBJECTS:protobuf_objlib> ${PROTOBUF_AUTO_SOURCES})
target_link_libraries(protobuf_lazy_descriptor_test gtest gtest_main)
set_target_properties(protobuf_lazy_descriptor_test
    PROPERTIES COMPILE_FLAGS -DPROTOBUF_TEST_NO_DESCRIPTORS)
    
# Just link with full protobuf support, tests are still for lite only
add_executable(protobuf_lite_test ${PROTOBUF_LITE_TEST_SOURCES}
    $<TARGET_OBJECTS:protobuf_objlib> ${PROTOBUF_AUTO_SOURCES})

add_dependencies(protobuf_test protobuf_compiler test_plugin PROTOBUF_RESOURCES)
add_dependencies(protobuf_lazy_descriptor_test protobuf_compiler)
add_dependencies(protobuf_lite_test protobuf_compiler)
