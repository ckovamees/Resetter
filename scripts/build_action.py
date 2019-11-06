from platformio import util

Import("env")

print("********************* T E S T ************************")

# env.AlwaysBuild(env.Alias("node", None, ["node --version"]))

#env.AlwaysBuild(env.Alias("ota",
#    "$BUILD_DIR/${PROGNAME}.elf",
#    ["ota_script --firmware-path $SOURCE"]))

project_config = util.load_project_config()
print(project_config)

