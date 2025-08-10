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
static TextLayer *s_debug_layer;
static TextLayer *s_sample_indicator_layer;
static TextLayer *s_heart_rate_layer;
static TextLayer *s_heart_rate_label_layer;
static TextLayer *s_readiness_layer;
static TextLayer *s_readiness_label_layer;
static TextLayer *s_sleep_layer;
static TextLayer *s_sleep_label_layer;

// Data buffers
static char s_time_buffer[16];
static char s_debug_buffer[32];
static char s_sample_indicator_buffer[16];
static char s_heart_rate_buffer[16];
static char s_readiness_buffer[16];
static char s_sleep_buffer[16];

// Timer for debug message timeout
static AppTimer *s_debug_timer = NULL;
static bool s_real_data_received = false;

// Forward declarations
static void update_debug_display(const char* message);

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

// Global Oura data
static OuraHeartRateData s_heart_rate_data = {0};
static OuraReadinessData s_readiness_data = {0};
static OuraSleepData s_sleep_data = {0};
static bool s_using_sample_data = true;

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

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time_display();
  
  // Update Oura data every hour (or on minute change for testing)
  if (units_changed & HOUR_UNIT) {
    // TODO: Trigger Oura API data refresh
    APP_LOG(APP_LOG_LEVEL_INFO, "Time to refresh Oura data");
  }
}

// =============================================================================
// HEART RATE MODULE (Bottom Left)
// =============================================================================

static void update_heart_rate_display() {
  if (s_heart_rate_data.data_available) {
    int hr = s_heart_rate_data.resting_heart_rate;
    snprintf(s_heart_rate_buffer, sizeof(s_heart_rate_buffer), "%d", hr);
  } else {
    snprintf(s_heart_rate_buffer, sizeof(s_heart_rate_buffer), "--");
  }
  text_layer_set_text(s_heart_rate_layer, s_heart_rate_buffer);
}

// =============================================================================
// READINESS MODULE (Bottom Center)
// =============================================================================

static void update_readiness_display() {
  if (s_readiness_data.data_available) {
    int score = s_readiness_data.readiness_score;
    snprintf(s_readiness_buffer, sizeof(s_readiness_buffer), "%d", score);
  } else {
    snprintf(s_readiness_buffer, sizeof(s_readiness_buffer), "--");
  }
  text_layer_set_text(s_readiness_layer, s_readiness_buffer);
}

// =============================================================================
// SLEEP MODULE (Bottom Right)
// =============================================================================

static void update_sleep_display() {
  if (s_sleep_data.data_available) {
    int score = s_sleep_data.sleep_score;
    snprintf(s_sleep_buffer, sizeof(s_sleep_buffer), "%d", score);
  } else {
    snprintf(s_sleep_buffer, sizeof(s_sleep_buffer), "--");
  }
  text_layer_set_text(s_sleep_layer, s_sleep_buffer);
}

// =============================================================================
// OURA API MODULE (Placeholder for future implementation)
// =============================================================================

static void request_oura_data() {
  // Request fresh data from JavaScript component
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
  if (message) {
    snprintf(s_debug_buffer, sizeof(s_debug_buffer), "%.30s", message);
    
    // Cancel existing timer
    if (s_debug_timer) {
      app_timer_cancel(s_debug_timer);
      s_debug_timer = NULL;
    }
    
    // Set timeout based on message type
    if (strstr(message, "Requesting")) {
      // 1 minute timeout for "Requesting real data"
      s_debug_timer = app_timer_register(60000, debug_timer_callback, NULL);
    }
  } else {
    s_debug_buffer[0] = '\0';  // Clear the buffer
    if (s_debug_timer) {
      app_timer_cancel(s_debug_timer);
      s_debug_timer = NULL;
    }
  }
  text_layer_set_text(s_debug_layer, s_debug_buffer);
}

// =============================================================================
// SAMPLE DATA INDICATOR
// =============================================================================

static void update_sample_indicator() {
  if (s_using_sample_data) {
    snprintf(s_sample_indicator_buffer, sizeof(s_sample_indicator_buffer), "Sample");
  } else {
    s_sample_indicator_buffer[0] = '\0';  // Clear the buffer
  }
  text_layer_set_text(s_sample_indicator_layer, s_sample_indicator_buffer);
}

