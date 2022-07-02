#include <odroid_system.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <SDL2/SDL.h>

#include "porting.h"
#include "crc32.h"

#include "gw_lcd.h"
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

#define WSWAN_FPS 75
#define WSWAN_SAMPLE_RATE 48000
#define WSWAN_AUDIO_BUFFER_LENGTH (WSWAN_SAMPLE_RATE / WSWAN_FPS)
#define AUDIO_BUFFER_LENGTH_DMA_WSWAN (2 * WSWAN_AUDIO_BUFFER_LENGTH)

#define WIDTH 224
#define WSWAN_WIDTH (224)
#define WSWAN_HEIGHT (144)
#define XBUF_WIDTH (224)
#define XBUF_HEIGHT (144)


#undef printf
#define APP_ID 20

#define WIDTH    224
#define HEIGHT   144
#define BPP      16
#define SCALE    2
static MDFN_Surface *surf   = NULL;

typedef uint16_t pixel_t;

#define AUDIO_SAMPLE_RATE   (48000)
#define AUDIO_BUFFER_LENGTH (AUDIO_SAMPLE_RATE / 60)
static int16_t *audio_samples_buf     = NULL;
static int32_t audio_samples_buf_size = 0;

#define RETRO_60HZ_FPS         ((4.0 * MEDNAFEN_CORE_TIMING_FPS) / 5.0)
#define RETRO_60HZ_CYCLE_INDEX 4

// Use 60Hz for GB
#define AUDIO_BUFFER_LENGTH_GB (AUDIO_SAMPLE_RATE / 60)
#define AUDIO_BUFFER_LENGTH_DMA_GB ((2 * AUDIO_SAMPLE_RATE) / 60)

#define FB_INTERNAL_OFFSET (((XBUF_HEIGHT - current_height) / 2 + 16) * XBUF_WIDTH + (XBUF_WIDTH - current_width) / 2)
static uint8_t emulator_framebuffer_wswan[XBUF_WIDTH * XBUF_HEIGHT * 2];

extern unsigned char ROM_DATA[];
extern unsigned int cart_rom_len;

static bool saveSRAM = false;

// 3 pages
uint8_t state_save_buffer[192 * 1024];

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *fb_texture;

SDL_AudioSpec wanted;
void fill_audio(void *udata, Uint8 *stream, int len);

extern unsigned char cart_rom[];
extern unsigned int cart_rom_len;

static int framePerSecond=0;

static int current_height, current_width;

//The frames per second cap timer
int capTimer;


int 		wsc = 1;			/*color/mono*/

uint32		rom_size;
uint16 WSButtonStatus;
static uint32 SRAMSize;
static uint32 mono_pal_start = 0x000000;
static uint32 mono_pal_end   = 0xFFFFFF;


void odroid_display_force_refresh(void)
{
    // forceVideoRefresh = true;
}


void fill_audio(void *udata, Uint8 *stream, int len)
{

}

void osd_gfx_set_mode(int width, int height) {
//	init_color_pals();
    printf("current_width: %d \ncurrent_height: %d\n", width, height);
    current_width = width;
    current_height = height;
    SDL_SetWindowSize( window, WIDTH * SCALE, HEIGHT * SCALE);
    SDL_DestroyTexture(fb_texture);
    fb_texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING,
        current_width, current_height);

}

void pce_input_read(odroid_gamepad_state_t* out_state) {
/*    unsigned char rc = 0;
    if (out_state->values[ODROID_INPUT_LEFT])   rc |= JOY_LEFT;
    if (out_state->values[ODROID_INPUT_RIGHT])  rc |= JOY_RIGHT;
    if (out_state->values[ODROID_INPUT_UP])     rc |= JOY_UP;
    if (out_state->values[ODROID_INPUT_DOWN])   rc |= JOY_DOWN;
    if (out_state->values[ODROID_INPUT_A])      rc |= JOY_A;
    if (out_state->values[ODROID_INPUT_B])      rc |= JOY_B;
    if (out_state->values[ODROID_INPUT_START])  rc |= JOY_RUN;
    if (out_state->values[ODROID_INPUT_SELECT]) rc |= JOY_SELECT;*/
//    PCE.Joypad.regs[0] = rc;
}

int init_window(int width, int height)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        return 0;

    window = SDL_CreateWindow("emulator",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        width * SCALE, height * SCALE,
        0);
    if (!window)
        return 0;

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer)
        return 0;

    fb_texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING,
        width, height);
    if (!fb_texture)
        return 0;

    return 0;
}

