#include "x4_hal.h"
#include "x4_epd.h"
#include "xteink_epd.h"
#include "xteink_board.h"
#include "xteink_peripherals.h"
#include "x4_input_queue.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "driver/sdspi_host.h"
#if XTEINK_ESP32S3
#include "driver/sdmmc_host.h"
#endif
#include "sdmmc_cmd.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>

static spi_device_handle_t spi;
static bool spi_bus_ready, selected, panel_ready, light_ready;
static bool cool_ready, warm_ready;
static XteinkEpd epd;
static adc_oneshot_unit_handle_t adc;
static i2c_master_bus_handle_t i2c;
static i2c_master_dev_handle_t touch_device, gauge_device;
static sdmmc_card_t *sd;
static TaskHandle_t input_task;
static volatile bool stop_input, input_stopped;
static portMUX_TYPE input_lock=portMUX_INITIALIZER_UNLOCKED;
static X4InputQueue presses;
static XteinkTouch touch;
static bool touch_pressed;
static uint8_t stable, candidate;
static int front_adc=-1, side_adc=-1, battery_mv=-1;
static int64_t changed_at, battery_at;

static void sleep_ms(unsigned int ms) { vTaskDelay(pdMS_TO_TICKS(ms) ? pdMS_TO_TICKS(ms) : 1); }
static uint64_t time_us(void) { return esp_timer_get_time(); }
static bool busy(void) { return gpio_get_level(EPD_BUSY)==(XTEINK_PANEL==XTEINK_SSD1677); }
static void reset(bool high) { gpio_set_level(EPD_RST,high); }
static void end(void)
{
    if (!selected) return;
    gpio_set_level(EPD_CS,1);
    spi_device_release_bus(spi);
    selected=false;
}
static bool data(const uint8_t *bytes, size_t size)
{
    while (size) {
        size_t count=size>4092 ? 4092 : size;
        spi_transaction_t tx={.length=count*8,.tx_buffer=bytes};
        if (spi_device_polling_transmit(spi,&tx)!=ESP_OK) return false;
        bytes+=count; size-=count;
    }
    return true;
}
static bool begin(uint8_t command)
{
    end();
    if (spi_device_acquire_bus(spi,portMAX_DELAY)!=ESP_OK) return false;
    selected=true;
    gpio_set_level(EPD_DC,0); gpio_set_level(EPD_CS,0);
    bool ok=data(&command,1);
    gpio_set_level(EPD_DC,1);
    return ok;
}
static bool ssd_write(bool is_data, const uint8_t *bytes, size_t size)
{
    if (spi_device_acquire_bus(spi,portMAX_DELAY)!=ESP_OK) return false;
    selected=true;
    gpio_set_level(EPD_DC,is_data); gpio_set_level(EPD_CS,0);
    bool ok=data(bytes,size); end(); return ok;
}
static bool output(int pin, bool high)
{
    gpio_config_t config={.pin_bit_mask=1ULL<<pin,.mode=GPIO_MODE_OUTPUT};
    // Set the latch before enabling output so active-low rails do not pulse on.
    return gpio_set_level(pin,high)==ESP_OK && gpio_config(&config)==ESP_OK;
}
static bool input(int pin, bool pullup)
{
    gpio_config_t config={.pin_bit_mask=1ULL<<pin,.mode=GPIO_MODE_INPUT,.pull_up_en=pullup};
    return gpio_config(&config)==ESP_OK;
}
static i2c_master_dev_handle_t i2c_device(uint8_t address)
{ return address==0x5d ? touch_device : gauge_device; }
static bool read_reg(uint8_t address, uint16_t reg, unsigned int n, uint8_t *bytes, size_t size)
{
    i2c_master_dev_handle_t device=i2c_device(address);
    uint8_t command[2]={reg>>8,reg};
    return device && i2c_master_transmit_receive(device,command+(n==1),n,bytes,size,20)==ESP_OK;
}
static bool write_reg(uint8_t address, uint16_t reg, unsigned int n, const uint8_t *bytes, size_t size)
{
    i2c_master_dev_handle_t device=i2c_device(address);
    if (!device || size>16) return false;
    uint8_t command[18]={reg>>8,reg};
    if (n==1) command[0]=reg;
    memcpy(command+n,bytes,size);
    return i2c_master_transmit(device,command,n+size,20)==ESP_OK;
}
static const XteinkI2c peripheral_io={read_reg,write_reg};

