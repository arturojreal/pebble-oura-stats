#include <pebble.h>
#include <string.h>

// =============================================================================
// OURA STATS WATCHFACE by Arturo J. Real
// =============================================================================
// Default Layout: Time (center top), Heart Rate (bottom left), 
//         Activity (bottom center), Sleep (bottom right)
// API: Oura Ring API v2
// =============================================================================

// Main window and layers
static Window *s_window;
static TextLayer *s_time_layer;
static TextLayer *s_date_layer;
static TextLayer *s_debug_layer;
static TextLayer *s_sample_indicator_layer;
static TextLayer *s_heart_rate_layer;
static TextLayer *s_heart_rate_label_layer;
static TextLayer *s_readiness_layer;
static TextLayer *s_readiness_label_layer;
static TextLayer *s_sleep_layer;
static TextLayer *s_sleep_label_layer;
static TextLayer *s_activity_layer;
static TextLayer *s_activity_label_layer;
static TextLayer *s_stress_layer;
static TextLayer *s_stress_label_layer;
static Layer *s_loading_layer;
static TextLayer *s_loading_text_layer; // Big bold header at top
static TextLayer *s_loading_logs_layer; // Multi-line logs underneath

// Persistent storage keys
#define PERSIST_KEY_SHOW_DEBUG          1001
#define PERSIST_KEY_REFRESH_FREQUENCY   1002
#define PERSIST_KEY_SHOW_LOADING        1003
#define PERSIST_KEY_SHOW_SECONDS        1004
#define PERSIST_KEY_COMPACT_TIME        1005
// Color and theme persistence keys
#define PERSIST_KEY_THEME_MODE          2001
#define PERSIST_KEY_CUSTOM_COLOR        2002
#define PERSIST_KEY_USE_EMOJI           2003
#define PERSIST_KEY_BG_COLOR            2100
#define PERSIST_KEY_TIME_COLOR          2101
#define PERSIST_KEY_DATE_COLOR          2102
#define PERSIST_KEY_READINESS_COLOR     2103
#define PERSIST_KEY_SLEEP_COLOR         2104
#define PERSIST_KEY_HEART_COLOR         2105
#define PERSIST_KEY_ACTIVITY_COLOR      2106
#define PERSIST_KEY_STRESS_COLOR        2107

// Data buffers
static char s_time_buffer[16];
static char s_date_buffer[32]; // Increased for long date formats like "Sunday, August 6"
static char s_sample_indicator_buffer[16];
static char s_heart_rate_buffer[16];
static char s_readiness_buffer[16];
static char s_sleep_buffer[16];
static char s_activity_buffer[16];
static char s_stress_buffer[16];

// Timer for debug message timeout
static AppTimer *s_debug_timer = NULL;
static bool s_real_data_received = false;
static bool s_loading = true;
static AppTimer *s_loading_hide_timer = NULL;
static bool s_show_loading = false; // Controls whether to show the loading overlay on refresh (configurable from JS)
static bool s_initial_startup = true; // Skip loading screen on first startup until JS sends preference
// becomes true when any real data or payload_complete received
static bool s_fetch_completed = false;
static bool s_show_debug = true; // Controls whether to accept and display debug logs
static int s_refresh_frequency_minutes = 30; // How often to refresh data
static int s_minutes_since_refresh = 0;      // Minute counter for refreshes
static bool s_show_seconds = false;          // Show seconds in time display
static bool s_compact_time = false;          // Compact time format (trim leading zero in 12h)

// Measurement layout configuration
// 0=readiness, 1=sleep, 2=heart_rate, 3=activity, 4=stress
static int s_layout_left = 0;    // Default: readiness
static int s_layout_middle = 1;  // Default: sleep
static int s_layout_right = 2;   // Default: heart_rate
static int s_layout_rows = 1;    // Default: 1 row
static int s_layout_row2_left = 3;   // Default: activity
static int s_layout_row2_right = 4;  // Default: stress

// Forward declarations
static void fetch_oura_data(void);
static void click_config_provider(void *context);
static void select_long_click_handler(ClickRecognizerRef recognizer, void *context);

// Date format configuration
// 0=MM-DD-YYYY, 1=DD-MM-YYYY, plus extended formats 2-11
// 2: "June 6, 2025" (%B %e, %Y)
// 3: "6 June 2025" (%e %B %Y)
// 4: "June 6" (%B %e)
// 5: "6 June" (%e %B)
// 6: "Jun 6, 2025" (%b %e, %Y)
// 7: "6 Jun 2025" (%e %b %Y)
// 8: "Jun 6" (%b %e)
// 9: "6 Jun" (%e %b)
// 10: "Friday, June 6" (%A, %B %e)
// 11: "Fri, Jun 6" (%a, %b %e)
static int s_date_format = 0;    // Default: MM-DD-YYYY

// Theme mode configuration
// 0=Dark Mode (white text on black), 1=Light Mode (black text on white), 2=Custom Color Mode (fixed color from config)
static int s_theme_mode = 2;     // Default: Custom Color Mode for test build

// Custom color system
static int s_custom_color_index = 0;  // Selected color index from config page
static const int s_color_palette_size = 64;

// Individual color settings
static bool s_use_emoji = false;
static int s_background_color = 0;    // Black
static int s_time_color = 63;         // White
static int s_date_color = 63;         // White
static int s_readiness_color = 63;    // White
static int s_sleep_color = 63;        // White
static int s_heart_rate_color = 63;   // White
static int s_activity_color = 63;     // White
static int s_stress_color = 63;       // White

// Forward declarations
static void update_debug_display(const char* message);
static void apply_theme_colors(void);
static bool is_light_color(GColor color);
static GColor get_palette_color(int index);
static void tick_handler(struct tm *tick_time, TimeUnits units_changed);
static void update_tick_subscription(void);
static void loading_layer_update_proc(Layer *layer, GContext *ctx);
static void hide_loading_overlay(void);
static void show_loading_overlay(void);

// Get color from palette by index
static GColor get_palette_color(int index) {
  switch (index % s_color_palette_size) {
    case 0: return GColorBlack;
    case 1: return GColorOxfordBlue;
    case 2: return GColorDukeBlue;
    case 3: return GColorBlue;
    case 4: return GColorDarkGreen;
    case 5: return GColorMidnightGreen;
    case 6: return GColorCobaltBlue;
    case 7: return GColorBlueMoon;
    case 8: return GColorIslamicGreen;
    case 9: return GColorJaegerGreen;
    case 10: return GColorTiffanyBlue;
    case 11: return GColorVividCerulean;
    case 12: return GColorGreen;
    case 13: return GColorMalachite;
    case 14: return GColorMediumSpringGreen;
    case 15: return GColorCyan;
    case 16: return GColorBulgarianRose;
    case 17: return GColorImperialPurple;
    case 18: return GColorIndigo;
    case 19: return GColorElectricUltramarine;
    case 20: return GColorArmyGreen;
    case 21: return GColorDarkGray;
    case 22: return GColorLiberty;
    case 23: return GColorVeryLightBlue;
    case 24: return GColorKellyGreen;
    case 25: return GColorMayGreen;
    case 26: return GColorCadetBlue;
    case 27: return GColorPictonBlue;
    case 28: return GColorBrightGreen;
    case 29: return GColorScreaminGreen;
    case 30: return GColorMediumAquamarine;
    case 31: return GColorElectricBlue;
    case 32: return GColorDarkCandyAppleRed;
    case 33: return GColorJazzberryJam;
    case 34: return GColorPurple;
    case 35: return GColorVividViolet;
    case 36: return GColorWindsorTan;
    case 37: return GColorRoseVale;
    case 38: return GColorPurpureus;
    case 39: return GColorLavenderIndigo;
    case 40: return GColorLimerick;
    case 41: return GColorBrass;
    case 42: return GColorLightGray;
    case 43: return GColorBabyBlueEyes;
    case 44: return GColorSpringBud;
    case 45: return GColorInchworm;
    case 46: return GColorMintGreen;
    case 47: return GColorCeleste;
    case 48: return GColorRed;
    case 49: return GColorFolly;
    case 50: return GColorFashionMagenta;
    case 51: return GColorMagenta;
    case 52: return GColorOrange;
    case 53: return GColorSunsetOrange;
    case 54: return GColorBrilliantRose;
    case 55: return GColorShockingPink;
    case 56: return GColorChromeYellow;
    case 57: return GColorRajah;
    case 58: return GColorMelon;
    case 59: return GColorRichBrilliantLavender;
    case 60: return GColorYellow;
    case 61: return GColorIcterine;
    case 62: return GColorPastelYellow;
    case 63: return GColorWhite;
    default: return GColorBlack;
  }
  
}

// Helper to (re)subscribe to tick timer based on show_seconds
static void update_tick_subscription(void) {
  tick_timer_service_unsubscribe();
  if (s_show_seconds) {
    tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
  } else {
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  }
}

