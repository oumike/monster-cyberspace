# Pin the language standards for the host build. GCC 15 defaults C to C23,
# where `bool` is a keyword, and the QRCode library typedefs its own bool.
Import("env")

env.Append(CFLAGS=["-std=gnu11"], CXXFLAGS=["-std=gnu++17"])
