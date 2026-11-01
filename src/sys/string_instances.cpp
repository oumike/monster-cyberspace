// std::string members that some objects call out of line. When none of our
// own objects defines them, the linker takes the first archive that does,
// and in the ESP32-S3 libraries that is esp_matter: Matter and the BLE
// controller come along (about 150 KB flash and 11 KB RAM). Compiling
// features out (feature_flags.h) removed the objects that used to define them.
#include <string>

template std::string std::string::substr(std::string::size_type, std::string::size_type) const;
template std::string::size_type std::string::find(char, std::string::size_type) const noexcept;