// Smart contrast system - determines if a color is light or dark
static bool is_light_color(GColor color) {
  // Define light colors that need dark text for contrast
  return gcolor_equal(color, GColorWhite) ||
         gcolor_equal(color, GColorVeryLightBlue) ||
         gcolor_equal(color, GColorBabyBlueEyes) ||
         gcolor_equal(color, GColorLightGray) ||
         gcolor_equal(color, GColorPastelYellow) ||
         gcolor_equal(color, GColorIcterine) ||
         gcolor_equal(color, GColorYellow) ||
         gcolor_equal(color, GColorChromeYellow) ||
         gcolor_equal(color, GColorMelon) ||
         gcolor_equal(color, GColorRichBrilliantLavender) ||
         gcolor_equal(color, GColorCyan) ||
         gcolor_equal(color, GColorMintGreen) ||
         gcolor_equal(color, GColorCeleste) ||
         gcolor_equal(color, GColorTiffanyBlue) ||
         gcolor_equal(color, GColorMediumSpringGreen) ||
         gcolor_equal(color, GColorScreaminGreen) ||
         gcolor_equal(color, GColorInchworm) ||
         gcolor_equal(color, GColorSpringBud) ||
         gcolor_equal(color, GColorLimerick);
}

// Helper functions for theme colors
static GColor get_background_color(void) {
  if (s_theme_mode == 2) {
    // Custom color mode - use selected color from config
    return get_palette_color(s_custom_color_index);
  }
  return s_theme_mode == 1 ? GColorWhite : GColorBlack;
}

static GColor get_text_color(void) {
  if (s_theme_mode == 2) {
    // Custom color mode - smart contrast based on selected color
    GColor bg = get_palette_color(s_custom_color_index);
    return is_light_color(bg) ? GColorBlack : GColorWhite;
  }
  return s_theme_mode == 1 ? GColorBlack : GColorWhite;
}

// =============================================================================
// OURA DATA STRUCTURES (Based on Oura API v2)
// =============================================================================

typedef struct {
  int resting_heart_rate;     // bpm
  int hrv_score;              // ms
  bool data_available;
} OuraHeartRateData;

typedef struct {
  int readiness_score;        // 0-100
  int temperature_deviation;  // celsius * 100
  int recovery_index;         // 0-100
  bool data_available;
} OuraReadinessData;

typedef struct {
  int sleep_score;            // 0-100
  int total_sleep_time;       // minutes
  int deep_sleep_time;        // minutes
  bool data_available;
} OuraSleepData;

typedef struct {
  int activity_score;         // 0-100
  int steps;                  // step count
  int active_calories;        // calories
  bool data_available;
} OuraActivityData;

typedef struct {
  int stress_duration;        // seconds
  int stress_high_duration;   // seconds
  bool data_available;
} OuraStressData;

// Global Oura data
static OuraHeartRateData s_heart_rate_data = {0};
static OuraReadinessData s_readiness_data = {0};
static OuraSleepData s_sleep_data = {0};
static OuraActivityData s_activity_data = {0};
static OuraStressData s_stress_data = {0};
static bool s_using_sample_data = false;

// =============================================================================
// TIME MODULE
// =============================================================================

static void update_time_display() {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  // Build base format depending on seconds preference
  const char *fmt_24 = s_show_seconds ? "%H:%M:%S" : "%H:%M";
  const char *fmt_12 = s_show_seconds ? "%I:%M:%S" : "%I:%M";
  strftime(s_time_buffer, sizeof(s_time_buffer), clock_is_24h_style() ? fmt_24 : fmt_12, tick_time);

  // Apply compact time: trim leading zero for 12h style (e.g., 08:15 -> 8:15)
  if (s_compact_time && !clock_is_24h_style()) {
    if (s_time_buffer[0] == '0') {
      // Shift string left by one character
      size_t len = strlen(s_time_buffer);
      for (size_t i = 0; i < len; i++) {
        s_time_buffer[i] = s_time_buffer[i + 1];
      }
    }
  }

  text_layer_set_text(s_time_layer, s_time_buffer);
}

static void update_date_display() {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  switch (s_date_format) {
    case 1: // DD-MM-YYYY
      strftime(s_date_buffer, sizeof(s_date_buffer), "%d-%m-%Y", tick_time);
      break;
    case 2: // June 6, 2025
      strftime(s_date_buffer, sizeof(s_date_buffer), "%B %e, %Y", tick_time);
      break;
    case 3: // 6 June 2025
      strftime(s_date_buffer, sizeof(s_date_buffer), "%e %B %Y", tick_time);
      break;
    case 4: // June 6
      strftime(s_date_buffer, sizeof(s_date_buffer), "%B %e", tick_time);
      break;
    case 5: // 6 June
      strftime(s_date_buffer, sizeof(s_date_buffer), "%e %B", tick_time);
      break;
    case 6: // Jun 6, 2025
      strftime(s_date_buffer, sizeof(s_date_buffer), "%b %e, %Y", tick_time);
      break;
    case 7: // 6 Jun 2025
      strftime(s_date_buffer, sizeof(s_date_buffer), "%e %b %Y", tick_time);
      break;
    case 8: // Jun 6
      strftime(s_date_buffer, sizeof(s_date_buffer), "%b %e", tick_time);
      break;
    case 9: // 6 Jun
      strftime(s_date_buffer, sizeof(s_date_buffer), "%e %b", tick_time);
      break;
    case 10: // Friday, June 6
      strftime(s_date_buffer, sizeof(s_date_buffer), "%A, %B %e", tick_time);
      break;
    case 11: // Fri, Jun 6
      strftime(s_date_buffer, sizeof(s_date_buffer), "%a, %b %e", tick_time);
      break;
    case 12: // YYYY-MM-DD
      strftime(s_date_buffer, sizeof(s_date_buffer), "%Y-%m-%d", tick_time);
      break;
    case 0: // MM-DD-YYYY
    default:
      strftime(s_date_buffer, sizeof(s_date_buffer), "%m-%d-%Y", tick_time);
      break;
  }
  
  // Apply text and dynamically scale font to fit the available space
  text_layer_set_text(s_date_layer, s_date_buffer);

  // Dynamically choose the largest font that fits the date layer bounds
  // Candidate fonts ordered from largest to smallest
  static const char *k_date_font_keys[] = {
    FONT_KEY_GOTHIC_28_BOLD,
    FONT_KEY_GOTHIC_24_BOLD,
    FONT_KEY_GOTHIC_18_BOLD,
    FONT_KEY_GOTHIC_14
  };
  const int k_num_date_fonts = (int)(sizeof(k_date_font_keys) / sizeof(k_date_font_keys[0]));

  Layer *date_layer = text_layer_get_layer(s_date_layer);
  GRect bounds = layer_get_bounds(date_layer);

  // Slight horizontal padding to avoid edge clipping
  const int padding_w = 6;
  GRect test_bounds = GRect(padding_w, 0, bounds.size.w - 2 * padding_w, bounds.size.h);

  for (int i = 0; i < k_num_date_fonts; i++) {
    GFont test_font = fonts_get_system_font(k_date_font_keys[i]);
    GSize size = graphics_text_layout_get_content_size(
        s_date_buffer,
        test_font,
        test_bounds,
        GTextOverflowModeWordWrap,
        GTextAlignmentCenter);
    if (size.w <= test_bounds.size.w && size.h <= test_bounds.size.h) {
      text_layer_set_font(s_date_layer, test_font);
      break;
    }
    // If none fit, the last iteration will use the smallest font below
    if (i == k_num_date_fonts - 1) {
      text_layer_set_font(s_date_layer, test_font);
    }
  }
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time_display();
  update_date_display();
  
  // Minute-based refresh using configurable interval
  if (units_changed & MINUTE_UNIT) {
    s_minutes_since_refresh++;
    if (s_minutes_since_refresh >= s_refresh_frequency_minutes) {
      APP_LOG(APP_LOG_LEVEL_INFO, "Refreshing Oura data (every %d min)", s_refresh_frequency_minutes);
      fetch_oura_data();
      s_minutes_since_refresh = 0;
    }
  }
}

// =============================================================================
// DYNAMIC LAYOUT SYSTEM
// =============================================================================

