Import("env")
folder = env.GetProjectOption("src_folder")

# Generic
env.Replace(
    PROJECT_SRC_DIR="$PROJECT_DIR/src/" + folder
)

env.AddCustomTarget(
    "uf2",
    "$BUILD_DIR/firmware.bin",
    '"$PYTHONEXE" "$PROJECT_DIR/uf2/uf2conv.py" "$BUILD_DIR/firmware.bin" -c -b 0x00 -f ESP32S3 -o "$PROJECT_DIR/uf2/output/$PIOENV".uf2'
)