static void fetch_oura_data() {
  // Set sample data for initial display (will be replaced by real data)
  update_debug_display("Loading sample data...");
  
  s_heart_rate_data.resting_heart_rate = 65;
  s_heart_rate_data.hrv_score = 45;
  s_heart_rate_data.data_available = true;
  
  s_readiness_data.readiness_score = 85;
  s_readiness_data.temperature_deviation = 0;
  s_readiness_data.recovery_index = 82;
  s_readiness_data.data_available = true;
  
  s_sleep_data.sleep_score = 78;
  s_sleep_data.total_sleep_time = 450; // 7.5 hours
  s_sleep_data.deep_sleep_time = 90;
  s_sleep_data.data_available = true;
  
  // Mark as using sample data
  s_using_sample_data = true;
  
  // Update all displays
  update_heart_rate_display();
  update_readiness_display();
  update_sleep_display();
  update_sample_indicator();
  
  APP_LOG(APP_LOG_LEVEL_INFO, "Sample Oura data loaded");
  
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
  
  // Set dark background
  window_set_background_color(window, GColorBlack);
  
  // Time display (center top) - bigger font, closer to top
  s_time_layer = text_layer_create(
      GRect(0, PBL_IF_ROUND_ELSE(10, 5), bounds.size.w, 60));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
  
  // Debug status (between time and sample indicator)
  s_debug_layer = text_layer_create(
      GRect(0, PBL_IF_ROUND_ELSE(70, 65), bounds.size.w, 15));
  text_layer_set_background_color(s_debug_layer, GColorClear);
  text_layer_set_text_color(s_debug_layer, GColorWhite);
  text_layer_set_font(s_debug_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_debug_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_debug_layer));
  
  // Sample indicator (under debug)
  s_sample_indicator_layer = text_layer_create(
      GRect(0, PBL_IF_ROUND_ELSE(85, 80), bounds.size.w, 20));
  text_layer_set_background_color(s_sample_indicator_layer, GColorClear);
  text_layer_set_text_color(s_sample_indicator_layer, GColorWhite);
  text_layer_set_font(s_sample_indicator_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_sample_indicator_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_sample_indicator_layer));
  
  // Sleep (bottom left) - larger font, moved up for emoji
  s_sleep_layer = text_layer_create(
      GRect(0, bounds.size.h - 85, bounds.size.w/3, 30));
  text_layer_set_background_color(s_sleep_layer, GColorClear);
  text_layer_set_text_color(s_sleep_layer, GColorWhite);
  text_layer_set_font(s_sleep_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_sleep_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_sleep_layer));
  
  // Sleep Label (emoji)
  s_sleep_label_layer = text_layer_create(
      GRect(0, bounds.size.h - 55, bounds.size.w/3, 35));
  text_layer_set_background_color(s_sleep_label_layer, GColorClear);
  text_layer_set_text_color(s_sleep_label_layer, GColorWhite);
  text_layer_set_font(s_sleep_label_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_sleep_label_layer, GTextAlignmentCenter);
  text_layer_set_text(s_sleep_label_layer, "😴");
  layer_add_child(window_layer, text_layer_get_layer(s_sleep_label_layer));
  
  // Readiness (bottom center) - larger font, moved up for emoji
  s_readiness_layer = text_layer_create(
      GRect(bounds.size.w/3, bounds.size.h - 85, bounds.size.w/3, 30));
  text_layer_set_background_color(s_readiness_layer, GColorClear);
  text_layer_set_text_color(s_readiness_layer, GColorWhite);
  text_layer_set_font(s_readiness_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_readiness_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_readiness_layer));
  
  // Readiness Label (emoji)
  s_readiness_label_layer = text_layer_create(
      GRect(bounds.size.w/3, bounds.size.h - 55, bounds.size.w/3, 35));
  text_layer_set_background_color(s_readiness_label_layer, GColorClear);
  text_layer_set_text_color(s_readiness_label_layer, GColorWhite);
  text_layer_set_font(s_readiness_label_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_readiness_label_layer, GTextAlignmentCenter);
  text_layer_set_text(s_readiness_label_layer, "🎉");
  layer_add_child(window_layer, text_layer_get_layer(s_readiness_label_layer));
  
  // Heart Rate (bottom right) - larger font, moved up for emoji
  s_heart_rate_layer = text_layer_create(
      GRect(2*bounds.size.w/3, bounds.size.h - 85, bounds.size.w/3, 30));
  text_layer_set_background_color(s_heart_rate_layer, GColorClear);
  text_layer_set_text_color(s_heart_rate_layer, GColorWhite);
  text_layer_set_font(s_heart_rate_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_heart_rate_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_heart_rate_layer));
  
  // Heart Rate Label (emoji)
  s_heart_rate_label_layer = text_layer_create(
      GRect(2*bounds.size.w/3, bounds.size.h - 55, bounds.size.w/3, 35));
  text_layer_set_background_color(s_heart_rate_label_layer, GColorClear);
  text_layer_set_text_color(s_heart_rate_label_layer, GColorWhite);
  text_layer_set_font(s_heart_rate_label_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_heart_rate_label_layer, GTextAlignmentCenter);
  text_layer_set_text(s_heart_rate_label_layer, "❤");
  layer_add_child(window_layer, text_layer_get_layer(s_heart_rate_label_layer));
}

static void window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_debug_layer);
  text_layer_destroy(s_sample_indicator_layer);
  text_layer_destroy(s_heart_rate_layer);
  text_layer_destroy(s_heart_rate_label_layer);
  text_layer_destroy(s_readiness_layer);
  text_layer_destroy(s_readiness_label_layer);
  text_layer_destroy(s_sleep_layer);
  text_layer_destroy(s_sleep_label_layer);
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
  
  // If we received any real data, hide the sample indicator
  s_using_sample_data = false;
  update_sample_indicator();
  
  // Mark that real data was received and clear debug message after 10 seconds
  s_real_data_received = true;
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
  
  APP_LOG(APP_LOG_LEVEL_INFO, "Oura Stats Watchface initialized");
}

static void deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