// Dynamic layout positioning function
static void apply_dynamic_layout_positioning() {
  if (!s_window) return; // Safety check
  
  Layer *window_layer = window_get_root_layer(s_window);
  GRect bounds = layer_get_bounds(window_layer);
  
  if (s_layout_rows == 1) {
    // Use larger time font in 1-row mode
    text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
    // 1-row mode: Large complications, normal positioning
    int row1_y_value = bounds.size.h - 79;  // Original position
    int row1_y_emoji = bounds.size.h - 59;  // Original emoji position
    
    // Update row 1 positions (large size)
    layer_set_frame(text_layer_get_layer(s_readiness_layer), 
                    GRect(0, row1_y_value, bounds.size.w/3, 24));
    layer_set_frame(text_layer_get_layer(s_readiness_label_layer), 
                    GRect(0, row1_y_emoji, bounds.size.w/3, 24));
    
    layer_set_frame(text_layer_get_layer(s_sleep_layer), 
                    GRect(bounds.size.w/3, row1_y_value, bounds.size.w/3, 24));
    layer_set_frame(text_layer_get_layer(s_sleep_label_layer), 
                    GRect(bounds.size.w/3, row1_y_emoji, bounds.size.w/3, 24));
    
    layer_set_frame(text_layer_get_layer(s_heart_rate_layer), 
                    GRect(2*bounds.size.w/3, row1_y_value, bounds.size.w/3, 24));
    layer_set_frame(text_layer_get_layer(s_heart_rate_label_layer), 
                    GRect(2*bounds.size.w/3, row1_y_emoji, bounds.size.w/3, 24));
    
    // Set large fonts for 1-row mode
    text_layer_set_font(s_readiness_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_font(s_readiness_label_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_font(s_sleep_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_font(s_sleep_label_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_font(s_heart_rate_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_font(s_heart_rate_label_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    
    // Hide row 2
    layer_set_hidden(text_layer_get_layer(s_activity_layer), true);
    layer_set_hidden(text_layer_get_layer(s_activity_label_layer), true);
    layer_set_hidden(text_layer_get_layer(s_stress_layer), true);
    layer_set_hidden(text_layer_get_layer(s_stress_label_layer), true);
    
  } else if (s_layout_rows == 2) {
    // Use a slightly smaller time font in 2-row mode for breathing room
    text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_34_MEDIUM_NUMBERS));
    // 2-row mode: Shrink row 1, move it up, add row 2 below, all same size
    int row1_y_value = bounds.size.h - 90;  // Move row 1 up (moved down 1px)
    int row1_y_emoji = bounds.size.h - 75;  // Move row 1 emoji up (moved down 1px)
    int row2_y_value = bounds.size.h - 50;  // Row 2 position (moved down 1px)
    int row2_y_emoji = bounds.size.h - 35;  // Row 2 emoji position (moved down 1px)
    
    // Update row 1 positions (smaller size, moved up)
    layer_set_frame(text_layer_get_layer(s_readiness_layer), 
                    GRect(0, row1_y_value, bounds.size.w/3, 20));
    layer_set_frame(text_layer_get_layer(s_readiness_label_layer), 
                    GRect(0, row1_y_emoji, bounds.size.w/3, 20));
    
    layer_set_frame(text_layer_get_layer(s_sleep_layer), 
                    GRect(bounds.size.w/3, row1_y_value, bounds.size.w/3, 20));
    layer_set_frame(text_layer_get_layer(s_sleep_label_layer), 
                    GRect(bounds.size.w/3, row1_y_emoji, bounds.size.w/3, 20));
    
    layer_set_frame(text_layer_get_layer(s_heart_rate_layer), 
                    GRect(2*bounds.size.w/3, row1_y_value, bounds.size.w/3, 20));
    layer_set_frame(text_layer_get_layer(s_heart_rate_label_layer), 
                    GRect(2*bounds.size.w/3, row1_y_emoji, bounds.size.w/3, 20));
    
    // Update row 2 positions (same size as shrunken row 1)
    layer_set_frame(text_layer_get_layer(s_activity_layer), 
                    GRect(0, row2_y_value, bounds.size.w/2, 20));
    layer_set_frame(text_layer_get_layer(s_activity_label_layer), 
                    GRect(0, row2_y_emoji, bounds.size.w/2, 20));
    
    layer_set_frame(text_layer_get_layer(s_stress_layer), 
                    GRect(bounds.size.w/2, row2_y_value, bounds.size.w/2, 20));
    layer_set_frame(text_layer_get_layer(s_stress_label_layer), 
                    GRect(bounds.size.w/2, row2_y_emoji, bounds.size.w/2, 20));
    
    // Show row 2
    layer_set_hidden(text_layer_get_layer(s_activity_layer), false);
    layer_set_hidden(text_layer_get_layer(s_activity_label_layer), false);
    layer_set_hidden(text_layer_get_layer(s_stress_layer), false);
    layer_set_hidden(text_layer_get_layer(s_stress_label_layer), false);
    
    // Update fonts to smaller size for both rows
    text_layer_set_font(s_readiness_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_font(s_readiness_label_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_font(s_sleep_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_font(s_sleep_label_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_font(s_heart_rate_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_font(s_heart_rate_label_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_font(s_activity_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_font(s_activity_label_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_font(s_stress_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_font(s_stress_label_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  }
  
  APP_LOG(APP_LOG_LEVEL_INFO, "Applied dynamic layout positioning: %d rows", s_layout_rows);
}

// Helper function to get the correct layer and buffer for a measurement type at a position
static void update_measurement_at_position(int measurement_type, int position) {
  TextLayer *layer = NULL;
  TextLayer *label_layer = NULL;
  char *buffer = NULL;
  char *value_text = ""; // blank until fetch completes
  char *emoji_text = "";
  char value_buffer[16];
  
  // Determine which layer and buffer to use based on position
  switch (position) {
    case 0: // Left position (using sleep layers)
      layer = s_sleep_layer;
      label_layer = s_sleep_label_layer;
      buffer = s_sleep_buffer;
      break;
    case 1: // Middle position (using readiness layers)
      layer = s_readiness_layer;
      label_layer = s_readiness_label_layer;
      buffer = s_readiness_buffer;
      break;
    case 2: // Right position (using heart rate layers)
      layer = s_heart_rate_layer;
      label_layer = s_heart_rate_label_layer;
      buffer = s_heart_rate_buffer;
      break;
    case 3: // Row 2 Left position (using activity layers)
      layer = s_activity_layer;
      label_layer = s_activity_label_layer;
      buffer = s_activity_buffer;
      break;
    case 4: // Row 2 Right position (using stress layers)
      layer = s_stress_layer;
      label_layer = s_stress_label_layer;
      buffer = s_stress_buffer;
      break;
    default:
      return; // Invalid position
  }
  
  // Get the value and text label based on measurement type (using text for Pebble Steel compatibility)
  switch (measurement_type) {
    case 0: // Readiness
      emoji_text = "RDY";
      if (s_readiness_data.data_available) {
        snprintf(value_buffer, sizeof(value_buffer), "%d", s_readiness_data.readiness_score);
        value_text = value_buffer;
      } else if (s_fetch_completed) { value_text = "--"; }
      break;
    case 1: // Sleep
      emoji_text = "SLP";
      if (s_sleep_data.data_available) {
        snprintf(value_buffer, sizeof(value_buffer), "%d", s_sleep_data.sleep_score);
        value_text = value_buffer;
      } else if (s_fetch_completed) { value_text = "--"; }
      break;
    case 2: // Heart Rate
      emoji_text = "HR";
      if (s_heart_rate_data.data_available) {
        snprintf(value_buffer, sizeof(value_buffer), "%d", s_heart_rate_data.resting_heart_rate);
        value_text = value_buffer;
      } else if (s_fetch_completed) { value_text = "--"; }
      break;
    case 3: // Activity
      emoji_text = "ACT";
      if (s_activity_data.data_available && s_activity_data.activity_score > 0) {
        snprintf(value_buffer, sizeof(value_buffer), "%d", s_activity_data.activity_score);
        value_text = value_buffer;
      } else if (s_fetch_completed) { value_text = "--"; }
      break;
    case 4: // Stress
      emoji_text = "STR";
      if (s_stress_data.data_available) {
        int total_minutes = s_stress_data.stress_duration / 60;
        int hours = total_minutes / 60;
        int minutes = total_minutes % 60;
        if (hours > 0) {
          snprintf(value_buffer, sizeof(value_buffer), "%dh %dm", hours, minutes);
        } else {
          snprintf(value_buffer, sizeof(value_buffer), "%dm", minutes);
        }
        value_text = value_buffer;
      } else if (s_fetch_completed) { value_text = "--"; }
      break;
  }
  
  // Update the display
  snprintf(buffer, 16, "%s", value_text);
  text_layer_set_text(layer, buffer);
  
  // Dynamically scale the metric value font to fit within its cell
  if (layer) {
    static const char *k_metric_font_keys[] = {
      FONT_KEY_GOTHIC_24_BOLD,
      FONT_KEY_GOTHIC_18_BOLD,
      FONT_KEY_GOTHIC_14
    };
    const int k_num_metric_fonts = (int)(sizeof(k_metric_font_keys) / sizeof(k_metric_font_keys[0]));
    Layer *metric_layer = text_layer_get_layer(layer);
    GRect m_bounds = layer_get_bounds(metric_layer);
    const int m_padding_w = 2; // tighter than date
    GRect m_test = GRect(m_padding_w, 0, m_bounds.size.w - 2 * m_padding_w, m_bounds.size.h);
    for (int i = 0; i < k_num_metric_fonts; i++) {
      GFont f = fonts_get_system_font(k_metric_font_keys[i]);
      GSize sz = graphics_text_layout_get_content_size(
        buffer,
        f,
        m_test,
        GTextOverflowModeFill,
        GTextAlignmentCenter);
      if (sz.w <= m_test.size.w && sz.h <= m_test.size.h) {
        text_layer_set_font(layer, f);
        break;
      }
      if (i == k_num_metric_fonts - 1) {
        text_layer_set_font(layer, f);
      }
    }
  }
  
  // Update the label with emoji or text based on setting
  if (label_layer) {
    // Platform-aware emoji enablement: Aplite has the most restrictions.
    // Prefer text on Aplite even if s_use_emoji is true.
    bool can_use_emoji = s_use_emoji;
#if defined(PBL_PLATFORM_APLITE)
    can_use_emoji = false;
#endif
    if (can_use_emoji) {
      // Use emoji symbols for labels
      switch (measurement_type) {
        // IMPORTANT: Pebble emoji require Gothic fonts and Unicode escapes (\\UXXXXXXXX)
        // Readiness: Flexed Biceps U+1F4AA (may be unsupported on some firmwares)
        case 0: text_layer_set_text(label_layer, "\U0001F4AA"); break;
        // Sleep: Sleeping Face U+1F634 (supported range)
        case 1: text_layer_set_text(label_layer, "\U0001F634"); break;
        // Heart Rate: Heart U+2764 (without VS-16)
        case 2: text_layer_set_text(label_layer, "\U00002764"); break;
        // Activity: Fire U+1F525
        case 3: text_layer_set_text(label_layer, "\U0001F525"); break;
        // Stress: Face with Open Mouth and Cold Sweat U+1F630 (supported range)
        case 4: text_layer_set_text(label_layer, "\U0001F630"); break;
        default: text_layer_set_text(label_layer, emoji_text); break;
      }
    } else {
      // Use text labels
      text_layer_set_text(label_layer, emoji_text);
    }
  }
}

// Update all measurements according to current layout
static void update_all_measurements() {
  update_measurement_at_position(s_layout_left, 0);    // Left position
  update_measurement_at_position(s_layout_middle, 1);  // Middle position
  update_measurement_at_position(s_layout_right, 2);   // Right position
  
  // Update row 2 if visible
  if (!layer_get_hidden(text_layer_get_layer(s_activity_layer))) {
    update_measurement_at_position(s_layout_row2_left, 3);   // Row 2 Left position
    update_measurement_at_position(s_layout_row2_right, 4);  // Row 2 Right position
  }
}

// =============================================================================
// LEGACY DISPLAY FUNCTIONS (now call the dynamic system)
// =============================================================================

static void update_heart_rate_display() {
  update_all_measurements();
}

static void update_readiness_display() {
  update_all_measurements();
}

static void update_sleep_display() {
  update_all_measurements();
}

static void update_activity_display() {
  // Use flexible layout system for activity display
  update_all_measurements();
}

static void update_stress_display() {
  // Use flexible layout system for stress display
  update_all_measurements();
}

// =============================================================================
// OURA API MODULE (Placeholder for future implementation)
// =============================================================================

static void request_oura_data() {
  // Request fresh data from JavaScript component
  // Show loading overlay if user has enabled it (allow on any refresh after initial startup)
  if (s_show_loading && !s_initial_startup) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Showing loading overlay (user enabled, not initial startup)");
    show_loading_overlay();
  } else if (s_show_loading && s_initial_startup) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Loading overlay enabled but skipping during initial startup");
  } else {
    APP_LOG(APP_LOG_LEVEL_INFO, "Loading overlay disabled by user (show_loading: %d)", s_show_loading);
  }
  
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_uint8(iter, MESSAGE_KEY_request_data, 1);
  app_message_outbox_send();
  
  APP_LOG(APP_LOG_LEVEL_INFO, "Requested Oura data from phone");
}

// =============================================================================
// DEBUG STATUS DISPLAY
// =============================================================================

// Timer callback to clear debug message
static void debug_timer_callback(void *data) {
  s_debug_timer = NULL;
  update_debug_display(NULL);  // Clear debug message
}

static void update_debug_display(const char* message) {
  // Respect user preference for debug visibility
  if (!s_show_debug) {
    return;
  }
  // Route debug logs to loading overlay only. Do not show on watchface.
  if (!s_loading) {
    return; // Suppress debug logs once watchface is visible
  }
 
  if (message && s_loading_logs_layer) {
    // Append message to multi-line buffer (with newline if needed)
    static char s_loading_logs_buffer[512];
    static bool initialized = false;
    if (!initialized) { s_loading_logs_buffer[0] = '\0'; initialized = true; }
 
    size_t cur_len = strlen(s_loading_logs_buffer);
    size_t msg_len = strlen(message);
    size_t need = (cur_len ? 1 : 0) + msg_len + 1; // newline + message + NUL
    if (cur_len + need < sizeof(s_loading_logs_buffer)) {
      if (cur_len) { s_loading_logs_buffer[cur_len++] = '\n'; s_loading_logs_buffer[cur_len] = '\0'; }
      strncat(s_loading_logs_buffer, message, sizeof(s_loading_logs_buffer) - cur_len - 1);
    } else {
      // If full, drop oldest by finding first '\n'
      char *first_nl = strchr(s_loading_logs_buffer, '\n');
      if (first_nl) {
        size_t remain = strlen(first_nl + 1);
        memmove(s_loading_logs_buffer, first_nl + 1, remain + 1);
      } else {
        // Too long without newline; reset buffer
        s_loading_logs_buffer[0] = '\0';
      }
      // Retry append after trimming
      cur_len = strlen(s_loading_logs_buffer);
      if (cur_len) { s_loading_logs_buffer[cur_len++] = '\n'; s_loading_logs_buffer[cur_len] = '\0'; }
      strncat(s_loading_logs_buffer, message, sizeof(s_loading_logs_buffer) - cur_len - 1);
    }
 
    text_layer_set_text(s_loading_logs_layer, s_loading_logs_buffer);
  }
}

// =============================================================================
// SAMPLE DATA INDICATOR
// =============================================================================

static void update_sample_indicator() {
  if (s_using_sample_data) {
    snprintf(s_sample_indicator_buffer, sizeof(s_sample_indicator_buffer), "This is sample data, not your data!");
  } else {
    s_sample_indicator_buffer[0] = '\0';  // Clear the buffer
  }
  text_layer_set_text(s_sample_indicator_layer, s_sample_indicator_buffer);
}

static void fetch_oura_data() {
  // Do NOT set any sample data. Leave fields blank until fetched.
  s_using_sample_data = false;
  update_time_display();
  update_date_display();
  update_all_measurements();
  update_sample_indicator();
  
  // Request real data from phone
  update_debug_display("Requesting real data...");
  request_oura_data();
}

// =============================================================================
// UI LAYOUT AND WINDOW MANAGEMENT
// =============================================================================

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  // Set background color based on theme
  window_set_background_color(window, get_background_color());
  
  // Time display (center top) - moved up 10 pixels for better positioning
  s_time_layer = text_layer_create(
      GRect(0, PBL_IF_ROUND_ELSE(5, 0), bounds.size.w, 50));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, get_text_color());
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
  
  // Date display (below time) - 20% bigger font for even better readability
  s_date_layer = text_layer_create(
      GRect(0, PBL_IF_ROUND_ELSE(50, 45), bounds.size.w, 40)); // Increased height for long dates
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, get_text_color());
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));
  
  // Debug status (moved down to accommodate date)
  s_debug_layer = text_layer_create(
      GRect(0, PBL_IF_ROUND_ELSE(85, 80), bounds.size.w, 15));
  text_layer_set_background_color(s_debug_layer, GColorClear);
  text_layer_set_text_color(s_debug_layer, get_text_color());
  text_layer_set_font(s_debug_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_debug_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_debug_layer));
  
  // Sample indicator (moved down to accommodate date)
  s_sample_indicator_layer = text_layer_create(
      GRect(0, PBL_IF_ROUND_ELSE(105, 100), bounds.size.w, 20));
  text_layer_set_background_color(s_sample_indicator_layer, GColorClear);
  text_layer_set_text_color(s_sample_indicator_layer, get_text_color());
  text_layer_set_font(s_sample_indicator_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_sample_indicator_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_sample_indicator_layer));
  
  // Configure input when window is ready
  window_set_click_config_provider(window, click_config_provider);
  
  // Top row for readiness, sleep, heart rate (3 columns) - moved down 2px, size up 20%
  // Readiness (top row left)
  s_readiness_layer = text_layer_create(
      GRect(0, bounds.size.h - 79, bounds.size.w/3, 24));
  text_layer_set_background_color(s_readiness_layer, GColorClear);
  text_layer_set_text_color(s_readiness_layer, get_text_color());
  text_layer_set_font(s_readiness_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_readiness_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_readiness_layer));

  // Readiness Label (emoji)
  s_readiness_label_layer = text_layer_create(
      GRect(0, bounds.size.h - 59, bounds.size.w/3, 24));
  text_layer_set_background_color(s_readiness_label_layer, GColorClear);
  text_layer_set_text_color(s_readiness_label_layer, get_text_color());
  text_layer_set_font(s_readiness_label_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_readiness_label_layer, GTextAlignmentCenter);
  text_layer_set_text(s_readiness_label_layer, "🎉");
  layer_add_child(window_layer, text_layer_get_layer(s_readiness_label_layer));

  // Sleep (top row middle)
  s_sleep_layer = text_layer_create(
      GRect(bounds.size.w/3, bounds.size.h - 79, bounds.size.w/3, 24));
  text_layer_set_background_color(s_sleep_layer, GColorClear);
  text_layer_set_text_color(s_sleep_layer, get_text_color());
  text_layer_set_font(s_sleep_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_sleep_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_sleep_layer));

  // Sleep Label (emoji)
  s_sleep_label_layer = text_layer_create(
      GRect(bounds.size.w/3, bounds.size.h - 59, bounds.size.w/3, 24));
  text_layer_set_background_color(s_sleep_label_layer, GColorClear);
  text_layer_set_text_color(s_sleep_label_layer, get_text_color());
  text_layer_set_font(s_sleep_label_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_sleep_label_layer, GTextAlignmentCenter);
  text_layer_set_text(s_sleep_label_layer, "😴");
  layer_add_child(window_layer, text_layer_get_layer(s_sleep_label_layer));

  // Heart Rate (top row right)
  s_heart_rate_layer = text_layer_create(
      GRect(2*bounds.size.w/3, bounds.size.h - 79, bounds.size.w/3, 24));
  text_layer_set_background_color(s_heart_rate_layer, GColorClear);
  text_layer_set_text_color(s_heart_rate_layer, get_text_color());
  text_layer_set_font(s_heart_rate_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_heart_rate_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_heart_rate_layer));

  // Heart Rate Label (emoji)
  s_heart_rate_label_layer = text_layer_create(
      GRect(2*bounds.size.w/3, bounds.size.h - 59, bounds.size.w/3, 24));
  text_layer_set_background_color(s_heart_rate_label_layer, GColorClear);
  text_layer_set_text_color(s_heart_rate_label_layer, get_text_color());
  text_layer_set_font(s_heart_rate_label_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_heart_rate_label_layer, GTextAlignmentCenter);
  text_layer_set_text(s_heart_rate_label_layer, "❤");
  layer_add_child(window_layer, text_layer_get_layer(s_heart_rate_label_layer));

  // Bottom row for activity and stress (2 columns) - HIDDEN BY DEFAULT (1-row layout)
  // Activity (bottom row left)
  s_activity_layer = text_layer_create(
      GRect(0, bounds.size.h - 41, bounds.size.w/2, 20));
  text_layer_set_background_color(s_activity_layer, GColorClear);
  text_layer_set_text_color(s_activity_layer, get_text_color());
  text_layer_set_font(s_activity_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_activity_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_activity_layer));
  layer_set_hidden(text_layer_get_layer(s_activity_layer), true); // Hidden by default

  // Activity Label (emoji)
  s_activity_label_layer = text_layer_create(
      GRect(0, bounds.size.h - 21, bounds.size.w/2, 20));
  text_layer_set_background_color(s_activity_label_layer, GColorClear);
  text_layer_set_text_color(s_activity_label_layer, get_text_color());
  text_layer_set_font(s_activity_label_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_activity_label_layer, GTextAlignmentCenter);
  text_layer_set_text(s_activity_label_layer, "🔥");
  layer_add_child(window_layer, text_layer_get_layer(s_activity_label_layer));
  layer_set_hidden(text_layer_get_layer(s_activity_label_layer), true); // Hidden by default

  // Stress (bottom row right)
  s_stress_layer = text_layer_create(
      GRect(bounds.size.w/2, bounds.size.h - 41, bounds.size.w/2, 20));
  text_layer_set_background_color(s_stress_layer, GColorClear);
  text_layer_set_text_color(s_stress_layer, get_text_color());
  text_layer_set_font(s_stress_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_stress_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_stress_layer));
  layer_set_hidden(text_layer_get_layer(s_stress_layer), true); // Hidden by default

  // Stress Label (emoji)
  s_stress_label_layer = text_layer_create(
      GRect(bounds.size.w/2, bounds.size.h - 21, bounds.size.w/2, 20));
  text_layer_set_background_color(s_stress_label_layer, GColorClear);
  text_layer_set_text_color(s_stress_label_layer, get_text_color());
  text_layer_set_font(s_stress_label_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_stress_label_layer, GTextAlignmentCenter);
  text_layer_set_text(s_stress_label_layer, "STR");
  layer_add_child(window_layer, text_layer_get_layer(s_stress_label_layer));
  layer_set_hidden(text_layer_get_layer(s_stress_label_layer), true); // Hidden by default

  // Loading overlay (top-most): deep green background with "Loading..." header and logs below
  s_loading_layer = layer_create(bounds);
  layer_set_update_proc(s_loading_layer, loading_layer_update_proc);
  layer_add_child(window_layer, s_loading_layer);
  
  // Big bold title at top
  s_loading_text_layer = text_layer_create(GRect(0, 4, bounds.size.w, 28));
  text_layer_set_background_color(s_loading_text_layer, GColorClear);
  text_layer_set_text_color(s_loading_text_layer, GColorWhite);
  text_layer_set_font(s_loading_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_loading_text_layer, GTextAlignmentCenter);
  text_layer_set_text(s_loading_text_layer, "Loading...");
  layer_add_child(window_layer, text_layer_get_layer(s_loading_text_layer));

  // Multi-line debug logs under the title
  int logs_y = 4 + 28 + 4;
  s_loading_logs_layer = text_layer_create(GRect(4, logs_y, bounds.size.w - 8, bounds.size.h - logs_y - 4));
  text_layer_set_background_color(s_loading_logs_layer, GColorClear);
  text_layer_set_text_color(s_loading_logs_layer, GColorWhite);
  text_layer_set_font(s_loading_logs_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_loading_logs_layer, GTextAlignmentLeft);
  text_layer_set_overflow_mode(s_loading_logs_layer, GTextOverflowModeWordWrap);
  text_layer_set_text(s_loading_logs_layer, "");
  layer_add_child(window_layer, text_layer_get_layer(s_loading_logs_layer));
  
  // Initialize loading overlay as hidden by default, but don't force s_loading state
  // The loading screen will be shown/hidden based on user preference and data requests
  layer_set_hidden(s_loading_layer, true);
  layer_set_hidden(text_layer_get_layer(s_loading_text_layer), true);
  layer_set_hidden(text_layer_get_layer(s_loading_logs_layer), true);

  // We no longer want debug text on the main watchface; hide it permanently
  if (s_debug_layer) {
    layer_set_hidden(text_layer_get_layer(s_debug_layer), true);
  }
  
  // Apply initial dynamic layout positioning (defaults to 1-row layout)
  apply_dynamic_layout_positioning();

  // Ensure initial time/date render uses scaled fonts before first tick
  update_time_display();
  update_date_display();
}

