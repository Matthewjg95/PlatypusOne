# Strict warnings applied per-target via platypus_set_warnings().
function(platypus_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion
            -Wnon-virtual-dtor -Woverloaded-virtual)
    endif()
endfunction()
