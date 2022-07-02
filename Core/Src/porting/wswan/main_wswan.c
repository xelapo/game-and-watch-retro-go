#include <odroid_system.h>

#include <assert.h>
#include "gw_lcd.h"
#include "gw_linker.h"
#include "gw_buttons.h"
#include "rom_manager.h"
#include "common.h"
#include "lz4_depack.h"
#include "miniz.h"
#include "lzma.h"
#include "appid.h"
#include "bilinear.h"
#include "rg_i18n.h"

#include "mednafen/wswan-settings.h"
#include "mednafen/git.h"
#include "mednafen/wswan/wswan.h"
#include "mednafen/mempatcher.h"
#include "mednafen/wswan/wswan-gfx.h"
#include "mednafen/wswan/wswan-interrupt.h"
#include "mednafen/wswan/wswan-memory.h"
#include "mednafen/wswan/start.inc"
#include "mednafen/wswan/wswan-sound.h"
#include "mednafen/wswan/wswan-v30mz.h"
#include "mednafen/wswan/wswan-rtc.h"
#include "mednafen/wswan/wswan-eeprom.h"

#define WSWAN_FPS 75 // WonderSwan screen refresh rate is 75.4 Hz
#define WSWAN_SAMPLE_RATE 48000
#define WSWAN_AUDIO_BUFFER_LENGTH (WSWAN_SAMPLE_RATE / WSWAN_FPS)
#define AUDIO_BUFFER_LENGTH_DMA_WSWAN (2 * WSWAN_AUDIO_BUFFER_LENGTH)
static int16_t audio_samples_buf[WSWAN_AUDIO_BUFFER_LENGTH];
static int32_t audio_samples_buf_size = WSWAN_AUDIO_BUFFER_LENGTH;

#define WIDTH 320
#define WSWAN_WIDTH (244)
#define WSWAN_HEIGHT (144)

int wsc = 1; /*1 color/0 mono*/
static bool rotate = false;

uint32		rom_size;
uint16 WSButtonStatus;
static uint32 SRAMSize;
static uint32 mono_pal_start = 0x000000;
static uint32 mono_pal_end   = 0xFFFFFF;

