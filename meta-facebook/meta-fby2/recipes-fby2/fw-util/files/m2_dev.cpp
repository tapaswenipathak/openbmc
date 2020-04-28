#include "fw-util.h"
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include "server.h"
#include <openbmc/pal.h>
#include <syslog.h>
#ifdef BIC_SUPPORT
#include <facebook/bic.h>
#include <unistd.h>

using namespace std;

#if defined(CONFIG_FBY2_GPV2)

#define MAX_READ_RETRY 10
/* NVMe-MI Form Factor Information 0 Register */
#define FFI_0_STORAGE 0x00
#define FFI_0_ACCELERATOR 0x01

#define MAX_GPV2_DRIVE_NUM 12

class M2_DevComponent : public Component { 
  uint8_t slot_id = 0;
  uint8_t dev_id = 0;

  enum Dev_Main_Slot {
    ON_EVEN = 0,
    ON_ODD = 1
  };

  struct PowerInfo {
    int ret;
    uint8_t status;
    uint8_t nvme_ready;
    uint8_t ffi;
    uint8_t major_ver;
    uint8_t minor_ver;
  };
  static PowerInfo statusTable[MAX_GPV2_DRIVE_NUM];
  static bool isDual;
  static Dev_Main_Slot dev_main_slot;

  Server server;
  int _update(string image, uint8_t force);
  void save_info(uint8_t id, int ret, uint8_t status, uint8_t nvme_ready, uint8_t ffi, uint8_t major_ver, uint8_t minor_ver);
  void print_single();
  void print_dual();
  public:
  M2_DevComponent(string fru, string comp, uint8_t _slot_id, uint8_t dev_id)
    : Component(fru, comp), slot_id(_slot_id), dev_id(dev_id), server(_slot_id, fru) {
    if (fby2_get_slot_type(_slot_id) != SLOT_TYPE_GPV2) {
      (*fru_list)[fru].erase(comp);
      if ((*fru_list)[fru].size() == 0) {
        fru_list->erase(fru);
      }
    }
  }

  int print_version() {
    int ret = 0;
    uint8_t retry = MAX_READ_RETRY;
    uint16_t vendor_id = 0;
    uint8_t nvme_ready = 0 ,status = 0 ,ffi = 0 ,meff = 0 ,major_ver = 0, minor_ver = 0;

    if (fby2_get_slot_type(slot_id) != SLOT_TYPE_GPV2)
      return -1;

    while (retry) {
      ret = bic_get_dev_power_status(slot_id, dev_id + 1, &nvme_ready, &status, &ffi, &meff, &vendor_id, &major_ver,&minor_ver);
      if (!ret)
        break;
      msleep(50);
      retry--;
    }
    save_info(dev_id, ret, status, nvme_ready, ffi, major_ver, minor_ver);
    if (isDual == false && meff == MEFF_DUAL_M2) {
      isDual = true;
      if (dev_id % 2 == 0) {
        dev_main_slot = Dev_Main_Slot::ON_EVEN;
      } else {
        dev_main_slot = Dev_Main_Slot::ON_ODD;
      }
    }
    if (dev_id + 1 == MAX_GPV2_DRIVE_NUM) {
      if (isDual == true) {
        print_dual();
      } else {
        print_single();
      }
      isDual = false;
    }
    return 0;
  }
  int update(string image);
  int fupdate(string image);
};

M2_DevComponent::PowerInfo M2_DevComponent::statusTable[] = {0};
bool M2_DevComponent::isDual = false;
M2_DevComponent::Dev_Main_Slot M2_DevComponent::dev_main_slot = M2_DevComponent::Dev_Main_Slot::ON_EVEN;

void M2_DevComponent::save_info(uint8_t id, int ret, uint8_t status,
 uint8_t nvme_ready, uint8_t ffi, uint8_t major_ver, uint8_t minor_ver) {
  if (id < 0 || id >= MAX_GPV2_DRIVE_NUM) {
    return;
  }
  statusTable[id].ret = ret;
  statusTable[id].status = status;
  statusTable[id].nvme_ready = nvme_ready;
  statusTable[id].ffi = ffi;
  statusTable[id].major_ver = major_ver;
  statusTable[id].minor_ver = minor_ver;
}

void M2_DevComponent::print_single() {
  for (int i = 0; i < MAX_GPV2_DRIVE_NUM; i++) {
    printf("device%d Version: ", i);
    if (statusTable[i].ret) {
      printf("NA\n");
    } else if (!statusTable[i].status) {
      printf("NA(Not Present)\n");
    } else if (!statusTable[i].nvme_ready) {
      printf("NA(NVMe Not Ready)\n");
    } else if (statusTable[i].ffi != FFI_0_ACCELERATOR) {
      printf("NA(Not Accelerator)\n");
    } else {
      printf("v%d.%d\n", statusTable[i].major_ver, statusTable[i].minor_ver);
    }
  }
}

