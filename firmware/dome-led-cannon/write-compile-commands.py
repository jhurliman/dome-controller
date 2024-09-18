import os
import subprocess

Import("env")

# include toolchain paths
env.Replace(COMPILATIONDB_INCLUDE_TOOLCHAIN=True)

# override compilation DB path
COMPILATIONDB_PATH = os.path.join(env.subst("$BUILD_DIR"), "compile_commands.json")
env.Replace(COMPILATIONDB_PATH=COMPILATIONDB_PATH)

if "compiledb" not in COMMAND_LINE_TARGETS:
    subprocess.run(['pio', 'run', '-t', 'compiledb'])
