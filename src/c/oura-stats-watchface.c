#include <pebble.h>

// =============================================================================
// OURA STATS WATCHFACE - MODULAR DESIGN
// =============================================================================
// Layout: Time (center top), Heart Rate (bottom left), 
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

// Data buffers
static char s_time_buffer[16];
static char s_date_buffer[16];
static char s_debug_buffer[32];
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
static bool s_show_loading = true; // Controls whether to show the loading overlay on refresh (configurable from JS)
// becomes true when any real data or payload_complete received
static bool s_fetch_completed = false;

// Measurement layout configuration
// 0=readiness, 1=sleep, 2=heart_rate, 3=activity, 4=stress
static int s_layout_left = 0;    // Default: readiness
static int s_layout_middle = 1;  // Default: sleep
static int s_layout_right = 2;   // Default: heart_rate

// Date format configuration
// 0=MM-DD-YYYY, 1=DD-MM-YYYY
static int s_date_format = 0;    // Default: MM-DD-YYYY

// Theme mode configuration
// 0=Dark Mode (white text on black), 1=Light Mode (black text on white), 2=Custom Color Mode (fixed color from config)
static int s_theme_mode = 2;     // Default: Custom Color Mode for test build

// Custom color system
static int s_custom_color_index = 0;  // Selected color index from config page
static const int s_color_palette_size = 64;

// Forward declarations
static void update_debug_display(const char* message);
static void apply_theme_colors(void);
static bool is_light_color(GColor color);
static GColor get_palette_color(int index);
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
  
  strftime(s_time_buffer, sizeof(s_time_buffer), clock_is_24h_style() ?
                                          "%H:%M" : "%I:%M", tick_time);
  
  text_layer_set_text(s_time_layer, s_time_buffer);
}

static void update_date_display() {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  
  if (s_date_format == 1) {
    // DD-MM-YYYY format
    strftime(s_date_buffer, sizeof(s_date_buffer), "%d-%m-%Y", tick_time);
  } else {
    // MM-DD-YYYY format (default)
    strftime(s_date_buffer, sizeof(s_date_buffer), "%m-%d-%Y", tick_time);
  }
  
  text_layer_set_text(s_date_layer, s_date_buffer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time_display();
  update_date_display();
  
  // Update Oura data every hour (or on minute change for testing)
  if (units_changed & HOUR_UNIT) {
    // TODO: Trigger Oura API data refresh
    APP_LOG(APP_LOG_LEVEL_INFO, "Time to refresh Oura data");
  }
}

// =============================================================================
// DYNAMIC LAYOUT SYSTEM
// =============================================================================

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
    default:
      return; // Invalid position
  }
  
  // Get the value and emoji based on measurement type
  switch (measurement_type) {
    case 0: // Readiness
      emoji_text = "🎉";
      if (s_readiness_data.data_available) {
        snprintf(value_buffer, sizeof(value_buffer), "%d", s_readiness_data.readiness_score);
        value_text = value_buffer;
      } else if (s_fetch_completed) { value_text = "--"; }
      break;
    case 1: // Sleep
      emoji_text = "😴";
      if (s_sleep_data.data_available) {
        snprintf(value_buffer, sizeof(value_buffer), "%d", s_sleep_data.sleep_score);
        value_text = value_buffer;
      } else if (s_fetch_completed) { value_text = "--"; }
      break;
    case 2: // Heart Rate
      emoji_text = "❤";
      if (s_heart_rate_data.data_available) {
        snprintf(value_buffer, sizeof(value_buffer), "%d", s_heart_rate_data.resting_heart_rate);
        value_text = value_buffer;
      } else if (s_fetch_completed) { value_text = "--"; }
      break;
  }
  
  // Update the display
  snprintf(buffer, 16, "%s", value_text);
  text_layer_set_text(layer, buffer);
  
  // Update the emoji label
  if (label_layer) {
    text_layer_set_text(label_layer, emoji_text);
  }
}

// Update all measurements according to current layout
static void update_all_measurements() {
  update_measurement_at_position(s_layout_left, 0);    // Left position
  update_measurement_at_position(s_layout_middle, 1);  // Middle position
  update_measurement_at_position(s_layout_right, 2);   // Right position
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
  if (s_activity_data.data_available && s_activity_data.activity_score > 0) {
    snprintf(s_activity_buffer, sizeof(s_activity_buffer), "%d", s_activity_data.activity_score);
  } else {
    snprintf(s_activity_buffer, sizeof(s_activity_buffer), "--");
  }
  
  if (s_activity_layer) {
    text_layer_set_text(s_activity_layer, s_activity_buffer);
  }
}

