#pragma once

// Boards first flashed with default_16MB.csv keep their settings in the nvs
// partition at 0x9000, which a factory image overwrites. partitions_16mb.csv
// renames that area nvs_old and puts nvs past the app slots; this copies
// whatever nvs_old holds into nvs and then erases nvs_old. Call it first in
// setup(), before anything opens NVS. A no-op when nvs_old is empty or absent.
namespace nvsmigrate {
void run();
}