static void loading_layer_update_proc(Layer *layer, GContext *ctx) {
  // Oxford Blue background for loading overlay (high contrast with white text)
  graphics_context_set_fill_color(ctx, GColorOxfordBlue);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}

static void show_loading_overlay(void) {
  // Make loading UI visible and clear logs area
  s_loading = true;
  if (s_loading_layer) layer_set_hidden(s_loading_layer, false);
  if (s_loading_text_layer) layer_set_hidden(text_layer_get_layer(s_loading_text_layer), false);
  if (s_loading_logs_layer) {
    text_layer_set_text(s_loading_logs_layer, "");
    layer_set_hidden(text_layer_get_layer(s_loading_logs_layer), false);
  }
}

static void hide_loading_overlay(void) {
  if (s_loading) {
    s_loading = false;
    if (s_loading_layer) layer_set_hidden(s_loading_layer, true);
    if (s_loading_text_layer) layer_set_hidden(text_layer_get_layer(s_loading_text_layer), true);
    if (s_loading_logs_layer) layer_set_hidden(text_layer_get_layer(s_loading_logs_layer), true);
  }
}

static void window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_date_layer);
  text_layer_destroy(s_debug_layer);
  text_layer_destroy(s_sample_indicator_layer);
  text_layer_destroy(s_heart_rate_layer);
  text_layer_destroy(s_heart_rate_label_layer);
  text_layer_destroy(s_readiness_layer);
  text_layer_destroy(s_readiness_label_layer);
  text_layer_destroy(s_sleep_layer);
  text_layer_destroy(s_sleep_label_layer);
  text_layer_destroy(s_activity_layer);
  text_layer_destroy(s_activity_label_layer);
  text_layer_destroy(s_stress_layer);
  text_layer_destroy(s_stress_label_layer);
  if (s_loading_text_layer) text_layer_destroy(s_loading_text_layer);
  if (s_loading_layer) layer_destroy(s_loading_layer);
  if (s_loading_hide_timer) { app_timer_cancel(s_loading_hide_timer); s_loading_hide_timer = NULL; }
}

