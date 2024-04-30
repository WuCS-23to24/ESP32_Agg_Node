#ifndef B6B5A080_A253_4EB5_8937_6294B99B5CC8
#define B6B5A080_A253_4EB5_8937_6294B99B5CC8

#include <bootloader_random.h>
#include <esp_random.h>

#include "UUID.h"

struct uuids
{
  private:
    const char *service_uuid = "d544bcdc-58fc-4f26-a5a4-24e8fd7f12ae";
    const char *characteristic_uuid = "f74fbe6b-bed4-492b-b9ed-928869ee9fd3";

  public:
    const char *get_service_uuid()
    {
        return service_uuid;
    }

    const char *get_characteristic_uuid()
    {
        return characteristic_uuid;
    }
};

#endif /* B6B5A080_A253_4EB5_8937_6294B99B5CC8 */
