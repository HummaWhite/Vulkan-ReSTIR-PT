cmake -B "build/ext/assimp" -S "ext/assimp" -DLIBRARY_SUFFIX=""

cmake --build "build/ext/assimp" --config Release

cmake -B "build" -S .
