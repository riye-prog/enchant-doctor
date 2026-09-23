#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
case "$(uname -s)" in
    Darwin)
        JAVA_HOME=${JAVA_HOME:-$(/usr/libexec/java_home -v 25)}
        jni_platform=darwin
        ;;
    Linux)
        : "${JAVA_HOME:?set JAVA_HOME to a java 25 jdk}"
        jni_platform=linux
        ;;
    *)
        printf '%s\n' 'use cmake directly with your jdk and vulkan sdk on this platform'
        exit 1
        ;;
esac
export JAVA_HOME
cmake -S "$project_dir/native" -B "$project_dir/native/build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DJAVA_INCLUDE_PATH="$JAVA_HOME/include" \
    -DJAVA_INCLUDE_PATH2="$JAVA_HOME/include/$jni_platform"
cmake --build "$project_dir/native/build" --parallel 6
