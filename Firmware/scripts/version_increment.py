# type: ignore
Import("env")  # noqa: F821 - PlatformIO injects this
import datetime
import os

# Get the project directory (where platformio.ini is located)
project_dir = env.get("PROJECT_DIR")
header_path = os.path.join(project_dir, "lib", "config", "build_timestamp.h")

# Generate timestamp immediately when script is loaded
timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")

# Ensure the directory exists
os.makedirs(os.path.dirname(header_path), exist_ok=True)

# Write the header file
with open(header_path, "w") as f:
    f.write(f'#pragma once\n')
    f.write(f'constexpr const char* BUILD_TIMESTAMP = "{timestamp}";\n')

print(f"Generated build timestamp: {timestamp}")