void M2_DevComponent::print_dual() {
  for (int i = 0; i < MAX_GPV2_DRIVE_NUM; i+=2) {
    printf("device%d/%d Version: ", i, i + 1);
    if (statusTable[i + dev_main_slot].ret) {
      printf("NA\n");
    } else if (!statusTable[i + dev_main_slot].status) {
      printf("NA(Not Present)\n");
    } else if (!statusTable[i + dev_main_slot].nvme_ready) {
      printf("NA(NVMe Not Ready)\n");
    } else if (statusTable[i + dev_main_slot].ffi != FFI_0_ACCELERATOR) {
      printf("NA(Not Accelerator)\n");
    } else {
      printf("v%d.%d\n", statusTable[i + dev_main_slot].major_ver, statusTable[i + dev_main_slot].minor_ver);
    }
  }
}

int M2_DevComponent::_update(string image, uint8_t force) {
  int ret;
  uint8_t status = DEVICE_POWER_OFF;
  uint8_t type = DEV_TYPE_UNKNOWN;
  uint8_t nvme_ready = 0;
  if (fby2_get_slot_type(slot_id) != SLOT_TYPE_GPV2) {
    return FW_STATUS_NOT_SUPPORTED;
  }
  try {
    ret = pal_get_dev_info(slot_id, dev_id+1, &nvme_ready ,&status, &type, force);
    if (!ret) {
      if (status != 0) {
        syslog(LOG_WARNING, "update: Slot%u Dev%d power=%u nvme_ready=%u type=%u", slot_id, dev_id, status, nvme_ready, type);
        if (type == DEV_TYPE_VSI_ACC) {
          server.ready();
          ret = bic_update_dev_firmware(slot_id, dev_id, UPDATE_VSI, (char *)image.c_str(),0);
          if (ret) {
            syslog(LOG_WARNING, "update: Slot%u Dev%d vsi rp failed", slot_id, dev_id);
          }
        } else if (type == DEV_TYPE_BRCM_ACC) {
          server.ready();
          ret = bic_update_dev_firmware(slot_id, dev_id, UPDATE_BRCM, (char *)image.c_str(),0);
          if (ret == FW_STATUS_SUCCESS) {
            pal_set_device_power(slot_id, dev_id+1, SERVER_POWER_CYCLE);
          } else {
            syslog(LOG_WARNING, "update: Slot%u Dev%d brcm vk failed", slot_id, dev_id);
          }
        } else if (type == DEV_TYPE_SPH_ACC) {
          uint16_t slot_dev_id = (slot_id << 4) | dev_id;
          sleep(1);

          server.ready();
          ret = bic_update_fw(slot_dev_id, UPDATE_SPH, (char *)image.c_str());

          printf("* Please do 12V-power-cycle on Slot%u to activate new FPGA fw.\n", slot_id);
        } else {
          printf("Can not get the device type, try again later.\n");
          ret = FW_STATUS_NOT_SUPPORTED;
        }
      } else {
        printf("device%d: Not Present\n", dev_id);
        ret = FW_STATUS_NOT_SUPPORTED;
      }
    } else {
      printf("Device%d status is unknown.\n", dev_id);
      ret = FW_STATUS_FAILURE;
    }
  } catch(string err) {
    ret = FW_STATUS_NOT_SUPPORTED;
  }
  return ret;
}

int M2_DevComponent::update(string image) {
  return _update(image, 0);
}

int M2_DevComponent::fupdate(string image) {
  return _update(image, 1);
}

// Register M.2 device components
// slot1 device from 0 to 11
M2_DevComponent m2_slot1_dev0("slot1", "device0", 1, 0);
M2_DevComponent m2_slot1_dev1("slot1", "device1", 1, 1);
M2_DevComponent m2_slot1_dev2("slot1", "device2", 1, 2);
M2_DevComponent m2_slot1_dev3("slot1", "device3", 1, 3);
M2_DevComponent m2_slot1_dev4("slot1", "device4", 1, 4);
M2_DevComponent m2_slot1_dev5("slot1", "device5", 1, 5);
M2_DevComponent m2_slot1_dev6("slot1", "device6", 1, 6);
M2_DevComponent m2_slot1_dev7("slot1", "device7", 1, 7);
M2_DevComponent m2_slot1_dev8("slot1", "device8", 1, 8);
M2_DevComponent m2_slot1_dev9("slot1", "device9", 1, 9);
M2_DevComponent m2_slot1_dev10("slot1", "device10", 1, 10);
M2_DevComponent m2_slot1_dev11("slot1", "device11", 1, 11);
// slot3 device from 0 to 11
M2_DevComponent m2_slot3_dev0("slot3", "device0", 3, 0);
M2_DevComponent m2_slot3_dev1("slot3", "device1", 3, 1);
M2_DevComponent m2_slot3_dev2("slot3", "device2", 3, 2);
M2_DevComponent m2_slot3_dev3("slot3", "device3", 3, 3);
M2_DevComponent m2_slot3_dev4("slot3", "device4", 3, 4);
M2_DevComponent m2_slot3_dev5("slot3", "device5", 3, 5);
M2_DevComponent m2_slot3_dev6("slot3", "device6", 3, 6);
M2_DevComponent m2_slot3_dev7("slot3", "device7", 3, 7);
M2_DevComponent m2_slot3_dev8("slot3", "device8", 3, 8);
M2_DevComponent m2_slot3_dev9("slot3", "device9", 3, 9);
M2_DevComponent m2_slot3_dev10("slot3", "device10", 3, 10);
M2_DevComponent m2_slot3_dev11("slot3", "device11", 3, 11);
#endif

#endif
