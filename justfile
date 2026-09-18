source_files := `find ~+/src -type f -name "*.sv" | xargs echo`
output_dir := "build"
output_exe := "gpu"
common_dir := `echo $(pwd)/src/common`
num_cores := `nproc`

CC := "g++"

default: run

compile:
    mkdir -p {{output_dir}}
    cd {{output_dir}} && cmake .. -DCMAKE_BUILD_TYPE=Debug && cmake --build . -j{{num_cores}}

run *args: compile
    ./{{output_dir}}/sim/simulator {{args}}

test: compile
    cd {{output_dir}} && ctest -j{{num_cores}} --output-on-failure

debug *args: compile
    gdb --args {{output_dir}}/sim/simulator {{args}}

clean:
    rm -rf {{output_dir}}
    rm -f results.xml
