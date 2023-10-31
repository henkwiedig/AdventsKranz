import subprocess
from datetime import datetime

Import("env")

def get_firmware_specifier_build_flag():
    ret = subprocess.run(["git", "rev-parse", "--short", "HEAD"], stdout=subprocess.PIPE, text=True)
    build_version = ret.stdout.strip() + "_" + datetime.now().strftime("%Y-%m-%d_%H:%M:%S")
    build_flag = "-D VERSION=\\\"" + build_version + "\\\""
    print ("Firmware Revision: " + build_version)
    return (build_flag)

env.Append(
    BUILD_FLAGS=[get_firmware_specifier_build_flag()]
)