void ws_input_read(odroid_gamepad_state_t* joy_state) {
    if (!rotate) {
        if (joy_state->values[ODROID_INPUT_LEFT]) {
            WSButtonStatus |= 1<<7;
        } else {
            WSButtonStatus &= ~(1<<7);
        }
        if (joy_state->values[ODROID_INPUT_RIGHT]) {
            WSButtonStatus |= 1<<5;
        } else {
            WSButtonStatus &= ~(1<<5);
        }
        if (joy_state->values[ODROID_INPUT_UP]) {
            WSButtonStatus |= 1<<4;
        } else {
            WSButtonStatus &= ~(1<<4);
        }
        if (joy_state->values[ODROID_INPUT_DOWN]) {
            WSButtonStatus |= 1<<6;
        } else {
            WSButtonStatus &= ~(1<<6);
        }
        if (joy_state->values[ODROID_INPUT_A]) {
            WSButtonStatus |= (1<<0);
        } else {
            WSButtonStatus &= ~(1<<0);
        }
        if (joy_state->values[ODROID_INPUT_B]) {
            WSButtonStatus |= 1<<1;
        } else {
            WSButtonStatus &= ~(1<<1);
        }
        // Game button on G&W
        if (joy_state->values[ODROID_INPUT_START]) {
            WSButtonStatus |= 1<<2;
        } else {
            WSButtonStatus &= ~(1<<2);
        }
        // Start button on Zelda G&W
        if (joy_state->values[ODROID_INPUT_X]) {
            WSButtonStatus |= 1<<8;
        } else {
            WSButtonStatus &= ~(1<<8);
        }
        // Time button on G&W
        if (joy_state->values[ODROID_INPUT_SELECT]) {
            WSButtonStatus |= 1<<9;
        } else {
            WSButtonStatus &= ~(1<<9);
        }
        // Select button on Zelda G&W
        if (joy_state->values[ODROID_INPUT_Y]) {
            WSButtonStatus |= 1<<3;
        } else {
            WSButtonStatus &= ~(1<<3);
        }
    } else {
        if (joy_state->values[ODROID_INPUT_LEFT]) {
            WSButtonStatus |= 1<<4;
        } else {
            WSButtonStatus &= ~(1<<4);
        }
        if (joy_state->values[ODROID_INPUT_RIGHT]) {
            WSButtonStatus |= 1<<6;
        } else {
            WSButtonStatus &= ~(1<<6);
        }
        if (joy_state->values[ODROID_INPUT_UP]) {
            WSButtonStatus |= 1<<5;
        } else {
            WSButtonStatus &= ~(1<<5);
        }
        if (joy_state->values[ODROID_INPUT_DOWN]) {
            WSButtonStatus |= 1<<7;
        } else {
            WSButtonStatus &= ~(1<<7);
        }
        if (joy_state->values[ODROID_INPUT_A]) {
            WSButtonStatus |= (1<<0);
        } else {
            WSButtonStatus &= ~(1<<0);
        }
        if (joy_state->values[ODROID_INPUT_B]) {
            WSButtonStatus |= 1<<1;
        } else {
            WSButtonStatus &= ~(1<<1);
        }
        // Game button on G&W
        if (joy_state->values[ODROID_INPUT_START]) {
            WSButtonStatus |= 1<<2;
        } else {
            WSButtonStatus &= ~(1<<2);
        }
        // Start button on Zelda G&W
        if (joy_state->values[ODROID_INPUT_X]) {
            WSButtonStatus |= 1<<9;
        } else {
            WSButtonStatus &= ~(1<<9);
        }
        // Time button on G&W
        if (joy_state->values[ODROID_INPUT_SELECT]) {
            WSButtonStatus |= 1<<8;
        } else {
            WSButtonStatus &= ~(1<<8);
        }
        // Select button on Zelda G&W
        if (joy_state->values[ODROID_INPUT_Y]) {
            WSButtonStatus |= 1<<3;
        } else {
            WSButtonStatus &= ~(1<<3);
        }
    }
}

static void reset(void)
{
    int u0;

    v30mz_reset(); /* Reset CPU */
    WSwan_MemoryReset();
    WSwan_GfxReset();
    WSwan_SoundReset();
    WSwan_InterruptReset();
    WSwan_RTCReset();
    WSwan_EEPROMReset();

    for(u0=0;u0<0xc9;u0++)
    {
        if(u0 != 0xC4 && u0 != 0xC5 && u0 != 0xBA && u0 != 0xBB)
            WSwan_writeport(u0,startio[u0]);
    }

    v30mz_set_reg(NEC_SS,0);
    v30mz_set_reg(NEC_SP,0x2000);
}

static int load(const uint8_t *data, size_t size)
{
    uint32 pow_size      = 0;
    uint32 real_rom_size = 0;
    uint8 header[10];

    if(size < 65536)
        return(0);

    real_rom_size = (size + 0xFFFF) & ~0xFFFF;
    pow_size = 1;
    while(pow_size < real_rom_size)
        pow_size*=2;
    rom_size      = pow_size + (pow_size == 0);
    wsCartROM     = (uint8 *)data;

    memcpy(header, wsCartROM + rom_size - 10, 10);

    SRAMSize = 0;
    eeprom_size = 0;

    switch(header[5])
    {
      case 0x01: SRAMSize =   8 * 1024; break;
      case 0x02: SRAMSize =  32 * 1024; break;
      case 0x03: SRAMSize = 128 * 1024; break;
      case 0x04: SRAMSize = 256 * 1024; break; /* Dicing Knight!, Judgement Silver */
      // We won't support Wonder Gate on the G&W
//      case 0x05: SRAMSize = 512 * 1024; break; /* Wonder Gate */

      case 0x10: eeprom_size = 128; break;
      case 0x20: eeprom_size = 2 *1024; break;
      case 0x50: eeprom_size = 1024; break;
    }

    MDFNMP_Init(16384, (1 << 20) / 1024);

    if(header[6] & 0x1)
        rotate = true;

    v30mz_init(WSwan_readmem20, WSwan_writemem20, WSwan_readport, WSwan_writeport);
    WSwan_MemoryInit(MDFN_GetSettingB("wswan.language"), wsc, SRAMSize, false); /* EEPROM and SRAM are loaded in this func. */
    WSwan_GfxInit();

    WSwan_SoundInit();

    reset();

    return(1);
}

