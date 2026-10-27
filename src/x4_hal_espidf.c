#include "x4_hal.h"
#include "x4_epd.h"
#include "x4_input_queue.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/sdspi_host.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static spi_device_handle_t spi;
static adc_oneshot_unit_handle_t adc;
static esp_timer_handle_t input_timer;
static adc_cali_handle_t battery_calibration;
static sdmmc_card_t *sd;
static bool bus_initialized, panel_initialized;
static portMUX_TYPE input_lock = portMUX_INITIALIZER_UNLOCKED;
static uint8_t stable, candidate;
static X4InputQueue presses;
static int64_t changed_at, battery_sample_at;
static int last_front = -1, last_side = -1, battery_mv = -1;

static void sleep_ms(unsigned int ms) { vTaskDelay(pdMS_TO_TICKS(ms) ? pdMS_TO_TICKS(ms) : 1); }
static uint64_t time_us(void) { return esp_timer_get_time(); }
static bool busy(void) { return gpio_get_level(GPIO_NUM_6); }
static void reset(bool high) { gpio_set_level(GPIO_NUM_5, high); }
static bool write_spi(bool data, const uint8_t *bytes, size_t size)
{
    if (spi_device_acquire_bus(spi, portMAX_DELAY) != ESP_OK) return false;
    gpio_set_level(GPIO_NUM_4, data);
    gpio_set_level(GPIO_NUM_21, 0);
    bool ok = true;
    while (size && ok) {
        size_t length = size > 4092 ? 4092 : size;
        spi_transaction_t transaction = { .length = length*8, .tx_buffer = bytes };
        ok = spi_device_transmit(spi, &transaction) == ESP_OK;
        bytes += length; size -= length;
    }
    gpio_set_level(GPIO_NUM_21, 1);
    spi_device_release_bus(spi);
    return ok;
}

static void sample_buttons(void *arg)
{
    (void)arg;
    int a = -1, b = -1;
    if (adc_oneshot_read(adc, ADC_CHANNEL_1, &a) != ESP_OK) a = -1;
    if (adc_oneshot_read(adc, ADC_CHANNEL_2, &b) != ESP_OK) b = -1;
    uint8_t raw = x4_decode_buttons(a, b, !gpio_get_level(GPIO_NUM_3));
    int64_t now = esp_timer_get_time();
    int mv = battery_mv;
    if (battery_calibration && now >= battery_sample_at) {
        int raw_battery, voltage;
        if (adc_oneshot_read(adc, ADC_CHANNEL_0, &raw_battery) == ESP_OK &&
            adc_cali_raw_to_voltage(battery_calibration, raw_battery, &voltage) == ESP_OK) mv = voltage*2;
        battery_sample_at = now + 1000000;
    }
    portENTER_CRITICAL(&input_lock);
    last_front = a; last_side = b; battery_mv = mv;
    portEXIT_CRITICAL(&input_lock);
    if (raw != candidate) { candidate = raw; changed_at = now; }
    if (now - changed_at >= 15000) {
        portENTER_CRITICAL(&input_lock);
        uint8_t edges=candidate & ~stable;
        for (int i=0; i<X4_BUTTON_COUNT; i++) if (edges & (1u<<i)) x4_input_push(&presses,i);
        stable = candidate;
        portEXIT_CRITICAL(&input_lock);
    }
}