static void netplay_callback(netplay_event_t event, void *arg)
{
    // Where we're going we don't need netplay!
}

static bool LoadStateStm(char *name)
{
    printf("Loading state from %s...\n", name);

	return 0;
}

static bool SaveStateStm(char *name)
{
    printf("Saving state to %s...\n", name);

	return 0;  
}

void pcm_submit(void)
{

}

static void reset(void)
{
   int		u0;

   v30mz_reset();				/* Reset CPU */
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
   rom_size      = pow_size+ (pow_size == 0);
   wsCartROM     = data;
   printf("rom_size %d\n",rom_size);

   /* This real_rom_size vs rom_size funny business 
    * is intended primarily for handling
    * WSR files. */
//   if(real_rom_size < rom_size)
//      memset(wsCartROM, 0xFF, rom_size - real_rom_size);

//   memcpy(wsCartROM + (rom_size - real_rom_size), data, size);

   memcpy(header, wsCartROM + rom_size - 10, 10);

   SRAMSize = 0;
   eeprom_size = 0;

   switch(header[5])
   {
      case 0x01: SRAMSize =   8 * 1024; break;
      case 0x02: SRAMSize =  32 * 1024; break;
      case 0x03: SRAMSize = 128 * 1024; break;
      case 0x04: SRAMSize = 256 * 1024; break; /* Dicing Knight!, Judgement Silver */
      case 0x05: SRAMSize = 512 * 1024; break; /* Wonder Gate */

      case 0x10: eeprom_size = 128; break;
      case 0x20: eeprom_size = 2 *1024; break;
      case 0x50: eeprom_size = 1024; break;
   }

   if((header[8] | (header[9] << 8)) == 0x8de1 && (header[0]==0x01)&&(header[2]==0x27)) /* Detective Conan */
   {
      /* WS cpu is using cache/pipeline or there's protected ROM bank where pointing CS */
//      wsCartROM[0xfffe8]=0xea;
//      wsCartROM[0xfffe9]=0x00;
//      wsCartROM[0xfffea]=0x00;
//      wsCartROM[0xfffeb]=0x00;
//      wsCartROM[0xfffec]=0x20;
   }

#if 0
   if(header[6] & 0x1)
      EmulatedWSwan.rotated = MDFN_ROTATE90;
#endif

   MDFNMP_Init(16384, (1 << 20) / 1024);

   v30mz_init(WSwan_readmem20, WSwan_writemem20, WSwan_readport, WSwan_writeport);
   WSwan_MemoryInit(MDFN_GetSettingB("wswan.language"), wsc, SRAMSize, false); /* EEPROM and SRAM are loaded in this func. */
   WSwan_GfxInit();

   WSwan_SoundInit();

   wsMakeTiles();

   reset();

   return(1);
}

void init(void)
{
    printf("init()\n");
    odroid_system_init(APP_ID, AUDIO_SAMPLE_RATE);
    odroid_system_emu_init(&LoadStateStm, &SaveStateStm, &netplay_callback);

    // Load ROM
    load(ROM_DATA,cart_rom_len);

    WSwan_SetPixelFormat(16, // RGB565
            mono_pal_start, mono_pal_end);
    WSwan_SetSoundRate(48000);
//    SetInput(0, "gamepad", &input_buf);
}

void pce_osd_gfx_blit(bool drawFrame) {
    static uint32_t lastFPSTime = 0;
    static uint32_t frames = 0;
    static int wantedTime = 1000 / WSWAN_FPS;

    if (!drawFrame) {
        return;
    }

    uint32_t currentTime = HAL_GetTick();
    uint32_t delta = currentTime - lastFPSTime;

    if (delta >= 1000) {
        framePerSecond = (10000 * frames) / delta;
        printf("FPS: %d.%d, frames %d, delta %d ms\n", framePerSecond / 10, framePerSecond % 10, frames, delta);
        frames = 0;
        lastFPSTime = currentTime;
    }


    SDL_UpdateTexture(fb_texture, NULL, surf->pixels, current_width * SCALE);
    SDL_RenderCopy(renderer, fb_texture, NULL, NULL);
    SDL_RenderPresent(renderer);

    //If frame finished early
    int frameTicks = SDL_GetTicks() - capTimer;
    if( frameTicks < wantedTime )
    {
        //Wait remaining time
        SDL_Delay( wantedTime - frameTicks );
    }
    frames++;
}