// =============================================================================
// INPUT: MANUAL REFRESH (Long-press SELECT)
// =============================================================================

static void select_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Allow overlay on manual trigger even if it's the first run
  s_initial_startup = false;
  s_minutes_since_refresh = 0;
  APP_LOG(APP_LOG_LEVEL_INFO, "SELECT long-click detected: forcing refresh");
  update_debug_display("Manual refresh requested...");
  vibes_short_pulse();
  fetch_oura_data();
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Alternate manual refresh on single press
  s_initial_startup = false;
  s_minutes_since_refresh = 0;
  APP_LOG(APP_LOG_LEVEL_INFO, "SELECT single-click detected: forcing refresh");
  update_debug_display("Manual refresh requested...");
  vibes_short_pulse();
  fetch_oura_data();
}

static void click_config_provider(void *context) {
  // Long-press SELECT (700ms) to force a refresh
  window_long_click_subscribe(BUTTON_ID_SELECT, 700, select_long_click_handler, NULL);
  // Single-click SELECT to force a refresh as well
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
}

// =============================================================================
// APPMESSAGE HANDLERS (Communication with JavaScript)
// =============================================================================

// Helper: convert a tuple that may be an int or a numeric string to int
static int tuple_to_int(const Tuple *t, int fallback) {
  if (!t || !t->value) {
    return fallback;
  }
  // If we received a C-string, parse it (config page sometimes sends numeric strings)
  if (t->type == TUPLE_CSTRING && t->value->cstring) {
    const char *s = t->value->cstring;
    // Use atoi; if non-numeric, it returns 0, which is acceptable for our palette indices
    return atoi(s);
  }
  // Otherwise, prefer int32
  return t->value->int32;
}

