#include <pebble.h>
 
Window *window;
static GBitmap *background_image;
static BitmapLayer *background_layer;
TextLayer *text_layer;
TextLayer *text2_layer;
TextLayer *text3_layer;
TextLayer *date_layer;
TextLayer *date2_layer;
TextLayer *battery_layer;
TextLayer *connection_layer;

// Weather layers including city, icon and temperature
TextLayer *city_layer;
BitmapLayer *icon_layer;
GBitmap *icon_bitmap;
TextLayer *temp_layer;

InverterLayer *inverter_layer = NULL;

// Weather icons
static const uint32_t WEATHER_ICONS[] = {
RESOURCE_ID_CLEAR_DAY,
RESOURCE_ID_CLEAR_NIGHT,
RESOURCE_ID_WINDY,
RESOURCE_ID_COLD,
RESOURCE_ID_PARTLY_CLOUDY_DAY,
RESOURCE_ID_PARTLY_CLOUDY_NIGHT,
RESOURCE_ID_HAZE,
RESOURCE_ID_CLOUD,
RESOURCE_ID_RAIN,
RESOURCE_ID_SNOW,
RESOURCE_ID_HAIL,
RESOURCE_ID_CLOUDY,
RESOURCE_ID_STORM,
RESOURCE_ID_NA,
};

enum WeatherKey {
WEATHER_ICON_KEY = 0x0, // TUPLE_INT
WEATHER_TEMPERATURE_KEY = 0x1, // TUPLE_CSTRING
WEATHER_CITY_KEY = 0x2
};

static AppSync sync;
static uint8_t sync_buffer[64];

static void sync_tuple_changed_callback(const uint32_t key,
const Tuple* new_tuple,
const Tuple* old_tuple,
void* context) {
// App Sync keeps new_tuple in sync_buffer, so we may use it directly
switch (key) {
case WEATHER_ICON_KEY:
if (icon_bitmap) {
gbitmap_destroy(icon_bitmap);
}
icon_bitmap = gbitmap_create_with_resource(
WEATHER_ICONS[new_tuple->value->uint8]);
bitmap_layer_set_bitmap(icon_layer, icon_bitmap);
break;
case WEATHER_TEMPERATURE_KEY:
text_layer_set_text(temp_layer, new_tuple->value->cstring);
break;
case WEATHER_CITY_KEY:
text_layer_set_text(city_layer, new_tuple->value->cstring);
break;
}
}

// Battery status handler
static void handle_battery(BatteryChargeState charge_state) {
  static char battery_text[] = "100%";

  if (charge_state.is_charging) {
    snprintf(battery_text, sizeof(battery_text), "charging");
  } else {
    snprintf(battery_text, sizeof(battery_text), "%d%%", charge_state.charge_percent);
  }
  text_layer_set_text(battery_layer, battery_text);
}

// Time and date handler
void handle_timechanges(struct tm *tick_time, TimeUnits units_changed) {
static char time_buffer[10];
static char time_min[10];
static char time_when[10];
static char date_buffer[10];
static char date_big[10];
strftime(time_buffer, sizeof(time_buffer), "%I", tick_time);
text_layer_set_text(text_layer, time_buffer);
strftime(time_min, sizeof(time_min), "%M", tick_time);
text_layer_set_text(text2_layer, time_min);
strftime(time_when, sizeof(time_when), "%p", tick_time);
text_layer_set_text(text3_layer, time_when);
strftime(date_buffer, sizeof(date_buffer), "%A", tick_time);
text_layer_set_text(date_layer, date_buffer);
strftime(date_big, sizeof(date_big), "%d", tick_time);
text_layer_set_text(date2_layer, date_big);
handle_battery(battery_state_service_peek());
}

// Bluetooth handler
static void handle_bluetooth(bool connected) {
  text_layer_set_text(connection_layer, connected ? "connected" : "");
}
 
