#include <cstring>
#include <cstdio>
#include "srpc.h"
#include "game/sce/iop.h"
#include "game/common/loader_rpc_types.h"
#include "game/common/game_common_types.h"
#include "common/versions.h"
#include "sbank.h"
#include "iso_api.h"
#include "common/util/Assert.h"

using namespace iop;

MusicTweaks gMusicTweakInfo;
constexpr int SRPC_MESSAGE_SIZE = 0x50;
uint8_t gLoaderBuf[SRPC_MESSAGE_SIZE];
int32_t gSoundEnable = 1;
u32 gInfoEE = 0;  // EE address where we should send info on each frame.

// english, french, germain, spanish, italian, japanese, uk.
static const char* languages[] = {"ENG", "FRE", "GER", "SPA", "ITA", "JAP", "UKE"};
const char* gLanguage = nullptr;

void srpc_init_globals() {
  memset((void*)&gMusicTweakInfo, 0, sizeof(gMusicTweakInfo));
  memset((void*)gLoaderBuf, 0, sizeof(gLoaderBuf));
  gSoundEnable = 1;
  gInfoEE = 0;
  gLanguage = languages[(int)Language::English];
}

// todo Thread_Player

void* RPC_Loader(unsigned int fno, void* data, int size);

u32 Thread_Loader() {
  sceSifQueueData dq;
  sceSifServeData serve;

  // set up RPC
  CpuDisableIntr();
  sceSifInitRpc(0);
  sceSifSetRpcQueue(&dq, GetThreadId());
  sceSifRegisterRpc(&serve, LOADER_RPC_ID, RPC_Loader, gLoaderBuf, nullptr, nullptr, &dq);
  CpuEnableIntr();
  sceSifRpcLoop(&dq);
  return 0;
}

// todo RPC_Player

void* RPC_Loader(unsigned int /*fno*/, void* data, int size) {
  int n_messages = size / SRPC_MESSAGE_SIZE;
  SoundRpcCommand* cmd = (SoundRpcCommand*)(data);
  if (gSoundEnable) {
    // I don't think it should be possible to have > 1 message here - the buffer isn't big enough.
    if (n_messages > 1) {
      ASSERT(false);
    }
    while (n_messages > 0) {
      switch (cmd->command) {
        case SoundCommand::LOAD_BANK: {
          // see if it's already loaded
          auto bank = LookupBank(cmd->load_bank.bank_name);
          if (!bank) {
            // see if we have room to load another
            auto new_bank = AllocateBank();
            if (new_bank) {
              // we do!
              LoadSoundBank(cmd->load_bank.bank_name, new_bank);
            }
          }
        } break;
        case SoundCommand::UNLOAD_BANK:
          printf("ignoring unload bank command!\n");
          break;
        case SoundCommand::GET_IRX_VERSION: {
          cmd->irx_version.major = IRX_VERSION_MAJOR;
          cmd->irx_version.minor = IRX_VERSION_MINOR;
          gInfoEE = cmd->irx_version.ee_addr;
          return cmd;
        } break;
        case SoundCommand::SET_LANGUAGE: {
          gLanguage = languages[cmd->set_language.langauge_id];
          printf("IOP language: %s\n", gLanguage);  // added.
          break;
        }
        case SoundCommand::LOAD_MUSIC:
          printf("ignoring load music\n");
          break;
        case SoundCommand::UNLOAD_MUSIC:
          printf("ignoring unload music\n");
          break;
        default:
          printf("Unhandled RPC Loader command %d\n", (int)cmd->command);
          ASSERT(false);
      }
      n_messages--;
      cmd++;
    }
  }
  return nullptr;
}
