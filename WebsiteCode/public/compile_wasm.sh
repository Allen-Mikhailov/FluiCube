emcc ./c/main.c -o ./wasm/main.js -sEXPORTED_RUNTIME_METHODS=ccall,cwrap -sMODULARIZE -sEXPORT_ES6
