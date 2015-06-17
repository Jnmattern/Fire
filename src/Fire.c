#include <pebble.h>

#define SCREEN_WIDTH 144
#define SCREEN_HEIGHT 168

#define UPDATE_DELAY 100

//#define LOG_FPS

Window *window;
Layer *rootLayer, *mainLayer;
TextLayer *hourLayer;
uint8_t *fire;
GColor8 colors[256];
static char empty[] = "";

#ifdef LOG_FPS
static time_t ts_start, ts_end;
static uint16_t tms_start, tms_end;
static uint32_t t_start, t_end, dt = 0, n = 0;
#endif

void updateFire(void *data) {
  layer_mark_dirty(mainLayer);
}

void mainLayerUpdate(struct Layer *layer, GContext *ctx) {
  static int o = SCREEN_WIDTH * (SCREEN_HEIGHT-1); // 144*167 -> Last line
  int i, j = o, d, s, x;
  uint8_t index;
  uint16_t temp;
  GBitmap *fb;
  uint8_t *fbData;

#ifdef LOG_FPS
  time_ms(&ts_start, &tms_start);
  t_start = (uint32_t)1000 * (uint32_t)ts_start + (uint32_t)tms_start;
#endif


  fb = graphics_capture_frame_buffer_format(ctx, GBitmapFormat8Bit);
  fbData = gbitmap_get_data(fb);


  // Randomize a few sparkles
  for (i=0; i<15; i++) {
    int y = 108 + rand()%55;
    int x = 2 + rand()%(SCREEN_WIDTH-4);
    int offset = SCREEN_WIDTH*y + x;

    fire[offset] = 255;
    offset += SCREEN_WIDTH;
    fire[offset-1] = 255;
    fire[offset] = 255;
    fire[offset+1] = 255;
    offset += SCREEN_WIDTH;
    fire[offset] = 255;
  }

  // Replace Hour text in white with random temperature
  for (x = 10080; x<16271; x++) {
    if (fbData[x] == 0xff) fire[x] = 64 + rand()%64;
  }

  // Feed the fire on bottom line by randomizing some hot pixels
  for (i=0; i<SCREEN_WIDTH; i++) {
    fire[j + i] = 255*(rand()%2);
  }

  // Compute next frame, line by line starting from bottom up
  // Each pixel is the mean of itself, top, left and right pixels, and then decremented by one
  for (index = 0; index < SCREEN_HEIGHT-1 ; index++) {
    d = j;
    for (i = 0; i < SCREEN_WIDTH - 1; i++, d++) {
      temp = fire[d];
      temp += fire[d + 1];
      temp += fire[d - 1];
      s = d - SCREEN_WIDTH;
      temp += fire[s];
      temp >>= 2;

      if (temp > 1) {
        temp--;
      }

      fire[s] = temp;
      // Update framebuffer using color palette
      fbData[s] = colors[temp].argb;
    }
    j -= SCREEN_WIDTH;
  }

  graphics_release_frame_buffer(ctx, fb);
  app_timer_register(UPDATE_DELAY, updateFire, NULL);

#ifdef LOG_FPS
  n++;
  time_ms(&ts_end, &tms_end);
  t_end = (uint32_t)1000 * (uint32_t)ts_end + (uint32_t)tms_end;
  dt += t_end - t_start;

  if (!(n%30)) {
    uint32_t mtime = dt / n;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "mtime = %d", (int)mtime);
  }
#endif
}

void handle_tick(struct tm *tick_time, TimeUnits units_changed) {
  static char hourText[] = "00:00";

  snprintf(hourText, sizeof(hourText), "%.2d:%.2d", tick_time->tm_hour, tick_time->tm_min);

  text_layer_set_text(hourLayer, hourText);
}


void initColors() {
  // Init a 256 color palette.
  // Most are the same as there are aonly 2bit/color, but 256 values are needed to be realistic
  int i;
  for (i = 0; i < 32; i++) {
    // Black to Dark Blue for lower temperatures
    colors[i] = GColorFromRGB(0, 0, i<<2);

    // Dark Blue to Red
    colors[i + 32] = GColorFromRGB(i<<3, 0, 64-(i<<1));

    // Red to Yellow
    colors[i + 64] = GColorFromRGB(255, i<<3, 0);

    // Yellow to White for higher temperatures
    colors[i + 96] = GColorFromRGB(255, 255, i<<2);
    colors[i + 128] = GColorFromRGB(255, 255, 64 + (i<<2));
    colors[i + 160] = GColorFromRGB(255, 255, 128 + (i<<2));
    colors[i + 192] = GColorFromRGB(255, 255, 192 + i);
    colors[i + 224] = GColorFromRGB(255, 255, 224 + i);
  }
}

void handle_init(void) {
  srand(time(NULL));

  fire = malloc(SCREEN_WIDTH*SCREEN_HEIGHT + 1);

  initColors();
  
  window = window_create();
  window_stack_push(window, true);
  window_set_background_color(window, GColorBlack);
  rootLayer = window_get_root_layer(window);

  hourLayer = text_layer_create(GRect(0, 60, 144, 100));
  text_layer_set_background_color(hourLayer, GColorClear);
  text_layer_set_text_color(hourLayer, GColorWhite);
  text_layer_set_font(hourLayer, fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49));
  text_layer_set_text_alignment(hourLayer, GTextAlignmentCenter);
  layer_add_child(rootLayer, text_layer_get_layer(hourLayer));

  mainLayer = layer_create(layer_get_bounds(rootLayer));
  layer_set_update_proc(mainLayer, mainLayerUpdate);
  layer_add_child(rootLayer, mainLayer);

  tick_timer_service_subscribe(MINUTE_UNIT, handle_tick);
  app_timer_register(UPDATE_DELAY, updateFire, NULL);
}

void handle_deinit(void) {
  tick_timer_service_unsubscribe();
  layer_destroy(mainLayer);
  text_layer_destroy(hourLayer);
  window_destroy(window);
  free(fire);
}

int main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}