bool x4_hal_init(const char *title)
{
    (void)title;
    stable = candidate = 0;
    memset(&presses,0,sizeof(presses));
    last_front = last_side = battery_mv = -1;
    battery_sample_at = 0;
    changed_at = esp_timer_get_time();
    gpio_config_t output = { .pin_bit_mask = (1ULL<<4)|(1ULL<<5)|(1ULL<<12)|(1ULL<<21), .mode = GPIO_MODE_OUTPUT };
    if (gpio_config(&output) != ESP_OK) goto fail;
    gpio_set_level(GPIO_NUM_12, 1); gpio_set_level(GPIO_NUM_21, 1);
    gpio_config_t input = { .pin_bit_mask = 1ULL<<6, .mode = GPIO_MODE_INPUT };
    if (gpio_config(&input) != ESP_OK) goto fail;
    input.pin_bit_mask = 1ULL<<3; input.pull_up_en = GPIO_PULLUP_ENABLE;
    if (gpio_config(&input) != ESP_OK) goto fail;
    spi_bus_config_t config = { .mosi_io_num = 10, .miso_io_num = 7, .sclk_io_num = 8,
        .quadwp_io_num = -1, .quadhd_io_num = -1, .max_transfer_sz = 4092 };
    if (spi_bus_initialize(SPI2_HOST, &config, SPI_DMA_CH_AUTO) != ESP_OK) goto fail;
    bus_initialized = true;
    spi_device_interface_config_t device = { .clock_speed_hz = 20000000, .mode = 0, .spics_io_num = -1, .queue_size = 1 };
    if (spi_bus_add_device(SPI2_HOST, &device, &spi) != ESP_OK) goto fail;
    X4EpdIO io = { write_spi, busy, reset, sleep_ms, time_us };
    if (!x4_epd_init(&io)) goto fail;
    panel_initialized = true;
    adc_oneshot_unit_init_cfg_t unit = { .unit_id = ADC_UNIT_1 };
    if (adc_oneshot_new_unit(&unit, &adc) != ESP_OK) goto fail;
    adc_oneshot_chan_cfg_t channel = { .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_12 };
    if (adc_oneshot_config_channel(adc, ADC_CHANNEL_1, &channel) != ESP_OK ||
        adc_oneshot_config_channel(adc, ADC_CHANNEL_2, &channel) != ESP_OK) goto fail;
    if (adc_oneshot_config_channel(adc, ADC_CHANNEL_0, &channel) == ESP_OK) {
        adc_cali_curve_fitting_config_t cal = { .unit_id = ADC_UNIT_1, .chan = ADC_CHANNEL_0,
            .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_12 };
        if (adc_cali_create_scheme_curve_fitting(&cal, &battery_calibration) != ESP_OK) battery_calibration = NULL;
    }
    esp_timer_create_args_t timer = { .callback = sample_buttons, .name = "x4_buttons" };
    if (esp_timer_create(&timer, &input_timer) != ESP_OK || esp_timer_start_periodic(input_timer, 5000) != ESP_OK) goto fail;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;
    host.max_freq_khz = 20000;
    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.gpio_cs = GPIO_NUM_12; slot.host_id = SPI2_HOST;
    esp_vfs_fat_sdmmc_mount_config_t mount = { .format_if_mount_failed = false, .max_files = 4, .allocation_unit_size = 16384 };
    esp_err_t err = esp_vfs_fat_sdspi_mount("/sd", &host, &slot, &mount, &sd);
    if (err != ESP_OK) { sd = NULL; ESP_LOGW("x4", "SD unavailable: %s", esp_err_to_name(err)); }
    return true;
fail:
    ESP_LOGE("x4", "Hardware initialization failed");
    x4_hal_close();
    return false;
}

void x4_hal_close(void)
{
    if (input_timer) { esp_timer_stop(input_timer); esp_timer_delete(input_timer); input_timer = NULL; }
    if (battery_calibration) { adc_cali_delete_scheme_curve_fitting(battery_calibration); battery_calibration = NULL; }
    if (adc) { adc_oneshot_del_unit(adc); adc = NULL; }
    if (sd) { esp_vfs_fat_sdcard_unmount("/sd", sd); sd = NULL; }
    if (panel_initialized) { x4_epd_sleep(); panel_initialized = false; }
    if (spi) { spi_bus_remove_device(spi); spi = NULL; }
    if (bus_initialized) { spi_bus_free(SPI2_HOST); bus_initialized = false; }
}
bool x4_hal_present(const uint8_t *frame, const uint8_t *previous, bool full)
{ return x4_epd_present(frame, previous, full); }
X4Input x4_hal_poll(void)
{
    portENTER_CRITICAL(&input_lock);
    X4Input input = { .down = stable };
    x4_input_drain(&presses,&input);
    portEXIT_CRITICAL(&input_lock);
    return input;
}
double x4_hal_time(void) { return esp_timer_get_time()/1000000.0; }
void x4_hal_sleep(double seconds) { sleep_ms((unsigned int)(seconds*1000.0)); }
const char *x4_hal_storage(void) { return sd ? "/sd" : NULL; }
X4Status x4_hal_status(void)
{
    portENTER_CRITICAL(&input_lock);
    X4Status status = { .front_adc = last_front, .side_adc = last_side, .battery_mv = battery_mv, .buttons = stable, .dropped_input_presses=presses.dropped };
    portEXIT_CRITICAL(&input_lock);
    status.free_heap = esp_get_free_heap_size();
    status.minimum_heap = esp_get_minimum_free_heap_size();
    status.largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    status.storage_mounted = sd != NULL;
    return status;
}