void odroid_input_read_gamepad_pce(odroid_gamepad_state_t* out_state)
{
#if 0
    SDL_Event event;
    static SDL_Event last_down_event;

    if (SDL_PollEvent(&event)) {
        if (event.type == SDL_KEYDOWN) {
            // printf("Press %d\n", event.key.keysym.sym);
            switch (event.key.keysym.sym) {
            case SDLK_x:
                out_state->values[ODROID_INPUT_A] = 1;
                break;
            case SDLK_z:
                out_state->values[ODROID_INPUT_B] = 1;
                break;
            case SDLK_LSHIFT:
                out_state->values[ODROID_INPUT_START] = 1;
                break;
            case SDLK_LCTRL:
                out_state->values[ODROID_INPUT_SELECT] = 1;
                break;
            case SDLK_UP:
                out_state->values[ODROID_INPUT_UP] = 1;
                break;
            case SDLK_DOWN:
                out_state->values[ODROID_INPUT_DOWN] = 1;
                break;
            case SDLK_LEFT:
                out_state->values[ODROID_INPUT_LEFT] = 1;
                break;
            case SDLK_RIGHT:
                out_state->values[ODROID_INPUT_RIGHT] = 1;
                break;
            case SDLK_ESCAPE:
                exit(1);
                break;
            default:
                break;
            }
            last_down_event = event;
        } else if (event.type == SDL_KEYUP) {
            // printf("Release %d\n", event.key.keysym.sym);
            switch (event.key.keysym.sym) {
            case SDLK_x:
                out_state->values[ODROID_INPUT_A] = 0;
                break;
            case SDLK_z:
                out_state->values[ODROID_INPUT_B] = 0;
                break;
            case SDLK_LSHIFT:
                out_state->values[ODROID_INPUT_START] = 0;
                break;
            case SDLK_LCTRL:
                out_state->values[ODROID_INPUT_SELECT] = 0;
                break;
            case SDLK_UP:
                out_state->values[ODROID_INPUT_UP] = 0;
                break;
            case SDLK_DOWN:
                out_state->values[ODROID_INPUT_DOWN] = 0;
                break;
            case SDLK_LEFT:
                out_state->values[ODROID_INPUT_LEFT] = 0;
                break;
            case SDLK_RIGHT:
                out_state->values[ODROID_INPUT_RIGHT] = 0;
                break;
            case SDLK_F1:
                if (last_down_event.key.keysym.sym == SDLK_F1)
                    SaveStateStm("save_pce.bin");
                break;
            case SDLK_F4:
                if (last_down_event.key.keysym.sym == SDLK_F4)
                    LoadStateStm("save_pce.bin");
                break;                
            default:
                break;
            }
        }
    }
#endif
}

void osd_log(int type, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);
}

int main(int argc, char *argv[])
{
    uint32_t SoundBufSize;
    surf = (MDFN_Surface*)calloc(1, sizeof(*surf));

    surf->width  = 320;
    surf->height = 240;
    surf->pitch  = 224;
    surf->depth  = 16;
    surf->pixels = (uint16_t*)calloc(1, 224 * 144 * sizeof(uint32_t));

    init_window(WIDTH, HEIGHT);

    /* Allocate an audio buffer of sufficient size
    * for the expected number of samples per frame
    * (size will be increased automatically if
    * configuration changes) */
    audio_samples_buf_size = ((int32_t)(48000 /
          50) + 1) << 1;
    audio_samples_buf      = (int16_t*)malloc(audio_samples_buf_size * sizeof(int16_t));

    osd_gfx_set_mode (224,144);

    init();
    odroid_gamepad_state_t joystick = {0};

    while (true)
    {

        //Start cap timer
        capTimer = SDL_GetTicks();

        while (!wsExecuteLine(surf, false));
        SoundBufSize = WSwan_SoundFlush(&audio_samples_buf, &audio_samples_buf_size);
//        printf("SoundBufSize %d audio_samples_buf_size %d\n",SoundBufSize,audio_samples_buf_size);
        odroid_input_read_gamepad_pce(&joystick);
        pce_input_read(&joystick);
        pce_osd_gfx_blit(true);
        v30mz_timestamp = 0;

    }

    SDL_Quit();

    return 0;
}