void handle_init(void) {
// Create a window and text layer
window = window_create();
window_set_fullscreen(window, true);
window_set_background_color(window, GColorBlack);
  
// Add background
Layer *window_layer = window_get_root_layer(window);
background_image = gbitmap_create_with_resource(RESOURCE_ID_background);
background_layer = bitmap_layer_create(layer_get_frame(window_layer));
bitmap_layer_set_bitmap(background_layer, background_image);
layer_add_child(window_layer, bitmap_layer_get_layer(background_layer));

text_layer = text_layer_create(GRect(72, 84, 72, 30));
text2_layer = text_layer_create(GRect(72, 108, 72, 30));
text3_layer = text_layer_create(GRect(72, 136, 72, 14));
 
// Set the text, font, and text alignment
text_layer_set_font(text_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
text_layer_set_font(text2_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
text_layer_set_font(text3_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
text_layer_set_background_color(text_layer, GColorClear);
text_layer_set_background_color(text2_layer, GColorClear);
text_layer_set_background_color(text3_layer, GColorClear);
text_layer_set_text_color(text_layer, GColorBlack);
text_layer_set_text_color(text2_layer, GColorBlack);
text_layer_set_text_color(text3_layer, GColorBlack);
text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);
text_layer_set_text_alignment(text2_layer, GTextAlignmentCenter);
text_layer_set_text_alignment(text3_layer, GTextAlignmentCenter);
 
// Add the text layer to the window
layer_add_child(window_get_root_layer(window), text_layer_get_layer(text_layer));
layer_add_child(window_get_root_layer(window), text_layer_get_layer(text2_layer));
layer_add_child(window_get_root_layer(window), text_layer_get_layer(text3_layer));

// Date
date_layer = text_layer_create(GRect(7, 24, 60, 16));
date2_layer = text_layer_create(GRect(6, 40, 60, 34));
text_layer_set_background_color(date_layer, GColorClear);
text_layer_set_background_color(date2_layer, GColorClear);
text_layer_set_text_color(date_layer, GColorWhite);
text_layer_set_text_color(date2_layer, GColorWhite);
text_layer_set_text_alignment(date_layer, GTextAlignmentCenter);
text_layer_set_text_alignment(date2_layer, GTextAlignmentCenter);
text_layer_set_font(date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
text_layer_set_font(date2_layer, fonts_get_system_font(FONT_KEY_BITHAM_34_MEDIUM_NUMBERS));
layer_add_child(window_get_root_layer(window), text_layer_get_layer(date_layer));
layer_add_child(window_get_root_layer(window), text_layer_get_layer(date2_layer));

// BT connection
connection_layer = text_layer_create(GRect(0, 120, 72, 48));
text_layer_set_text_color(connection_layer, GColorWhite);
text_layer_set_background_color(connection_layer, GColorClear);
text_layer_set_font(connection_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
text_layer_set_text_alignment(connection_layer, GTextAlignmentCenter);
handle_bluetooth(bluetooth_connection_service_peek());

// Battery
battery_layer = text_layer_create(GRect(0, 100, 72, 48));
text_layer_set_text_color(battery_layer, GColorWhite);
text_layer_set_background_color(battery_layer, GColorClear);
text_layer_set_font(battery_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
text_layer_set_text_alignment(battery_layer, GTextAlignmentCenter);
text_layer_set_text(battery_layer, "100%");

// Setup weather bar
icon_layer = bitmap_layer_create(GRect(80, 25, 58, 40));
bitmap_layer_set_background_color(icon_layer, GColorClear);
layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(icon_layer));

city_layer = text_layer_create(GRect(80, 11, 58, 14));
text_layer_set_text_color(city_layer, GColorWhite);
text_layer_set_background_color(city_layer, GColorClear);
text_layer_set_font(city_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
text_layer_set_text_alignment(city_layer, GTextAlignmentCenter);
layer_add_child(window_get_root_layer(window), text_layer_get_layer(city_layer)); 
  
temp_layer = text_layer_create(GRect(80, 60, 58, 20));
text_layer_set_text_color(temp_layer, GColorWhite);
text_layer_set_background_color(temp_layer, GColorClear);
text_layer_set_font(temp_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
text_layer_set_text_alignment(temp_layer, GTextAlignmentCenter);
layer_add_child(window_get_root_layer(window), text_layer_get_layer(temp_layer));

// Setup messaging
const int inbound_size = 64;
const int outbound_size = 64;
app_message_open(inbound_size, outbound_size);

Tuplet initial_values[] = {
TupletInteger(WEATHER_ICON_KEY, (uint8_t) 13),
TupletCString(WEATHER_TEMPERATURE_KEY, ""),
TupletCString(WEATHER_CITY_KEY, "N/A")
};
app_sync_init(&sync, sync_buffer, sizeof(sync_buffer), initial_values,
ARRAY_LENGTH(initial_values), sync_tuple_changed_callback,
NULL, NULL);  

// inverter_layer = inverter_layer_create(GRect(0, 0, 144, 168));
// layer_add_child(window_get_root_layer(window), inverter_layer_get_layer(inverter_layer));

time_t now = time(NULL);
handle_timechanges(localtime(&now), SECOND_UNIT);
tick_timer_service_subscribe(SECOND_UNIT, handle_timechanges);

battery_state_service_subscribe(&handle_battery);
bluetooth_connection_service_subscribe(&handle_bluetooth);

layer_add_child(window_get_root_layer(window), text_layer_get_layer(connection_layer));
layer_add_child(window_get_root_layer(window), text_layer_get_layer(battery_layer));

// Push the window
window_stack_push(window, true);
 
// App Logging!
APP_LOG(APP_LOG_LEVEL_DEBUG, "Applog:");
}
 
void handle_deinit(void) {
// Destroy the text layer
battery_state_service_unsubscribe();
bluetooth_connection_service_unsubscribe();
text_layer_destroy(text_layer);
text_layer_destroy(text2_layer);
text_layer_destroy(text3_layer);
text_layer_destroy(date_layer);
text_layer_destroy(date2_layer);
text_layer_destroy(connection_layer);
text_layer_destroy(battery_layer);
text_layer_destroy(temp_layer);
text_layer_destroy(city_layer);
bitmap_layer_destroy(icon_layer);
// inverter_layer_destroy(inverter_layer);
// Destroy the window
window_destroy(window);
}
 
int main(void) {
handle_init();
app_event_loop();
handle_deinit();
} 