// Helper: convert tuple to boolean, accepting 1/0 and "true"/"false" strings
static bool tuple_to_bool(const Tuple *t, bool fallback) {
  if (!t || !t->value) {
    return fallback;
  }
  if (t->type == TUPLE_CSTRING && t->value->cstring) {
    const char *s = t->value->cstring;
    // Check first character to avoid heavy libc calls
    if (s[0] == '1' || s[0] == 't' || s[0] == 'T' || s[0] == 'y' || s[0] == 'Y') return true;
    if (s[0] == '0' || s[0] == 'f' || s[0] == 'F' || s[0] == 'n' || s[0] == 'N') return false;
    // Fallback to atoi
    return atoi(s) != 0;
  }
  return t->value->int32 != 0;
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Message received from phone");
  
  // Handle debug status messages
  Tuple *debug_tuple = dict_find(iterator, MESSAGE_KEY_debug_status);
  if (debug_tuple) {
    update_debug_display(debug_tuple->value->cstring);
  }
  
  // Process heart rate data
  Tuple *heart_rate_tuple = dict_find(iterator, MESSAGE_KEY_heart_rate);
  if (heart_rate_tuple) {
    Tuple *hr_value = dict_find(iterator, MESSAGE_KEY_resting_heart_rate);
    Tuple *hrv_value = dict_find(iterator, MESSAGE_KEY_hrv_score);
    Tuple *hr_available = dict_find(iterator, MESSAGE_KEY_data_available);
    
    if (hr_value && hrv_value && hr_available) {
      s_heart_rate_data.resting_heart_rate = hr_value->value->int32;
      s_heart_rate_data.hrv_score = hrv_value->value->int32;
      s_heart_rate_data.data_available = hr_available->value->int32 == 1;
      update_heart_rate_display();
      APP_LOG(APP_LOG_LEVEL_INFO, "Heart rate updated: %d bpm", s_heart_rate_data.resting_heart_rate);
    }
  }
  
  // Process readiness data
  Tuple *readiness_tuple = dict_find(iterator, MESSAGE_KEY_readiness);
  if (readiness_tuple) {
    Tuple *score_value = dict_find(iterator, MESSAGE_KEY_readiness_score);
    Tuple *temp_deviation_value = dict_find(iterator, MESSAGE_KEY_temperature_deviation);
    Tuple *recovery_value = dict_find(iterator, MESSAGE_KEY_recovery_index);
    Tuple *rdy_available = dict_find(iterator, MESSAGE_KEY_data_available);
    
    if (score_value && temp_deviation_value && recovery_value && rdy_available) {
      s_readiness_data.readiness_score = score_value->value->int32;
      s_readiness_data.temperature_deviation = temp_deviation_value->value->int32;
      s_readiness_data.recovery_index = recovery_value->value->int32;
      s_readiness_data.data_available = rdy_available->value->int32 == 1;
      update_readiness_display();
      APP_LOG(APP_LOG_LEVEL_INFO, "Readiness updated: %d score, recovery: %d", 
              s_readiness_data.readiness_score, s_readiness_data.recovery_index);
    }
  }
  
  // Process sleep data
  Tuple *sleep_tuple = dict_find(iterator, MESSAGE_KEY_sleep);
  if (sleep_tuple) {
    Tuple *sleep_score_value = dict_find(iterator, MESSAGE_KEY_sleep_score);
    Tuple *total_sleep_value = dict_find(iterator, MESSAGE_KEY_total_sleep_time);
    Tuple *deep_sleep_value = dict_find(iterator, MESSAGE_KEY_deep_sleep_time);
    Tuple *sleep_available = dict_find(iterator, MESSAGE_KEY_data_available);
    
    if (sleep_score_value && total_sleep_value && deep_sleep_value && sleep_available) {
      s_sleep_data.sleep_score = sleep_score_value->value->int32;
      s_sleep_data.total_sleep_time = total_sleep_value->value->int32;
      s_sleep_data.deep_sleep_time = deep_sleep_value->value->int32;
      s_sleep_data.data_available = sleep_available->value->int32 == 1;
      update_sleep_display();
      APP_LOG(APP_LOG_LEVEL_INFO, "Sleep updated: %d score, %d min total", 
              s_sleep_data.sleep_score, s_sleep_data.total_sleep_time);
    }
  }
  
  // Process activity data (use struct + display helper for "--" when unavailable)
  Tuple *activity_score_tuple = dict_find(iterator, MESSAGE_KEY_activity_score);
  if (activity_score_tuple) {
    int activity_score = activity_score_tuple->value->int32;
    s_activity_data.activity_score = activity_score;
    // Optional extras if present
    Tuple *active_calories_tuple = dict_find(iterator, MESSAGE_KEY_active_calories);
    if (active_calories_tuple) {
      s_activity_data.active_calories = active_calories_tuple->value->int32;
    }
    Tuple *steps_tuple = dict_find(iterator, MESSAGE_KEY_steps);
    if (steps_tuple) {
      s_activity_data.steps = steps_tuple->value->int32;
    }
    // Infer availability: any positive field indicates availability
    s_activity_data.data_available = (s_activity_data.activity_score > 0) ||
                                     (s_activity_data.active_calories > 0) ||
                                     (s_activity_data.steps > 0);
    update_activity_display();
    APP_LOG(APP_LOG_LEVEL_INFO, "Activity updated: %d score (available=%d)", activity_score, s_activity_data.data_available);
  }
  
  // Process stress data (use struct + display helper for "--" when 0/unavailable)
  Tuple *stress_duration_tuple = dict_find(iterator, MESSAGE_KEY_stress_duration);
  if (stress_duration_tuple) {
    int stress_seconds = stress_duration_tuple->value->int32;
    s_stress_data.stress_duration = stress_seconds;
    Tuple *stress_high_tuple = dict_find(iterator, MESSAGE_KEY_stress_high_duration);
    if (stress_high_tuple) {
      s_stress_data.stress_high_duration = stress_high_tuple->value->int32;
    }
    // Consider stress available if we received the tuple, even if 0 seconds
    s_stress_data.data_available = true;
    update_stress_display();
    APP_LOG(APP_LOG_LEVEL_INFO, "Stress updated: %ds (available=%d)", stress_seconds, s_stress_data.data_available);
  }
  
  // Process layout configuration
  Tuple *layout_left_tuple = dict_find(iterator, MESSAGE_KEY_layout_left);
  Tuple *layout_middle_tuple = dict_find(iterator, MESSAGE_KEY_layout_middle);
  Tuple *layout_right_tuple = dict_find(iterator, MESSAGE_KEY_layout_right);
  
  if (layout_left_tuple && layout_middle_tuple && layout_right_tuple) {
    s_layout_left = layout_left_tuple->value->int32;
    s_layout_middle = layout_middle_tuple->value->int32;
    s_layout_right = layout_right_tuple->value->int32;
    
    APP_LOG(APP_LOG_LEVEL_INFO, "Layout config updated: L=%d M=%d R=%d", 
            s_layout_left, s_layout_middle, s_layout_right);
    
    // Update all displays to reflect new layout
    update_heart_rate_display();
    update_readiness_display();
    update_sleep_display();
  }
  
  // Process flexible layout configuration (row 2 support)
  Tuple *layout_rows_tuple = dict_find(iterator, MESSAGE_KEY_layout_rows);
  Tuple *row2_left_tuple = dict_find(iterator, MESSAGE_KEY_row2_left);
  Tuple *row2_right_tuple = dict_find(iterator, MESSAGE_KEY_row2_right);
  
  if (layout_rows_tuple) {
    s_layout_rows = layout_rows_tuple->value->int32;
    APP_LOG(APP_LOG_LEVEL_INFO, "Layout rows updated: %d", s_layout_rows);
    
    if (row2_left_tuple && row2_right_tuple) {
      s_layout_row2_left = row2_left_tuple->value->int32;
      s_layout_row2_right = row2_right_tuple->value->int32;
      
      APP_LOG(APP_LOG_LEVEL_INFO, "Row 2 config updated: L=%d R=%d", 
              s_layout_row2_left, s_layout_row2_right);
    }
    
    // Apply dynamic layout positioning based on row count
    apply_dynamic_layout_positioning();
    
    // Update all displays to reflect new layout (including row positioning)
    update_heart_rate_display();
    update_readiness_display();
    update_sleep_display();
    update_activity_display();
    update_stress_display();
  }
  
  // Process individual color configuration
  Tuple *use_emoji_tuple = dict_find(iterator, MESSAGE_KEY_use_emoji);
  if (use_emoji_tuple) {
    s_use_emoji = tuple_to_bool(use_emoji_tuple, s_use_emoji);
    APP_LOG(APP_LOG_LEVEL_INFO, "Emoji mode updated: %s", s_use_emoji ? "enabled" : "disabled");
    persist_write_bool(PERSIST_KEY_USE_EMOJI, s_use_emoji);
    // Immediately refresh labels to reflect emoji/text change
    update_all_measurements();
  }
  
  Tuple *background_color_tuple = dict_find(iterator, MESSAGE_KEY_background_color);
  if (background_color_tuple) {
    s_background_color = tuple_to_int(background_color_tuple, s_background_color);
    APP_LOG(APP_LOG_LEVEL_INFO, "Background color updated: %d", s_background_color);
    persist_write_int(PERSIST_KEY_BG_COLOR, s_background_color);
  }
  
  Tuple *time_color_tuple = dict_find(iterator, MESSAGE_KEY_time_color);
  if (time_color_tuple) {
    s_time_color = tuple_to_int(time_color_tuple, s_time_color);
    APP_LOG(APP_LOG_LEVEL_INFO, "Time color updated: %d", s_time_color);
    persist_write_int(PERSIST_KEY_TIME_COLOR, s_time_color);
  }
  
  Tuple *date_color_tuple = dict_find(iterator, MESSAGE_KEY_date_color);
  if (date_color_tuple) {
    s_date_color = tuple_to_int(date_color_tuple, s_date_color);
    APP_LOG(APP_LOG_LEVEL_INFO, "Date color updated: %d", s_date_color);
    persist_write_int(PERSIST_KEY_DATE_COLOR, s_date_color);
  }
  
  Tuple *readiness_color_tuple = dict_find(iterator, MESSAGE_KEY_readiness_color);
  if (readiness_color_tuple) {
    s_readiness_color = tuple_to_int(readiness_color_tuple, s_readiness_color);
    APP_LOG(APP_LOG_LEVEL_INFO, "Readiness color updated: %d", s_readiness_color);
    persist_write_int(PERSIST_KEY_READINESS_COLOR, s_readiness_color);
  }
  
  Tuple *sleep_color_tuple = dict_find(iterator, MESSAGE_KEY_sleep_color);
  if (sleep_color_tuple) {
    s_sleep_color = tuple_to_int(sleep_color_tuple, s_sleep_color);
    APP_LOG(APP_LOG_LEVEL_INFO, "Sleep color updated: %d", s_sleep_color);
    persist_write_int(PERSIST_KEY_SLEEP_COLOR, s_sleep_color);
  }
  
  Tuple *heart_rate_color_tuple = dict_find(iterator, MESSAGE_KEY_heart_rate_color);
  if (heart_rate_color_tuple) {
    s_heart_rate_color = tuple_to_int(heart_rate_color_tuple, s_heart_rate_color);
    APP_LOG(APP_LOG_LEVEL_INFO, "Heart rate color updated: %d", s_heart_rate_color);
    persist_write_int(PERSIST_KEY_HEART_COLOR, s_heart_rate_color);
  }
  
  Tuple *activity_color_tuple = dict_find(iterator, MESSAGE_KEY_activity_color);
  if (activity_color_tuple) {
    s_activity_color = tuple_to_int(activity_color_tuple, s_activity_color);
    APP_LOG(APP_LOG_LEVEL_INFO, "Activity color updated: %d", s_activity_color);
    persist_write_int(PERSIST_KEY_ACTIVITY_COLOR, s_activity_color);
  }
  
  Tuple *stress_color_tuple = dict_find(iterator, MESSAGE_KEY_stress_color);
  if (stress_color_tuple) {
    s_stress_color = tuple_to_int(stress_color_tuple, s_stress_color);
    APP_LOG(APP_LOG_LEVEL_INFO, "Stress color updated: %d", s_stress_color);
    persist_write_int(PERSIST_KEY_STRESS_COLOR, s_stress_color);
  }
  
  // Apply colors if any color settings were received
  if (background_color_tuple || time_color_tuple || date_color_tuple || 
      readiness_color_tuple || sleep_color_tuple || heart_rate_color_tuple ||
      activity_color_tuple || stress_color_tuple) {
    apply_theme_colors();
  }

  // Process date format configuration
  Tuple *date_format_tuple = dict_find(iterator, MESSAGE_KEY_date_format);
  if (date_format_tuple) {
    s_date_format = tuple_to_int(date_format_tuple, s_date_format);
    APP_LOG(APP_LOG_LEVEL_INFO, "Date format updated: %d", s_date_format);
    update_date_display();
  }
  
  // Process theme mode configuration
  Tuple *theme_mode_tuple = dict_find(iterator, MESSAGE_KEY_theme_mode);
  if (theme_mode_tuple) {
    s_theme_mode = tuple_to_int(theme_mode_tuple, s_theme_mode);
    APP_LOG(APP_LOG_LEVEL_INFO, "Theme mode updated: %d", s_theme_mode);
    persist_write_int(PERSIST_KEY_THEME_MODE, s_theme_mode);
    apply_theme_colors();
  }
  
  // Process custom color index (for theme mode 2)
  Tuple *custom_color_tuple = dict_find(iterator, MESSAGE_KEY_custom_color_index);
  if (custom_color_tuple) {
    s_custom_color_index = tuple_to_int(custom_color_tuple, s_custom_color_index);
    APP_LOG(APP_LOG_LEVEL_INFO, "Custom color index updated: %d", s_custom_color_index);
    persist_write_int(PERSIST_KEY_CUSTOM_COLOR, s_custom_color_index);
    if (s_theme_mode == 2) {
      apply_theme_colors();
      APP_LOG(APP_LOG_LEVEL_INFO, "Custom color applied to watchface");
    }
  }

  // Process show loading screen configuration
  Tuple *show_loading_tuple = dict_find(iterator, MESSAGE_KEY_show_loading);
  if (show_loading_tuple) {
    s_show_loading = tuple_to_bool(show_loading_tuple, s_show_loading);
    s_initial_startup = false; // Initial startup complete, now respect user preference
    persist_write_bool(PERSIST_KEY_SHOW_LOADING, s_show_loading);
    APP_LOG(APP_LOG_LEVEL_INFO, "Show loading overlay setting: %d (initial startup complete)", s_show_loading);
  }

  // Process show seconds configuration
  Tuple *show_seconds_tuple = dict_find(iterator, MESSAGE_KEY_show_seconds);
  if (show_seconds_tuple) {
    s_show_seconds = tuple_to_bool(show_seconds_tuple, s_show_seconds);
    persist_write_bool(PERSIST_KEY_SHOW_SECONDS, s_show_seconds);
    update_tick_subscription();
    update_time_display();
    APP_LOG(APP_LOG_LEVEL_INFO, "Show Seconds setting updated: %d", s_show_seconds);
  }

  // Process compact time configuration
  Tuple *compact_time_tuple = dict_find(iterator, MESSAGE_KEY_compact_time);
  if (compact_time_tuple) {
    s_compact_time = tuple_to_bool(compact_time_tuple, s_compact_time);
    persist_write_bool(PERSIST_KEY_COMPACT_TIME, s_compact_time);
    update_time_display();
    APP_LOG(APP_LOG_LEVEL_INFO, "Compact Time setting updated: %d", s_compact_time);
  }

  // Process show debug preference
  Tuple *show_debug_tuple = dict_find(iterator, MESSAGE_KEY_show_debug);
  if (show_debug_tuple) {
    s_show_debug = tuple_to_bool(show_debug_tuple, s_show_debug);
    persist_write_bool(PERSIST_KEY_SHOW_DEBUG, s_show_debug);
    APP_LOG(APP_LOG_LEVEL_INFO, "Show debug setting updated: %d", s_show_debug);
  }

  // Process refresh frequency (minutes)
  Tuple *refresh_freq_tuple = dict_find(iterator, MESSAGE_KEY_refresh_frequency);
  if (refresh_freq_tuple) {
    int new_freq = tuple_to_int(refresh_freq_tuple, s_refresh_frequency_minutes);
    if (new_freq < 1) new_freq = 1; // Safety guard
    s_refresh_frequency_minutes = new_freq;
    s_minutes_since_refresh = 0; // Restart counter on change
    persist_write_int(PERSIST_KEY_REFRESH_FREQUENCY, s_refresh_frequency_minutes);
    APP_LOG(APP_LOG_LEVEL_INFO, "Refresh frequency updated: %d minutes", s_refresh_frequency_minutes);
  }
  
  // If we received any real data, hide the sample indicator
  s_using_sample_data = false;
  update_sample_indicator();
  
  // Mark that real data was received and clear debug message after 10 seconds
  s_real_data_received = true;
  Tuple *payload_complete_tuple = dict_find(iterator, MESSAGE_KEY_payload_complete);
  if (payload_complete_tuple) {
    s_fetch_completed = true;
    // Always hide loading screen when data arrives, regardless of how it was shown
    if (s_loading) {
      // Hold loading screen for 2 seconds to allow reading logs
      if (s_loading_hide_timer) { app_timer_cancel(s_loading_hide_timer); }
      s_loading_hide_timer = app_timer_register(2000, (AppTimerCallback) hide_loading_overlay, NULL);
    }
  }
  if (s_debug_timer) {
    app_timer_cancel(s_debug_timer);
  }
  s_debug_timer = app_timer_register(10000, debug_timer_callback, NULL);
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", reason);
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed: %d", reason);
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success");
}