int app_main_wswan(uint8_t load_state, uint8_t start_paused)
{
    static dma_transfer_state_t last_dma_state = DMA_TRANSFER_STATE_HF;
    bool drawFrame;
    size_t offset;
    MDFN_Surface surface;
    odroid_gamepad_state_t joystick;
    odroid_dialog_choice_t options[] = {
        ODROID_DIALOG_CHOICE_LAST
    };

    if (start_paused) {
        common_emu_state.pause_after_frames = 2;
    } else {
        common_emu_state.pause_after_frames = 0;
    }
    common_emu_state.frame_time_10us = (uint16_t)(100000 / WSWAN_FPS + 0.5f);

    odroid_system_init(APPID_WSWAN, WSWAN_SAMPLE_RATE);
    odroid_system_emu_init(NULL, NULL, NULL);

    // Black background
    memset(framebuffer1, 0, sizeof(framebuffer1));
    memset(framebuffer2, 0, sizeof(framebuffer2));

    // Init Sound
    memset(audiobuffer_dma, 0, sizeof(audiobuffer_dma));
    HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint8_t *)audiobuffer_dma, AUDIO_BUFFER_LENGTH_DMA_WSWAN);

    load(ROM_DATA,ROM_DATA_LENGTH);

    WSwan_SetPixelFormat(16, // RGB565
            mono_pal_start, mono_pal_end);
    WSwan_SetSoundRate(WSWAN_SAMPLE_RATE);

    surface.pixels = framebuffer1;
    surface.width = 320;
    surface.height = 240;
    surface.pitch = 320;
    surface.depth = 16;
    while (1)
    {
        wdog_refresh();
        drawFrame = common_emu_frame_loop();
        odroid_input_read_gamepad(&joystick);
        common_emu_input_loop(&joystick, options);
        ws_input_read(&joystick);
        surface.pixels = lcd_get_active_buffer();
        while (!wsExecuteLineRotate(&surface, /*!drawFrame*/false, rotate));
        offset = (dma_state == DMA_TRANSFER_STATE_HF) ? 0 : (WSWAN_AUDIO_BUFFER_LENGTH);

        uint8_t volume = odroid_audio_volume_get();
        int32_t factor = volume_tbl[volume];
//        WSwan_SoundFlush(audio_samples_buf, &audio_samples_buf_size);

        size_t offset = (dma_state == DMA_TRANSFER_STATE_HF) ? 0 : WSWAN_AUDIO_BUFFER_LENGTH;
        if (volume == ODROID_AUDIO_VOLUME_MIN) {
            for (int i = 0; i < WSWAN_AUDIO_BUFFER_LENGTH; i++) {
                audiobuffer_dma[offset + i] = 0;
            }
        } else {
            for (int i = 0; i < WSWAN_AUDIO_BUFFER_LENGTH; i++) {
                int16_t sample = audio_samples_buf[i];
                audiobuffer_dma[offset + i] = (sample * factor) >> 8;
            }
        }

        common_ingame_overlay();
        lcd_swap();
        v30mz_timestamp = 0;
        if(!common_emu_state.skip_frames){
            for(uint8_t p = 0; p < common_emu_state.pause_frames + 1; p++) {
                while (dma_state == last_dma_state) {
                    cpumon_sleep();
                }
                last_dma_state = dma_state;
            }
        }
    }

    return 0;
}
