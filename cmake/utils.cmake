function(force_redefine_file_macro_for_sources targetname)
    get_target_property(source_files "${targetname}" SOURCES)
    foreach(sourcefile ${source_files})
        # Get source file's current list of compile definitions.
        get_property(defs SOURCE "${sourcefile}"
            PROPERTY COMPILE_DEFINITIONS)
        # Get the relative path of the source file in project directory
        get_filename_component(filepath "${sourcefile}" ABSOLUTE)
        string(REPLACE ${PROJECT_SOURCE_DIR}/ "" relpath ${filepath})
        list(APPEND defs "__FILE__=\"${relpath}\"")
        # Set the updated compile definitions on the source file.
        set_property(
            SOURCE "${sourcefile}"
            PROPERTY COMPILE_DEFINITIONS ${defs}
            )
    endforeach()
endfunction()

function(nemo_add_executable targetname srcs depends libs)
    add_executable(${targetname} ${srcs})
    add_dependencies(${targetname} ${depends})
    force_redefine_file_macro_for_sources(${targetname})
    target_link_libraries(${targetname} ${libs})
endfunction()

function(nemo_add_tests source_path depends libs)
    file(GLOB srcs ${source_path}/*.cc)
    foreach(src ${srcs})
        get_filename_component(srcname ${src} NAME_WE)
        nemo_add_executable("${srcname}" "${source_path}/${srcname}.cc" "${depends}" "${libs}")
    endforeach()
endfunction()

function(ragel_maker  src_rl outputlist outputdir)
    get_filename_component(src_file ${src_rl} NAME_WE)
    set(rl_out ${outputdir}/${src_file}.rl.cc)

    set(${outputlist} ${${outputlist}} ${rl_out} PARENT_SCOPE)

    add_custom_command(
        OUTPUT ${rl_out}
        COMMAND cd ${outputdir}
        COMMAND ragel ${src_rl} -o ${rl_out} -l -C -G2
        DEPENDS ${src_rl}
    )
    set_source_files_properties(${rl_out} PROPERTIES GENERATED TRUE)
endfunction()


function(protobuf_maker source_path header_output_path src_output_path)
    file(GLOB srcs ${source_path}/*.proto)
    list(APPEND PROTO_FLAGS --proto_path=${source_path})
    foreach(src ${srcs})
        set(proto_header "${source_path}/${src}.pb.h")
        set(proto_src "${source_path}/${src}.pb.cc")
    
        EXECUTE_PROCESS(
            COMMAND ${PROTOBUF_PROTOC_EXECUTABLE} ${PROTO_FLAGS} --cpp_out=${src_output_path} ${src}.proto
        )
        message("moving " ${proto_header} " to " ${header_output_path})
        EXECUTE_PROCESS(
            COMMAND mv ${proto_header} ${header_output_path}
        )
    endforeach()
endfunction()

function(append_files source_path target)
    file(GLOB srcs ${source_path}/*.cc)
    foreach(src ${srcs})
        list(APPEND target "${source_path}/${src}.cc")
    endforeach()
endfunction()