// Apply theme colors to all UI elements
static void apply_theme_colors(void) {
  // Update window background
  if (s_window) {
    window_set_background_color(s_window, get_palette_color(s_background_color));
  }
  
  // Update time and date with individual colors
  if (s_time_layer) {
    text_layer_set_text_color(s_time_layer, get_palette_color(s_time_color));
  }
  
  if (s_date_layer) {
    text_layer_set_text_color(s_date_layer, get_palette_color(s_date_color));
  }
  
  // Keep debug and sample indicator using time color for consistency
  if (s_debug_layer) {
    text_layer_set_text_color(s_debug_layer, get_palette_color(s_time_color));
  }
  
  if (s_sample_indicator_layer) {
    text_layer_set_text_color(s_sample_indicator_layer, get_palette_color(s_time_color));
  }
  
  // Update measurement layers with individual colors
  if (s_sleep_layer) {
    text_layer_set_text_color(s_sleep_layer, get_palette_color(s_sleep_color));
  }
  
  if (s_sleep_label_layer) {
    text_layer_set_text_color(s_sleep_label_layer, get_palette_color(s_sleep_color));
  }
  
  if (s_readiness_layer) {
    text_layer_set_text_color(s_readiness_layer, get_palette_color(s_readiness_color));
  }
  
  if (s_readiness_label_layer) {
    text_layer_set_text_color(s_readiness_label_layer, get_palette_color(s_readiness_color));
  }
  
  if (s_heart_rate_layer) {
    text_layer_set_text_color(s_heart_rate_layer, get_palette_color(s_heart_rate_color));
  }
  
  if (s_heart_rate_label_layer) {
    text_layer_set_text_color(s_heart_rate_label_layer, get_palette_color(s_heart_rate_color));
  }
  
  if (s_activity_layer) {
    text_layer_set_text_color(s_activity_layer, get_palette_color(s_activity_color));
  }
  
  if (s_activity_label_layer) {
    text_layer_set_text_color(s_activity_label_layer, get_palette_color(s_activity_color));
  }
  
  if (s_stress_layer) {
    text_layer_set_text_color(s_stress_layer, get_palette_color(s_stress_color));
  }
  
  if (s_stress_label_layer) {
    text_layer_set_text_color(s_stress_label_layer, get_palette_color(s_stress_color));
  }
}

