import os

dirs = [
    "AndroidApp/app/src/main/java/com/example/hc06app",
    "AndroidApp/app/src/main/res/layout",
    "AndroidApp/app/src/main/res/values",
    "AndroidApp/app/src/main/res/raw"
]

for d in dirs:
    os.makedirs(d, exist_ok=True)

print("Directories created successfully.")
