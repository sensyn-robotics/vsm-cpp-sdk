set(CMAKE_CXX_FLAGS "-O0 -gdwarf-3 -Wwrite-strings -Woverloaded-virtual -Wno-sign-compare")

# Setup common sources and libds
set(LIBPROTOBUF_LITE_SOURCES
    google/protobuf/stubs/atomicops_internals_x86_gcc.cc         
    google/protobuf/stubs/atomicops_internals_x86_msvc.cc        
    google/protobuf/stubs/common.cc                              
    google/protobuf/stubs/once.cc                                
    google/protobuf/stubs/hash.h                                 
    google/protobuf/stubs/map-util.h                             
    google/protobuf/stubs/stl_util.h                             
    google/protobuf/stubs/stringprintf.cc                        
    google/protobuf/stubs/stringprintf.h                         
    google/protobuf/extension_set.cc                             
    google/protobuf/generated_message_util.cc                    
    google/protobuf/message_lite.cc                              
    google/protobuf/repeated_field.cc                            
    google/protobuf/wire_format_lite.cc                          
    google/protobuf/io/coded_stream.cc                           
    google/protobuf/io/coded_stream_inl.h                        
    google/protobuf/io/zero_copy_stream.cc                       
    google/protobuf/io/zero_copy_stream_impl_lite.cc)
    
set(LIBPROTOBUF_SOURCES
    google/protobuf/stubs/strutil.cc                             
    google/protobuf/stubs/strutil.h                              
    google/protobuf/stubs/substitute.cc                          
    google/protobuf/stubs/substitute.h                           
    google/protobuf/stubs/structurally_valid.cc                  
    google/protobuf/descriptor.cc                                
    google/protobuf/descriptor.pb.cc                             
    google/protobuf/descriptor_database.cc                       
    google/protobuf/dynamic_message.cc                           
    google/protobuf/extension_set_heavy.cc                       
    google/protobuf/generated_message_reflection.cc              
    google/protobuf/message.cc                                   
    google/protobuf/reflection_ops.cc                            
    google/protobuf/service.cc                                   
    google/protobuf/text_format.cc                               
    google/protobuf/unknown_field_set.cc                         
    google/protobuf/wire_format.cc                               
    google/protobuf/io/gzip_stream.cc                            
    google/protobuf/io/printer.cc                                
    google/protobuf/io/tokenizer.cc                              
    google/protobuf/io/zero_copy_stream_impl.cc                  
    google/protobuf/compiler/importer.cc                         
    google/protobuf/compiler/parser.cc)
    
set(LIBPROTOC_SOURCES
    google/protobuf/compiler/code_generator.cc                   
    google/protobuf/compiler/command_line_interface.cc           
    google/protobuf/compiler/plugin.cc                           
    google/protobuf/compiler/plugin.pb.cc                        
    google/protobuf/compiler/subprocess.cc                       
    google/protobuf/compiler/subprocess.h                        
    google/protobuf/compiler/zip_writer.cc                       
    google/protobuf/compiler/zip_writer.h                        
    google/protobuf/compiler/cpp/cpp_enum.cc                     
    google/protobuf/compiler/cpp/cpp_enum.h                      
    google/protobuf/compiler/cpp/cpp_enum_field.cc               
    google/protobuf/compiler/cpp/cpp_enum_field.h                
    google/protobuf/compiler/cpp/cpp_extension.cc                
    google/protobuf/compiler/cpp/cpp_extension.h                 
    google/protobuf/compiler/cpp/cpp_field.cc                    
    google/protobuf/compiler/cpp/cpp_field.h                     
    google/protobuf/compiler/cpp/cpp_file.cc                     
    google/protobuf/compiler/cpp/cpp_file.h                      
    google/protobuf/compiler/cpp/cpp_generator.cc                
    google/protobuf/compiler/cpp/cpp_helpers.cc                  
    google/protobuf/compiler/cpp/cpp_helpers.h                   
    google/protobuf/compiler/cpp/cpp_message.cc                  
    google/protobuf/compiler/cpp/cpp_message.h                   
    google/protobuf/compiler/cpp/cpp_message_field.cc            
    google/protobuf/compiler/cpp/cpp_message_field.h             
    google/protobuf/compiler/cpp/cpp_options.h                   
    google/protobuf/compiler/cpp/cpp_primitive_field.cc          
    google/protobuf/compiler/cpp/cpp_primitive_field.h           
    google/protobuf/compiler/cpp/cpp_service.cc                  
    google/protobuf/compiler/cpp/cpp_service.h                   
    google/protobuf/compiler/cpp/cpp_string_field.cc             
    google/protobuf/compiler/cpp/cpp_string_field.h              
    google/protobuf/compiler/java/java_enum.cc                   
    google/protobuf/compiler/java/java_enum.h                    
    google/protobuf/compiler/java/java_enum_field.cc             
    google/protobuf/compiler/java/java_enum_field.h              
    google/protobuf/compiler/java/java_extension.cc              
    google/protobuf/compiler/java/java_extension.h               
    google/protobuf/compiler/java/java_field.cc                  
    google/protobuf/compiler/java/java_field.h                   
    google/protobuf/compiler/java/java_file.cc                   
    google/protobuf/compiler/java/java_file.h                    
    google/protobuf/compiler/java/java_generator.cc              
    google/protobuf/compiler/java/java_helpers.cc                
    google/protobuf/compiler/java/java_helpers.h                 
    google/protobuf/compiler/java/java_message.cc                
    google/protobuf/compiler/java/java_message.h                 
    google/protobuf/compiler/java/java_message_field.cc          
    google/protobuf/compiler/java/java_message_field.h           
    google/protobuf/compiler/java/java_primitive_field.cc        
    google/protobuf/compiler/java/java_primitive_field.h         
    google/protobuf/compiler/java/java_service.cc                
    google/protobuf/compiler/java/java_service.h                 
    google/protobuf/compiler/java/java_string_field.cc           
    google/protobuf/compiler/java/java_string_field.h            
    google/protobuf/compiler/java/java_doc_comment.cc            
    google/protobuf/compiler/java/java_doc_comment.h             
    google/protobuf/compiler/python/python_generator.cc)
  
set(PROTOC_SOURCES google/protobuf/compiler/main.cc)

set_directory_properties(PROPERTIES INCLUDE_DIRECTORIES "")
include_directories(".")

# Reused by protoc, sdk and UT
add_library(protobuf_objlib OBJECT ${LIBPROTOC_SOURCES} ${LIBPROTOBUF_SOURCES} ${LIBPROTOBUF_LITE_SOURCES})

add_executable(protoc ${PROTOC_SOURCES} $<TARGET_OBJECTS:protobuf_objlib>)

if (PROTOBUF_UT_ENABLED)
    add_subdirectory(../gtest gtest)
    include_directories(../gtest/include)
    include(cmake/protobuf_ut.cmake)
endif()