// =============================================================================
// APP LIFECYCLE
// =============================================================================

static void init(void) {
  // Create main window
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload
  });
  window_stack_push(s_window, true);
  
  // Initialize AppMessage for communication with JavaScript
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  
  // Open AppMessage with appropriate buffer sizes
  const int inbox_size = 512;
  const int outbox_size = 64;
  app_message_open(inbox_size, outbox_size);

  // Load persisted preferences
  if (persist_exists(PERSIST_KEY_SHOW_LOADING)) {
    s_show_loading = persist_read_bool(PERSIST_KEY_SHOW_LOADING);
  }
  if (persist_exists(PERSIST_KEY_SHOW_DEBUG)) {
    s_show_debug = persist_read_bool(PERSIST_KEY_SHOW_DEBUG);
  }
  if (persist_exists(PERSIST_KEY_REFRESH_FREQUENCY)) {
    s_refresh_frequency_minutes = persist_read_int(PERSIST_KEY_REFRESH_FREQUENCY);
  }
  if (persist_exists(PERSIST_KEY_SHOW_SECONDS)) {
    s_show_seconds = persist_read_bool(PERSIST_KEY_SHOW_SECONDS);
  }
  if (persist_exists(PERSIST_KEY_COMPACT_TIME)) {
    s_compact_time = persist_read_bool(PERSIST_KEY_COMPACT_TIME);
  }
  // Load persisted theme/color preferences
  if (persist_exists(PERSIST_KEY_THEME_MODE)) {
    s_theme_mode = persist_read_int(PERSIST_KEY_THEME_MODE);
  }
  if (persist_exists(PERSIST_KEY_CUSTOM_COLOR)) {
    s_custom_color_index = persist_read_int(PERSIST_KEY_CUSTOM_COLOR);
  }
  if (persist_exists(PERSIST_KEY_USE_EMOJI)) {
    s_use_emoji = persist_read_bool(PERSIST_KEY_USE_EMOJI);
  }
  if (persist_exists(PERSIST_KEY_BG_COLOR)) {
    s_background_color = persist_read_int(PERSIST_KEY_BG_COLOR);
  }
  if (persist_exists(PERSIST_KEY_TIME_COLOR)) {
    s_time_color = persist_read_int(PERSIST_KEY_TIME_COLOR);
  }
  if (persist_exists(PERSIST_KEY_DATE_COLOR)) {
    s_date_color = persist_read_int(PERSIST_KEY_DATE_COLOR);
  }
  if (persist_exists(PERSIST_KEY_READINESS_COLOR)) {
    s_readiness_color = persist_read_int(PERSIST_KEY_READINESS_COLOR);
  }
  if (persist_exists(PERSIST_KEY_SLEEP_COLOR)) {
    s_sleep_color = persist_read_int(PERSIST_KEY_SLEEP_COLOR);
  }
  if (persist_exists(PERSIST_KEY_HEART_COLOR)) {
    s_heart_rate_color = persist_read_int(PERSIST_KEY_HEART_COLOR);
  }
  if (persist_exists(PERSIST_KEY_ACTIVITY_COLOR)) {
    s_activity_color = persist_read_int(PERSIST_KEY_ACTIVITY_COLOR);
  }
  if (persist_exists(PERSIST_KEY_STRESS_COLOR)) {
    s_stress_color = persist_read_int(PERSIST_KEY_STRESS_COLOR);
  }
  if (s_refresh_frequency_minutes < 1) s_refresh_frequency_minutes = 30;
  s_minutes_since_refresh = 0;
  
  // Initialize displays
  update_time_display();
  // Apply persisted theme/colors after layers are created
  apply_theme_colors();
  fetch_oura_data();
  
  // Subscribe to time updates
  update_tick_subscription();
  
  // Initialize custom color mode
  if (s_theme_mode == 2) {
    apply_theme_colors();
  }
  
  APP_LOG(APP_LOG_LEVEL_INFO, "Oura Stats Watchface initialized (theme_mode: %d)", s_theme_mode);
}

static void deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