static void update_stress_display() {
  if (s_stress_data.data_available) {
    int total_minutes = s_stress_data.stress_duration / 60; // allow 0m
    int hours = total_minutes / 60;
    int minutes = total_minutes % 60;
    
    if (hours > 0) {
      snprintf(s_stress_buffer, sizeof(s_stress_buffer), "%dh %dm", hours, minutes);
    } else {
      snprintf(s_stress_buffer, sizeof(s_stress_buffer), "%dm", minutes);
    }
  } else {
    snprintf(s_stress_buffer, sizeof(s_stress_buffer), "--");
  }
  
  if (s_stress_layer) {
    text_layer_set_text(s_stress_layer, s_stress_buffer);
  }
}

// =============================================================================
// OURA API MODULE (Placeholder for future implementation)
// =============================================================================

static void request_oura_data() {
  // Request fresh data from JavaScript component
  // If enabled, show loading overlay for this refresh
  if (s_show_loading) {
    show_loading_overlay();
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
      GRect(0, PBL_IF_ROUND_ELSE(50, 45), bounds.size.w, 30));
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
  
  // Main measurements row (3 columns) - repositioned higher to make room for activity/stress
  // Sleep (middle row left)
  s_sleep_layer = text_layer_create(
      GRect(0, bounds.size.h - 91, bounds.size.w/3, 25));
  text_layer_set_background_color(s_sleep_layer, GColorClear);
  text_layer_set_text_color(s_sleep_layer, get_text_color());
  text_layer_set_font(s_sleep_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_sleep_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_sleep_layer));
  
  // Sleep Label (emoji)
  s_sleep_label_layer = text_layer_create(
      GRect(0, bounds.size.h - 66, bounds.size.w/3, 20));
  text_layer_set_background_color(s_sleep_label_layer, GColorClear);
  text_layer_set_text_color(s_sleep_label_layer, get_text_color());
  text_layer_set_font(s_sleep_label_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_sleep_label_layer, GTextAlignmentCenter);
  text_layer_set_text(s_sleep_label_layer, "😴");
  layer_add_child(window_layer, text_layer_get_layer(s_sleep_label_layer));

  // Readiness (middle row middle)
  s_readiness_layer = text_layer_create(
      GRect(bounds.size.w/3, bounds.size.h - 91, bounds.size.w/3, 25));
  text_layer_set_background_color(s_readiness_layer, GColorClear);
  text_layer_set_text_color(s_readiness_layer, get_text_color());
  text_layer_set_font(s_readiness_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_readiness_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_readiness_layer));

  // Readiness Label (emoji)
  s_readiness_label_layer = text_layer_create(
      GRect(bounds.size.w/3, bounds.size.h - 66, bounds.size.w/3, 20));
  text_layer_set_background_color(s_readiness_label_layer, GColorClear);
  text_layer_set_text_color(s_readiness_label_layer, get_text_color());
  text_layer_set_font(s_readiness_label_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_readiness_label_layer, GTextAlignmentCenter);
  text_layer_set_text(s_readiness_label_layer, "🎉");
  layer_add_child(window_layer, text_layer_get_layer(s_readiness_label_layer));

  // Heart Rate (middle row right)
  s_heart_rate_layer = text_layer_create(
      GRect(2*bounds.size.w/3, bounds.size.h - 91, bounds.size.w/3, 25));
  text_layer_set_background_color(s_heart_rate_layer, GColorClear);
  text_layer_set_text_color(s_heart_rate_layer, get_text_color());
  text_layer_set_font(s_heart_rate_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_heart_rate_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_heart_rate_layer));

  // Heart Rate Label (emoji)
  s_heart_rate_label_layer = text_layer_create(
      GRect(2*bounds.size.w/3, bounds.size.h - 66, bounds.size.w/3, 20));
  text_layer_set_background_color(s_heart_rate_label_layer, GColorClear);
  text_layer_set_text_color(s_heart_rate_label_layer, get_text_color());
  text_layer_set_font(s_heart_rate_label_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_heart_rate_label_layer, GTextAlignmentCenter);
  text_layer_set_text(s_heart_rate_label_layer, "❤");
  layer_add_child(window_layer, text_layer_get_layer(s_heart_rate_label_layer));

  // Bottom row for activity and stress (2 columns) - moved up by 1 pixel
  // Activity (bottom row left)
  s_activity_layer = text_layer_create(
      GRect(0, bounds.size.h - 41, bounds.size.w/2, 20));
  text_layer_set_background_color(s_activity_layer, GColorClear);
  text_layer_set_text_color(s_activity_layer, get_text_color());
  text_layer_set_font(s_activity_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_activity_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_activity_layer));

  // Activity Label (emoji)
  s_activity_label_layer = text_layer_create(
      GRect(0, bounds.size.h - 21, bounds.size.w/2, 20));
  text_layer_set_background_color(s_activity_label_layer, GColorClear);
  text_layer_set_text_color(s_activity_label_layer, get_text_color());
  text_layer_set_font(s_activity_label_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_activity_label_layer, GTextAlignmentCenter);
  text_layer_set_text(s_activity_label_layer, "🔥");
  layer_add_child(window_layer, text_layer_get_layer(s_activity_label_layer));

  // Stress (bottom row right)
  s_stress_layer = text_layer_create(
      GRect(bounds.size.w/2, bounds.size.h - 41, bounds.size.w/2, 20));
  text_layer_set_background_color(s_stress_layer, GColorClear);
  text_layer_set_text_color(s_stress_layer, get_text_color());
  text_layer_set_font(s_stress_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_stress_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_stress_layer));

  // Stress Label (emoji)
  s_stress_label_layer = text_layer_create(
      GRect(bounds.size.w/2, bounds.size.h - 21, bounds.size.w/2, 20));
  text_layer_set_background_color(s_stress_label_layer, GColorClear);
  text_layer_set_text_color(s_stress_label_layer, get_text_color());
  text_layer_set_font(s_stress_label_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_stress_label_layer, GTextAlignmentCenter);
  text_layer_set_text(s_stress_label_layer, "😰");
  layer_add_child(window_layer, text_layer_get_layer(s_stress_label_layer));

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
  
  // Ensure overlay visibility according to state
  layer_set_hidden(s_loading_layer, !s_loading);
  layer_set_hidden(text_layer_get_layer(s_loading_text_layer), !s_loading);
  layer_set_hidden(text_layer_get_layer(s_loading_logs_layer), !s_loading);

  // We no longer want debug text on the main watchface; hide it permanently
  if (s_debug_layer) {
    layer_set_hidden(text_layer_get_layer(s_debug_layer), true);
  }
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
// APPMESSAGE HANDLERS (Communication with JavaScript)
// =============================================================================

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
  
  // Process date format configuration
  Tuple *date_format_tuple = dict_find(iterator, MESSAGE_KEY_date_format);
  if (date_format_tuple) {
    s_date_format = date_format_tuple->value->int32;
    APP_LOG(APP_LOG_LEVEL_INFO, "Date format updated: %d", s_date_format);
    update_date_display();
  }
  
  // Process theme mode configuration
  Tuple *theme_mode_tuple = dict_find(iterator, MESSAGE_KEY_theme_mode);
  if (theme_mode_tuple) {
    s_theme_mode = theme_mode_tuple->value->int32;
    APP_LOG(APP_LOG_LEVEL_INFO, "Theme mode updated: %d", s_theme_mode);
    apply_theme_colors();
  }
  
  // Process custom color index (for theme mode 2)
  Tuple *custom_color_tuple = dict_find(iterator, MESSAGE_KEY_custom_color_index);
  if (custom_color_tuple) {
    s_custom_color_index = custom_color_tuple->value->int32;
    APP_LOG(APP_LOG_LEVEL_INFO, "Custom color index updated: %d", s_custom_color_index);
    if (s_theme_mode == 2) {
      apply_theme_colors();
      APP_LOG(APP_LOG_LEVEL_INFO, "Custom color applied to watchface");
    }
  }

  // Process show loading screen configuration
  Tuple *show_loading_tuple = dict_find(iterator, MESSAGE_KEY_show_loading);
  if (show_loading_tuple) {
    s_show_loading = show_loading_tuple->value->int32 ? true : false;
    APP_LOG(APP_LOG_LEVEL_INFO, "Show loading overlay setting: %d", s_show_loading);
  }
  
  // Process flexible layout configuration
  Tuple *layout_rows_tuple = dict_find(iterator, MESSAGE_KEY_layout_rows);
  Tuple *row1_left_tuple = dict_find(iterator, MESSAGE_KEY_row1_left);
  Tuple *row1_middle_tuple = dict_find(iterator, MESSAGE_KEY_row1_middle);
  Tuple *row1_right_tuple = dict_find(iterator, MESSAGE_KEY_row1_right);
  Tuple *row2_left_tuple = dict_find(iterator, MESSAGE_KEY_row2_left);
  Tuple *row2_middle_tuple = dict_find(iterator, MESSAGE_KEY_row2_middle);
  Tuple *row2_right_tuple = dict_find(iterator, MESSAGE_KEY_row2_right);
  // Silence potential unused variable warnings until row2 support is wired to UI
  (void)row2_left_tuple; (void)row2_middle_tuple; (void)row2_right_tuple;
  
  if (layout_rows_tuple && row1_left_tuple && row1_middle_tuple && row1_right_tuple) {
    int layout_rows = layout_rows_tuple->value->int32;
    int row1_left = row1_left_tuple->value->int32;
    int row1_middle = row1_middle_tuple->value->int32;
    int row1_right = row1_right_tuple->value->int32;
    
    APP_LOG(APP_LOG_LEVEL_INFO, "Flexible layout config received: %d rows", layout_rows);
    APP_LOG(APP_LOG_LEVEL_INFO, "Row 1 layout: L=%d M=%d R=%d", row1_left, row1_middle, row1_right);
    
    // Apply flexible layout configuration to simple layout variables
    s_layout_left = row1_left;
    s_layout_middle = row1_middle;
    s_layout_right = row1_right;
    
    APP_LOG(APP_LOG_LEVEL_INFO, "Applied flexible layout: L=%d M=%d R=%d", 
            s_layout_left, s_layout_middle, s_layout_right);
    
    // Update all displays with new layout configuration
    update_heart_rate_display();
    update_readiness_display();
    update_sleep_display();
    update_activity_display();
    update_stress_display();
    
    APP_LOG(APP_LOG_LEVEL_INFO, "All displays updated with new flexible layout");
  }
  
  // If we received any real data, hide the sample indicator
  s_using_sample_data = false;
  update_sample_indicator();
  
  // Mark that real data was received and clear debug message after 10 seconds
  s_real_data_received = true;
  Tuple *payload_complete_tuple = dict_find(iterator, MESSAGE_KEY_payload_complete);
  if (payload_complete_tuple) {
    s_fetch_completed = true;
    // Hold loading screen for 3 seconds to allow reading logs
    if (s_loading_hide_timer) { app_timer_cancel(s_loading_hide_timer); }
    s_loading_hide_timer = app_timer_register(3000, (AppTimerCallback) hide_loading_overlay, NULL);
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
  GColor bg_color = get_background_color();
  GColor text_color = get_text_color();
  
  // Update window background
  if (s_window) {
    window_set_background_color(s_window, bg_color);
  }
  
  // Update all text layers
  if (s_time_layer) {
    text_layer_set_text_color(s_time_layer, text_color);
  }
  
  if (s_date_layer) {
    text_layer_set_text_color(s_date_layer, text_color);
  }
  
  if (s_debug_layer) {
    text_layer_set_text_color(s_debug_layer, text_color);
  }
  
  if (s_sample_indicator_layer) {
    text_layer_set_text_color(s_sample_indicator_layer, text_color);
  }
  
  // Update measurement layers
  if (s_sleep_layer) {
    text_layer_set_text_color(s_sleep_layer, text_color);
  }
  
  if (s_sleep_label_layer) {
    text_layer_set_text_color(s_sleep_label_layer, text_color);
  }
  
  if (s_readiness_layer) {
    text_layer_set_text_color(s_readiness_layer, text_color);
  }
  
  if (s_readiness_label_layer) {
    text_layer_set_text_color(s_readiness_label_layer, text_color);
  }
  
  if (s_heart_rate_layer) {
    text_layer_set_text_color(s_heart_rate_layer, text_color);
  }
  
  if (s_heart_rate_label_layer) {
    text_layer_set_text_color(s_heart_rate_label_layer, text_color);
  }
  
  if (s_activity_layer) {
    text_layer_set_text_color(s_activity_layer, text_color);
  }
  
  if (s_activity_label_layer) {
    text_layer_set_text_color(s_activity_label_layer, text_color);
  }
  
  if (s_stress_layer) {
    text_layer_set_text_color(s_stress_layer, text_color);
  }
  
  if (s_stress_label_layer) {
    text_layer_set_text_color(s_stress_label_layer, text_color);
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
  
  // Show the window
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
  
  // Initialize displays
  update_time_display();
  fetch_oura_data();
  
  // Subscribe to time updates
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  
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