bool xteink_hal_frontlight(unsigned int brightness, unsigned int warmth)
{
    if (!light_ready || brightness>100 || warmth>100) return false;
    unsigned int cool,warm;
    xteink_frontlight_duty(brightness,warmth,&cool,&warm);
    return ledc_set_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,cool)==ESP_OK &&
        ledc_update_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0)==ESP_OK &&
        ledc_set_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_1,warm)==ESP_OK &&
        ledc_update_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_1)==ESP_OK;
}
static void sample_input(void *unused)
{
    (void)unused;
    XteinkTouch next={0};
    while (!stop_input) {
        int a=-1,b=-1;
        uint8_t raw=0;
#if XTEINK_DEVICE == XTEINK_X3
        if (adc_oneshot_read(adc,ADC_CHANNEL_1,&a)!=ESP_OK) a=-1;
        if (adc_oneshot_read(adc,ADC_CHANNEL_2,&b)!=ESP_OK) b=-1;
        raw=x4_decode_buttons(a,b,!gpio_get_level(3));
#else
        static const int pins[X4_BUTTON_COUNT]={
#if XTEINK_DEVICE == XTEINK_X4_CLASSIC
            9,8,5,2,0,7,3
#else
            -1,-1,-1,-1,0,7,3
#endif
        };
        for (int i=0; i<X4_BUTTON_COUNT; i++) if (pins[i]>=0 && !gpio_get_level(pins[i])) raw|=1u<<i;
        if (touch_device) {
            xteink_gt911_poll(&peripheral_io,&next);
            if (next.home) raw|=1u<<X4_BACK;
        }
#endif
        int64_t now=esp_timer_get_time();
        int mv=-1;
        bool battery_update=now>=battery_at;
        if (battery_update) { mv=xteink_battery_mv(&peripheral_io,XTEINK_DEVICE==XTEINK_X3); battery_at=now+1000000; }
        if (raw!=candidate) { candidate=raw; changed_at=now; }
        portENTER_CRITICAL(&input_lock);
        front_adc=a; side_adc=b;
        if (battery_update) battery_mv=mv;
        if (now-changed_at>=15000) {
            uint8_t edges=candidate&~stable;
            for (int i=0; i<X4_BUTTON_COUNT; i++) if (edges&(1u<<i)) x4_input_push(&presses,i);
            stable=candidate;
        }
        if (next.down && !touch.down) touch_pressed=true;
        // Preserve the last touch location across its release.
        if (!next.down) { next.x=touch.x; next.y=touch.y; }
        touch=next;
        portEXIT_CRITICAL(&input_lock);
        sleep_ms(5);
    }
    input_stopped=true;
    vTaskDelete(NULL);
}
static void init_i2c(void)
{
    i2c_master_bus_config_t config={.i2c_port=I2C_NUM_0,.sda_io_num=I2C_SDA,.scl_io_num=I2C_SCL,
        .clk_source=I2C_CLK_SRC_DEFAULT,.glitch_ignore_cnt=7,.flags.enable_internal_pullup=true};
    if (i2c_new_master_bus(&config,&i2c)!=ESP_OK) { i2c=NULL; return; }
    i2c_device_config_t gauge={.dev_addr_length=I2C_ADDR_BIT_LEN_7,.device_address=XTEINK_DEVICE==XTEINK_X3 ? 0x55 : 0x63,.scl_speed_hz=400000};
    if (i2c_master_bus_add_device(i2c,&gauge,&gauge_device)!=ESP_OK) gauge_device=NULL;
#if XTEINK_HAS_TOUCH
    if (!output(2,false) || !output(4,false) || !output(10,false)) return;
    sleep_ms(10); gpio_set_level(4,1); sleep_ms(50);
    gpio_set_level(10,0); sleep_ms(50); input(10,false);
    gauge.device_address=0x5d;
    if (i2c_master_probe(i2c,0x5d,50)==ESP_OK && i2c_master_bus_add_device(i2c,&gauge,&touch_device)!=ESP_OK) touch_device=NULL;
#endif
}
static void mount_sd(void)
{
    esp_vfs_fat_sdmmc_mount_config_t mount={.format_if_mount_failed=false,.max_files=4,.allocation_unit_size=16384};
#if XTEINK_DEVICE == XTEINK_X3
    sdmmc_host_t host=SDSPI_HOST_DEFAULT(); host.slot=SPI2_HOST; host.max_freq_khz=20000;
    sdspi_device_config_t slot=SDSPI_DEVICE_CONFIG_DEFAULT(); slot.gpio_cs=12; slot.host_id=SPI2_HOST;
    if (esp_vfs_fat_sdspi_mount("/sd",&host,&slot,&mount,&sd)!=ESP_OK) sd=NULL;
#else
    sdmmc_host_t host=SDMMC_HOST_DEFAULT(); host.max_freq_khz=SDMMC_FREQ_DEFAULT;
    sdmmc_slot_config_t slot=SDMMC_SLOT_CONFIG_DEFAULT();
    slot.clk=41; slot.cmd=42; slot.d0=40; slot.width=1; slot.flags=SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    for (int attempt=0; attempt<3 && !sd; attempt++) {
        gpio_set_level(SD_ENABLE,1); sleep_ms(80); gpio_set_level(SD_ENABLE,0); sleep_ms(120);
        if (esp_vfs_fat_sdmmc_mount("/sd",&host,&slot,&mount,&sd)!=ESP_OK) { sd=NULL; continue; }
        uint8_t *sector=heap_caps_malloc(512,MALLOC_CAP_DMA);
        bool ok=sector && sdmmc_read_sectors(sd,sector,0,1)==ESP_OK;
        free(sector);
        if (!ok) { esp_vfs_fat_sdcard_unmount("/sd",sd); sd=NULL; }
    }
#endif
    if (!sd) ESP_LOGW("xteink","SD unavailable; continuing without storage");
}
bool x4_hal_init(const char *title)
{
    (void)title;
    stable=candidate=0; presses=(X4InputQueue){0}; touch=(XteinkTouch){0}; touch_pressed=false;
    front_adc=side_adc=battery_mv=-1; changed_at=time_us(); battery_at=0; stop_input=input_stopped=false;
    ESP_LOGW("xteink","Experimental %s firmware: hardware UNTESTED",XTEINK_DEVICE_NAME);
#if XTEINK_ESP32S3
    if (!output(1,true)) goto fail;
#endif
    if (!output(SD_ENABLE,SD_ENABLE_LEVEL) || !output(EPD_CS,true) || !output(EPD_DC,true) ||
        !output(EPD_RST,true) || !input(EPD_BUSY,false) || !input(3,true)) goto fail;
#if XTEINK_DEVICE == XTEINK_X3
    if (!output(12,true)) goto fail;
    adc_oneshot_unit_init_cfg_t unit={.unit_id=ADC_UNIT_1};
    adc_oneshot_chan_cfg_t channel={.atten=ADC_ATTEN_DB_12,.bitwidth=ADC_BITWIDTH_12};
    if (adc_oneshot_new_unit(&unit,&adc)!=ESP_OK || adc_oneshot_config_channel(adc,ADC_CHANNEL_1,&channel)!=ESP_OK ||
        adc_oneshot_config_channel(adc,ADC_CHANNEL_2,&channel)!=ESP_OK) goto fail;
#else
    if (!input(0,true) || !input(7,true)) goto fail;
#if XTEINK_DEVICE == XTEINK_X4_CLASSIC
    if (!input(9,true) || !input(8,true) || !input(5,true) || !input(2,true)) goto fail;
#endif
#endif
    spi_bus_config_t bus={.mosi_io_num=EPD_MOSI,.miso_io_num=XTEINK_DEVICE==XTEINK_X3 ? 7 : -1,
        .sclk_io_num=EPD_CLK,.quadwp_io_num=-1,.quadhd_io_num=-1,.max_transfer_sz=4092};
    if (spi_bus_initialize(SPI2_HOST,&bus,SPI_DMA_CH_AUTO)!=ESP_OK) goto fail;
    spi_bus_ready=true;
    spi_device_interface_config_t device={.clock_speed_hz=10000000,.mode=0,.spics_io_num=-1,.queue_size=1};
    if (spi_bus_add_device(SPI2_HOST,&device,&spi)!=ESP_OK) goto fail;
#if XTEINK_PANEL == XTEINK_SSD1677
    X4EpdIO panel={ssd_write,busy,reset,sleep_ms,time_us};
    if (!x4_epd_init(&panel)) goto fail;
#else
    XteinkEpdIO panel={begin,data,end,busy,reset,sleep_ms,time_us};
    XteinkPanel controller=XTEINK_PANEL==XTEINK_PANEL_UC8253 ? XTEINK_UC8253 :
        XTEINK_PANEL==XTEINK_PANEL_UC8179 ? XTEINK_UC8179 :
        XTEINK_DEVICE==XTEINK_X3 ? XTEINK_UC8279_X3 : XTEINK_UC8279_X4;
    if (!xteink_epd_init(&epd,&panel,controller,XTEINK_DEVICE==XTEINK_X4_CLASSIC)) goto fail;
#endif
    panel_ready=true;
    init_i2c();
#if XTEINK_HAS_FRONTLIGHT
    ledc_timer_config_t timer={.speed_mode=LEDC_LOW_SPEED_MODE,.duty_resolution=LEDC_TIMER_10_BIT,
        .timer_num=LEDC_TIMER_0,.freq_hz=25000,.clk_cfg=LEDC_AUTO_CLK};
    if (output(8,false) && output(9,false) && ledc_timer_config(&timer)==ESP_OK) {
        ledc_channel_config_t cool={.gpio_num=8,.speed_mode=LEDC_LOW_SPEED_MODE,.channel=LEDC_CHANNEL_0,.timer_sel=LEDC_TIMER_0};
        ledc_channel_config_t warm=cool; warm.gpio_num=9; warm.channel=LEDC_CHANNEL_1;
        cool_ready=ledc_channel_config(&cool)==ESP_OK;
        warm_ready=ledc_channel_config(&warm)==ESP_OK;
        light_ready=cool_ready && warm_ready;
    }
#endif
    mount_sd();
    if (xTaskCreate(sample_input,"xteink_input",4096,NULL,5,&input_task)!=pdPASS) goto fail;
    return true;
fail:
    ESP_LOGE("xteink","Hardware initialization failed");
    x4_hal_close(); return false;
}
void x4_hal_close(void)
{
    if (input_task) { stop_input=true; while (!input_stopped) sleep_ms(1); input_task=NULL; }
    if (cool_ready) ledc_stop(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,0);
    if (warm_ready) ledc_stop(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_1,0);
    cool_ready=warm_ready=light_ready=false;
    if (sd) { esp_vfs_fat_sdcard_unmount("/sd",sd); sd=NULL; }
    if (panel_ready) {
#if XTEINK_PANEL == XTEINK_SSD1677
        x4_epd_sleep();
#else
        xteink_epd_sleep(&epd);
#endif
        panel_ready=false;
    }
    end();
    if (spi) { spi_bus_remove_device(spi); spi=NULL; }
    if (spi_bus_ready) { spi_bus_free(SPI2_HOST); spi_bus_ready=false; }
    if (adc) { adc_oneshot_del_unit(adc); adc=NULL; }
    if (touch_device) { i2c_master_bus_rm_device(touch_device); touch_device=NULL; }
    if (gauge_device) { i2c_master_bus_rm_device(gauge_device); gauge_device=NULL; }
    if (i2c) { i2c_del_master_bus(i2c); i2c=NULL; }
    gpio_set_level(SD_ENABLE,!SD_ENABLE_LEVEL);
#if XTEINK_HAS_TOUCH
    gpio_set_level(2,1);
#endif
}
bool x4_hal_begin_present(const uint8_t *frame, const uint8_t *previous, bool full)
{
#if XTEINK_PANEL == XTEINK_SSD1677
    return x4_epd_begin(frame,previous,full);
#else
    (void)previous; return xteink_epd_begin(&epd,frame,full);
#endif
}
bool x4_hal_present_busy(void)
{
#if XTEINK_PANEL == XTEINK_SSD1677
    return x4_epd_busy();
#else
    return xteink_epd_busy(&epd);
#endif
}
bool x4_hal_finish_present(void)
{
#if XTEINK_PANEL == XTEINK_SSD1677
    return x4_epd_finish();
#else
    return xteink_epd_finish(&epd);
#endif
}
bool x4_hal_display_idle(void)
{
#if XTEINK_PANEL == XTEINK_SSD1677
    return !panel_ready || x4_epd_idle();
#else
    return !panel_ready || xteink_epd_idle(&epd);
#endif
}
X4Input x4_hal_poll(void)
{
    portENTER_CRITICAL(&input_lock);
    X4Input result={.down=stable,.touch_down=touch.down,.touch_pressed=touch_pressed,.touch_x=touch.x,.touch_y=touch.y};
    touch_pressed=false;
    x4_input_drain(&presses,&result);
    portEXIT_CRITICAL(&input_lock);
    return result;
}
double x4_hal_time(void) { return time_us()/1000000.0; }
void x4_hal_sleep(double seconds) { sleep_ms((unsigned int)(seconds*1000)); }
const char *x4_hal_storage(void) { return sd ? "/sd" : NULL; }
X4Status x4_hal_status(void)
{
    portENTER_CRITICAL(&input_lock);
    X4Status result={.front_adc=front_adc,.side_adc=side_adc,.battery_mv=battery_mv,
        .buttons=stable,.dropped_input_presses=presses.dropped};
    portEXIT_CRITICAL(&input_lock);
    result.free_heap=esp_get_free_heap_size(); result.minimum_heap=esp_get_minimum_free_heap_size();
    result.largest_block=heap_caps_get_largest_free_block(MALLOC_CAP_8BIT); result.storage_mounted=sd!=NULL;
    return result;
}
