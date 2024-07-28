/*
   MIT License

  Copyright (c) 2023 Felix Biego

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

  ______________  _____
  ___  __/___  /_ ___(_)_____ _______ _______
  __  /_  __  __ \__  / _  _ \__  __ `/_  __ \
  _  __/  _  /_/ /_  /  /  __/_  /_/ / / /_/ /
  /_/     /_.___/ /_/   \___/ _\__, /  \____/
                              /____/

*/

#define LGFX_USE_V1
#include "app_hal.h"
#include "Arduino.h"
#include <ArduinoJson.h>
#include <ChronosESP32.h>
#include <ESP32Time.h>
#include <LovyanGFX.hpp>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <Timber.h>

#include "ui/ui.h"
#include <lvgl.h>

#include "ui/custom_face.h"

#include "main.h"
#include "splash.h"

#include "FFat.h"
#include "FS.h"

#define FLASH FFat
#define F_NAME "FATFS"
#define buf_size 10

class LGFX : public lgfx::LGFX_Device {

  lgfx::Panel_GC9A01 _panel_instance;
  lgfx::Light_PWM _light_instance;
  lgfx::Bus_SPI _bus_instance;
  lgfx::Touch_CST816S _touch_instance;

public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();

      // SPIバスの設定
      cfg.spi_host = SPI; // 使用するSPIを選択  ESP32-S2,C3 : SPI2_HOST or
                          // SPI3_HOST / ESP32 : VSPI_HOST or HSPI_HOST
      // ※ ESP-IDFバージョンアップに伴い、VSPI_HOST ,
      // HSPI_HOSTの記述は非推奨になるため、エラーが出る場合は代わりにSPI2_HOST
      // , SPI3_HOSTを使用してください。
      cfg.spi_mode = 0; // SPI通信モードを設定 (0 ~ 3)
      cfg.freq_write =
          80000000; // 传输时的SPI时钟（最高80MHz，四舍五入为80MHz除以整数得到的值）
      cfg.freq_read = 20000000; // 接收时的SPI时钟
      cfg.spi_3wire = true; // 受信をMOSIピンで行う場合はtrueを設定
      cfg.use_lock = true;  // 使用事务锁时设置为 true
      cfg.dma_channel =
          SPI_DMA_CH_AUTO; // 使用するDMAチャンネルを設定 (0=DMA不使用 / 1=1ch /
                           // 2=ch / SPI_DMA_CH_AUTO=自動設定)
      // ※
      // ESP-IDFバージョンアップに伴い、DMAチャンネルはSPI_DMA_CH_AUTO(自動設定)が推奨になりました。1ch,2chの指定は非推奨になります。
      cfg.pin_sclk = SCLK; // SPIのSCLKピン番号を設定
      cfg.pin_mosi = MOSI; // SPIのCLKピン番号を設定
      cfg.pin_miso = MISO; // SPIのMISOピン番号を設定 (-1 = disable)
      cfg.pin_dc = DC;     // SPIのD/Cピン番号を設定  (-1 = disable)

      _bus_instance.config(cfg); // 設定値をバスに反映します。
      _panel_instance.setBus(&_bus_instance); // バスをパネルにセットします。
    }

    { // 表示パネル制御の設定を行います。
      auto cfg =
          _panel_instance.config(); // 表示パネル設定用の構造体を取得します。

      cfg.pin_cs = CS; // CSが接続されているピン番号   (-1 = disable)
      cfg.pin_rst = RST; // RSTが接続されているピン番号  (-1 = disable)
      cfg.pin_busy = -1; // BUSYが接続されているピン番号 (-1 = disable)

      // ※ 以下の設定値はパネル毎に一般的な初期値が設定さ
      // BUSYが接続されているピン番号 (-1 =
      // disable)れていますので、不明な項目はコメントアウトして試してみてください。

      cfg.memory_width = WIDTH; // ドライバICがサポートしている最大の幅
      cfg.memory_height = HEIGHT; // ドライバICがサポートしている最大の高さ
      cfg.panel_width = WIDTH;   // 実際に表示可能な幅
      cfg.panel_height = HEIGHT; // 実際に表示可能な高さ
      cfg.offset_x = OFFSET_X;   // パネルのX方向オフセット量
      cfg.offset_y = OFFSET_Y;   // パネルのY方向オフセット量
      cfg.offset_rotation = 0; // 值在旋转方向的偏移0~7（4~7是倒置的）
      cfg.dummy_read_pixel = 8; // 在读取像素之前读取的虚拟位数
      cfg.dummy_read_bits = 1; // 读取像素以外的数据之前的虚拟读取位数
      cfg.readable = false; // 如果可以读取数据，则设置为 true
      cfg.invert = true;    // 如果面板的明暗反转，则设置为 true
      cfg.rgb_order = RGB_ORDER; // 如果面板的红色和蓝色被交换，则设置为 true
      cfg.dlen_16bit = false; // 对于以 16 位单位发送数据长度的面板，设置为 true
      cfg.bus_shared = false; // 如果总线与 SD 卡共享，则设置为 true（使用
                              // drawJpgFile 等执行总线控制）

      _panel_instance.config(cfg);
    }

    { // Set backlight control. (delete if not necessary)
      auto cfg =
          _light_instance
              .config(); // Get the structure for backlight configuration.

      cfg.pin_bl = BL;     // pin number to which the backlight is connected
      cfg.invert = false;  // true to invert backlight brightness
      cfg.freq = 44100;    // backlight PWM frequency
      cfg.pwm_channel = 1; // PWM channel number to use

      _light_instance.config(cfg);
      _panel_instance.setLight(
          &_light_instance); // Sets the backlight to the panel.
    }

    { // タッチスクリーン制御の設定を行います。（必要なければ削除）
      auto cfg = _touch_instance.config();

      cfg.x_min = 0; // タッチスクリーンから得られる最小のX値(生の値)
      cfg.x_max = WIDTH; // タッチスクリーンから得られる最大のX値(生の値)
      cfg.y_min = 0; // タッチスクリーンから得られる最小のY値(生の値)
      cfg.y_max = HEIGHT; // タッチスクリーンから得られる最大のY値(生の値)
      cfg.pin_int = TP_INT; // INTが接続されているピン番号
      // cfg.pin_rst = TP_RST;
      cfg.bus_shared = false; // 画面と共通のバスを使用している場合 trueを設定
      cfg.offset_rotation =
          0; // 表示とタッチの向きのが一致しない場合の調整 0~7の値で設定
      cfg.i2c_port = 0;      // 使用するI2Cを選択 (0 or 1)
      cfg.i2c_addr = 0x15;   // I2Cデバイスアドレス番号
      cfg.pin_sda = I2C_SDA; // SDAが接続されているピン番号
      cfg.pin_scl = I2C_SCL; // SCLが接続されているピン番号
      cfg.freq = 400000;     // I2Cクロックを設定

      _touch_instance.config(cfg);
      _panel_instance.setTouch(
          &_touch_instance); // タッチスクリーンをパネルにセットします。
    }

    setPanel(&_panel_instance); // 使用するパネルをセットします。
  }
};

LGFX tft;

#ifdef CS_CONFIG
ChronosESP32 watch("Chronos C3", CS_CONFIG);
#else
ChronosESP32 watch("Chronos C3");
#endif

Preferences prefs;

static const uint32_t screenWidth = WIDTH;
static const uint32_t screenHeight = HEIGHT;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[2][screenWidth * buf_size];

bool weatherUpdate = true, notificationsUpdate = true;

ChronosTimer screenTimer;
ChronosTimer alertTimer;
ChronosTimer searchTimer;

lv_obj_t *lastActScr;

bool circular = false;
bool alertSwitch = false;
bool gameActive = false;

String customFacePaths[15];
int customFaceIndex;

// watchface transfer
int cSize, pos, recv;
uint32_t total, currentRecv;
bool last;
String fName;
uint8_t buf1[1024];
uint8_t buf2[1024];
static bool writeFile = false, transfer = false, wSwitch = true;
static int wLen1 = 0, wLen2 = 0;
bool start = false;
int lastCustom;

TaskHandle_t gameHandle = NULL;

void showAlert();
bool isDay();
void setTimeout(int i);

void hal_setup(void);
void hal_loop(void);

void update_faces();
void updateQrLinks();

void flashDrive_cb(lv_event_t *e);
void driveList_cb(lv_event_t *e);

void checkLocal();
void registerWatchface_cb(const char *name, const lv_img_dsc_t *preview,
                          lv_obj_t **watchface);
void registerCustomFace(const char *name, const lv_img_dsc_t *preview,
                        lv_obj_t **watchface, String path);

String hexString(uint8_t *arr, size_t len, bool caps = false,
                 String separator = "");

bool loadCustomFace(String file);
bool deleteCustomFace(String file);
bool readDialBytes(const char *path, uint8_t *data, size_t offset, size_t size);
bool isKnown(uint8_t id);
void parseDial(const char *path);
bool lvImgHeader(uint8_t *byteArray, uint8_t cf, uint16_t w, uint16_t h);

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area,
                   lv_color_t *color_p) {
  if (tft.getStartCount() == 0) {
    tft.endWrite();
  }

  tft.pushImageDMA(area->x1, area->y1, area->x2 - area->x1 + 1,
                   area->y2 - area->y1 + 1, (lgfx::swap565_t *)&color_p->full);
  lv_disp_flush_ready(disp); /* tell lvgl that flushing is done */
}

/*Read the touchpad*/
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {

  bool touched;
  uint8_t gesture;
  uint16_t touchX, touchY;
  RemoteTouch rt = watch.getTouch(); // remote touch
  if (rt.state) {
    // use remote touch when active
    touched = rt.state;
    touchX = rt.x;
    touchY = rt.y;
  } else {
    touched = tft.getTouch(&touchX, &touchY);
  }

  if (!touched) {
    data->state = LV_INDEV_STATE_REL;
  } else {
    data->state = LV_INDEV_STATE_PR;

    /*Set the coordinates*/
    data->point.x = touchX;
    data->point.y = touchY;
    screenTimer.time = millis();
    screenTimer.active = true;
  }
}

String heapUsage() {
  String usage;
  uint32_t total = ESP.getHeapSize();
  uint32_t free = ESP.getFreeHeap();
  usage += "Total: " + String(total);
  usage += "\tFree: " + String(free);
  usage += "\t" + String(((total - free) * 1.0) / total * 100, 2) + "%";
  return usage;
}

void *sd_open_cb(struct _lv_fs_drv_t *drv, const char *path,
                 lv_fs_mode_t mode) {
  char buf[256];
  sprintf(buf, "/%s", path);
  // Serial.print("path : ");
  // Serial.println(buf);

  File f;

  if (mode == LV_FS_MODE_WR) {
    f = FLASH.open(buf, FILE_WRITE);
  } else if (mode == LV_FS_MODE_RD) {
    f = FLASH.open(buf);
  } else if (mode == (LV_FS_MODE_WR | LV_FS_MODE_RD)) {
    f = FLASH.open(buf, FILE_WRITE);
  }

  if (!f) {
    return NULL; // Return NULL if the file failed to open
  }

  File *fp = new File(f); // Allocate the File object on the heap
  return (void *)fp;      // Return the pointer to the allocated File object
}

lv_fs_res_t sd_read_cb(struct _lv_fs_drv_t *drv, void *file_p, void *buf,
                       uint32_t btr, uint32_t *br) {
  lv_fs_res_t res = LV_FS_RES_NOT_IMP;
  File *fp = (File *)file_p;
  uint8_t *buffer = (uint8_t *)buf;

  // Serial.print("name sd_read_cb : ");
  // Serial.println(fp->name());
  *br = fp->read(buffer, btr);

  res = LV_FS_RES_OK;
  return res;
}

lv_fs_res_t sd_seek_cb(struct _lv_fs_drv_t *drv, void *file_p, uint32_t pos,
                       lv_fs_whence_t whence) {
  lv_fs_res_t res = LV_FS_RES_OK;
  File *fp = (File *)file_p;

  uint32_t actual_pos;

  switch (whence) {
  case LV_FS_SEEK_SET:
    actual_pos = pos;
    break;
  case LV_FS_SEEK_CUR:
    actual_pos = fp->position() + pos;
    break;
  case LV_FS_SEEK_END:
    actual_pos = fp->size() + pos;
    break;
  default:
    return LV_FS_RES_INV_PARAM; // Invalid parameter
  }

  if (!fp->seek(actual_pos)) {
    return LV_FS_RES_UNKNOWN; // Seek failed
  }

  // Serial.print("name sd_seek_cb : ");
  // Serial.println(fp->name());

  return res;
}

lv_fs_res_t sd_tell_cb(struct _lv_fs_drv_t *drv, void *file_p,
                       uint32_t *pos_p) {
  lv_fs_res_t res = LV_FS_RES_NOT_IMP;
  File *fp = (File *)file_p;

  *pos_p = fp->position();
  // Serial.print("name in sd_tell_cb : ");
  // Serial.println(fp->name());
  res = LV_FS_RES_OK;
  return res;
}

lv_fs_res_t sd_close_cb(struct _lv_fs_drv_t *drv, void *file_p) {
  File *fp = (File *)file_p;

  fp->close();
  // delete fp;  // Free the allocated memory

  return LV_FS_RES_OK;
}

void checkLocal() {

  File root = FLASH.open("/");
  if (!root) {
    Serial.println("- failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println(" - not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
    } else {
      // addListFile(file.name(), file.size());
      String nm = String(file.name());
      if (nm.endsWith(".jsn")) {
        // load watchface elements
        nm = "/" + nm;
        registerCustomFace(nm.c_str(), &ui_img_custom_preview_png,
                           &face_custom_root, nm);
      }
    }
    file = root.openNextFile();
  }
}

String readFile(const char *path) {
  String result;
  File file = FLASH.open(path);
  if (!file || file.isDirectory()) {
    Serial.println("- failed to open file for reading");
    return result;
  }

  Serial.println("- read from file:");
  while (file.available()) {
    result += (char)file.read();
  }
  file.close();
  return result;
}

void deleteFile(const char *path) {
  Serial.printf("Deleting file: %s\r\n", path);
  if (FLASH.remove(path)) {
    Serial.println("- file deleted");
  } else {
    Serial.println("- delete failed");
  }
}

bool setupFS() {

  if (!FLASH.begin(true, "/ffat", MAX_FILE_OPEN)) {
    FLASH.format();

    return false;
  }

  static lv_fs_drv_t sd_drv;
  lv_fs_drv_init(&sd_drv);
  sd_drv.cache_size = 512;

  sd_drv.letter = 'S';
  sd_drv.open_cb = sd_open_cb;
  sd_drv.close_cb = sd_close_cb;
  sd_drv.read_cb = sd_read_cb;
  sd_drv.seek_cb = sd_seek_cb;
  sd_drv.tell_cb = sd_tell_cb;
  lv_fs_drv_register(&sd_drv);

  checkLocal();

  return true;
}

void listDir(const char *dirname, uint8_t levels) {

  lv_obj_clean(ui_fileManagerPanel);

  addListBack(driveList_cb);

  File root = FLASH.open(dirname);
  if (!root) {
    Serial.println("- failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println(" - not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      addListDir(file.name());
      // if (levels)
      // {
      //   listDir(file.path(), levels - 1);
      // }
    } else {
      addListFile(file.name(), file.size());
    }
    file = root.openNextFile();
  }
}

void flashDrive_cb(lv_event_t *e) {
  lv_disp_t *display = lv_disp_get_default();
  lv_obj_t *actScr = lv_disp_get_scr_act(display);
  if (actScr != ui_filesScreen) {
    return;
  }

  listDir("/", 0);
}

void sdDrive_cb(lv_event_t *e) {
  lv_disp_t *display = lv_disp_get_default();
  lv_obj_t *actScr = lv_disp_get_scr_act(display);
  if (actScr != ui_filesScreen) {
    return;
  }

  showError("Error", "SD card is currently unavaliable");
}

void driveList_cb(lv_event_t *e) {
  lv_obj_clean(ui_fileManagerPanel);

  addListDrive(F_NAME, FLASH.totalBytes(), FLASH.usedBytes(), flashDrive_cb);
  addListDrive("SD card", 0, 0, sdDrive_cb); // dummy SD card drive
}

bool loadCustomFace(String file) {
  String path = file;
  if (!path.startsWith("/")) {
    path = "/" + path;
  }
  String read = readFile(path.c_str());
  JsonDocument face;
  DeserializationError err = deserializeJson(face, read);
  if (!err) {
    if (!face.containsKey("elements")) {
      return false;
    }
    String name = face["name"].as<String>();
    JsonArray elements = face["elements"].as<JsonArray>();
    int sz = elements.size();

    Serial.print(sz);
    Serial.println(" elements");

    invalidate_all();
    lv_obj_clean(face_custom_root);

    for (int i = 0; i < sz; i++) {
      JsonObject element = elements[i];
      int id = element["id"].as<int>();
      int x = element["x"].as<int>();
      int y = element["y"].as<int>();
      int pvX = element["pvX"].as<int>();
      int pvY = element["pvY"].as<int>();
      String image = element["image"].as<String>();
      JsonArray group = element["group"].as<JsonArray>();

      const char *group_arr[20];
      int group_size = group.size();
      for (int j = 0; j < group_size && j < 20; j++) {
        group_arr[j] = group[j].as<const char *>();
      }

      add_item(face_custom_root, id, x, y, pvX, pvY, image.c_str(), group_arr,
               group_size);
    }

    return true;
  } else {
    Serial.println("Deserialize failed");
  }

  return false;
}

bool deleteCustomFace(String file) {
  String path = file;
  if (!path.startsWith("/")) {
    path = "/" + path;
  }
  String read = readFile(path.c_str());
  JsonDocument face;
  DeserializationError err = deserializeJson(face, read);
  if (!err) {
    if (!face.containsKey("assets")) {
      return false;
    }

    JsonArray assets = face["assets"].as<JsonArray>();
    int sz = assets.size();

    for (int j = 0; j < sz; j++) {
      deleteFile(assets[j].as<const char *>());
    }

    deleteFile(path.c_str());

    return true;
  } else {
    Serial.println("Deserialize failed");
  }

  return false;
}

void registerCustomFace(const char *name, const lv_img_dsc_t *preview,
                        lv_obj_t **watchface, String path) {
  if (numFaces >= MAX_FACES) {
    return;
  }
  faces[numFaces].name = name;
  faces[numFaces].preview = preview;
  faces[numFaces].watchface = watchface;

  faces[numFaces].customIndex = customFaceIndex;
  faces[numFaces].custom = true;

  addWatchface(faces[numFaces].name, faces[numFaces].preview, numFaces);

  customFacePaths[customFaceIndex] = path;
  customFaceIndex++;

  Timber.i("Custom Watchface: %s registered at %d", name, numFaces);
  numFaces++;
}

void onCustomDelete(lv_event_t *e) {
  int index = (int)lv_event_get_user_data(e);

  Serial.println("Delete custom watchface");
  Serial.println(customFacePaths[index]);
  showError("Delete", "The watchface will be deleted from storage, ESP32 will "
                      "restart after deletion");
  if (deleteCustomFace(customFacePaths[index])) {
    lv_scr_load_anim(ui_appListScreen, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 500, 0,
                     false);
    ESP.restart();
  } else {
    showError("Error", "Failed to delete watchface");
  }
}

void addFaceList(lv_obj_t *parent, Face face) {

  lv_obj_t *ui_faceItemPanel = lv_obj_create(parent);
  lv_obj_set_width(ui_faceItemPanel, 240);
  lv_obj_set_height(ui_faceItemPanel, 50);
  lv_obj_set_align(ui_faceItemPanel, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_faceItemPanel, LV_OBJ_FLAG_SCROLLABLE); /// Flags
  lv_obj_set_style_radius(ui_faceItemPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(ui_faceItemPanel, lv_color_hex(0xFFFFFF),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_faceItemPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_color(ui_faceItemPanel, lv_color_hex(0xFFFFFF),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_opa(ui_faceItemPanel, 255,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(ui_faceItemPanel, 1,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_side(ui_faceItemPanel, LV_BORDER_SIDE_BOTTOM,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
  // lv_obj_set_style_bg_color(ui_faceItemPanel, lv_color_hex(0xFFFFFF),
  // LV_PART_MAIN | LV_STATE_PRESSED); lv_obj_set_style_bg_opa(ui_faceItemPanel,
  // 100, LV_PART_MAIN | LV_STATE_PRESSED);

  lv_obj_t *ui_faceItemIcon = lv_img_create(ui_faceItemPanel);
  lv_img_set_src(ui_faceItemIcon, &ui_img_clock_png);
  lv_obj_set_width(ui_faceItemIcon, LV_SIZE_CONTENT);  /// 1
  lv_obj_set_height(ui_faceItemIcon, LV_SIZE_CONTENT); /// 1
  lv_obj_set_x(ui_faceItemIcon, 10);
  lv_obj_set_y(ui_faceItemIcon, 0);
  lv_obj_set_align(ui_faceItemIcon, LV_ALIGN_LEFT_MID);
  lv_obj_add_flag(ui_faceItemIcon, LV_OBJ_FLAG_ADV_HITTEST);  /// Flags
  lv_obj_clear_flag(ui_faceItemIcon, LV_OBJ_FLAG_SCROLLABLE); /// Flags

  lv_obj_t *ui_faceItemName = lv_label_create(ui_faceItemPanel);
  lv_obj_set_width(ui_faceItemName, 117);
  lv_obj_set_height(ui_faceItemName, LV_SIZE_CONTENT); /// 1
  lv_obj_set_x(ui_faceItemName, 50);
  lv_obj_set_y(ui_faceItemName, 0);
  lv_obj_set_align(ui_faceItemName, LV_ALIGN_LEFT_MID);
  lv_label_set_long_mode(ui_faceItemName, LV_LABEL_LONG_CLIP);
  if (face.custom) {
    lv_label_set_text(ui_faceItemName,
                      customFacePaths[face.customIndex].c_str());
  } else {
    lv_label_set_text(ui_faceItemName, face.name);
  }

  lv_obj_set_style_text_font(ui_faceItemName, &lv_font_montserrat_16,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_t *ui_faceItemDelete = lv_img_create(ui_faceItemPanel);
  lv_img_set_src(ui_faceItemDelete, &ui_img_bin_png);
  lv_obj_set_width(ui_faceItemDelete, LV_SIZE_CONTENT);  /// 1
  lv_obj_set_height(ui_faceItemDelete, LV_SIZE_CONTENT); /// 1
  lv_obj_set_x(ui_faceItemDelete, -10);
  lv_obj_set_y(ui_faceItemDelete, 0);
  lv_obj_set_align(ui_faceItemDelete, LV_ALIGN_RIGHT_MID);
  lv_obj_add_flag(ui_faceItemDelete,
                  LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_ADV_HITTEST); /// Flags
  lv_obj_clear_flag(ui_faceItemDelete, LV_OBJ_FLAG_SCROLLABLE);     /// Flags
  lv_obj_set_style_radius(ui_faceItemDelete, 20,
                          LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(ui_faceItemDelete, lv_color_hex(0xF34235),
                            LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(ui_faceItemDelete, 255,
                          LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_border_color(ui_faceItemDelete, lv_color_hex(0xF34235),
                                LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_border_opa(ui_faceItemDelete, 255,
                              LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_border_width(ui_faceItemDelete, 2,
                                LV_PART_MAIN | LV_STATE_PRESSED);

  if (!face.custom) {
    lv_obj_add_flag(ui_faceItemDelete, LV_OBJ_FLAG_HIDDEN);

  } else {
    lv_obj_add_event_cb(ui_faceItemDelete, onCustomDelete, LV_EVENT_CLICKED,
                        (void *)face.customIndex);
  }
}

void connectionCallback(bool state) {
  Timber.d(state ? "Connected" : "Disconnected");
  if (state) {
    lv_obj_clear_state(ui_btStateButton, LV_STATE_CHECKED);
  } else {
    lv_obj_add_state(ui_btStateButton, LV_STATE_CHECKED);
  }
  lv_label_set_text_fmt(ui_appConnectionText, "Status\n%s",
                        state ? "#19f736 Connected#" : "#f73619 Disconnected#");
}

void ringerCallback(String caller, bool state) {
  lv_disp_t *display = lv_disp_get_default();
  lv_obj_t *actScr = lv_disp_get_scr_act(display);

  if (state) {
    screenTimer.time = millis() + 50;

    lastActScr = actScr;
    Serial.print("Ringer: Incoming call from ");
    Serial.println(caller);
    lv_label_set_text(ui_callName, caller.c_str());
    lv_scr_load_anim(ui_callScreen, LV_SCR_LOAD_ANIM_FADE_IN, 500, 0, false);
  } else {
    Serial.println("Ringer dismissed");
    // load last active screen
    if (actScr == ui_callScreen && lastActScr != nullptr) {
      lv_scr_load_anim(lastActScr, LV_SCR_LOAD_ANIM_FADE_OUT, 500, 0, false);
    }
  }
  screenTimer.active = true;
}

void notificationCallback(Notification notification) {
  Timber.d("Notification Received from " + notification.app + " at " +
           notification.time);
  Timber.d(notification.message);
  notificationsUpdate = true;
  // onNotificationsOpen(click);
  showAlert();
}

void configCallback(Config config, uint32_t a, uint32_t b) {
  switch (config) {
  case CF_RST:

    Serial.println("Reset request, formating storage");
    FLASH.format();
    delay(2000);
    ESP.restart();

    break;
  case CF_WEATHER:

    if (a) {
      lv_label_set_text_fmt(ui_weatherTemp, "%d°C", watch.getWeatherAt(0).temp);
      // set icon ui_weatherIcon
      setWeatherIcon(ui_weatherIcon, watch.getWeatherAt(0).icon, isDay());
    }
    if (a == 2) {
      weatherUpdate = true;
    }

    break;
  case CF_FONT:
    screenTimer.time = millis();
    screenTimer.active = true;
    if (((b >> 16) & 0xFFFF) == 0x01) { // Style 1
      if ((b & 0xFFFF) == 0x01) {       // TOP
        lv_obj_set_style_text_color(ui_hourLabel, lv_color_hex(a),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
      }
      if ((b & 0xFFFF) == 0x02) { // CENTER
        lv_obj_set_style_text_color(ui_minuteLabel, lv_color_hex(a),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
      }
      if ((b & 0xFFFF) == 0x03) { // BOTTOM
        lv_obj_set_style_text_color(ui_dayLabel, lv_color_hex(a),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_dateLabel, lv_color_hex(a),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_weatherTemp, lv_color_hex(a),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_amPmLabel, lv_color_hex(a),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
      }
    }

    break;
  case CF_CAMERA: {
    lv_disp_t *display = lv_disp_get_default();
    lv_obj_t *actScr = lv_disp_get_scr_act(display);

    if (b) {
      screenTimer.time = millis() + 50;
      lastActScr = actScr;
      lv_scr_load_anim(ui_cameraScreen, LV_SCR_LOAD_ANIM_FADE_IN, 500, 0,
                       false);
      screenTimer.active = true;
    } else {
      if (actScr == ui_cameraScreen && lastActScr != nullptr) {
        lv_scr_load_anim(lastActScr, LV_SCR_LOAD_ANIM_FADE_OUT, 500, 0, false);
      }
      screenTimer.active = true;
    }
  } break;

  case CF_APP:
    // state is saved internally
    Serial.print("Chronos App; Code: ");
    Serial.print(a); // int code = watch.getAppCode();
    Serial.print(" Version: ");
    Serial.println(watch.getAppVersion());
    lv_label_set_text_fmt(ui_appDetailsText, "Chronos app\nv%s (%d)",
                          watch.getAppVersion().c_str(), a);
    break;
  case CF_QR:
    if (a == 1) {
      updateQrLinks();
    }
    break;
  }
}

void onBrightnessChange(lv_event_t *e) {
  // Your code here
  lv_obj_t *slider = lv_event_get_target(e);
  int v = lv_slider_get_value(slider);
  tft.setBrightness(v);

  prefs.putInt("brightness", v);
}

void onTimeoutChange(lv_event_t *e) {
  lv_obj_t *obj = lv_event_get_target(e);
  uint16_t sel = lv_dropdown_get_selected(obj);
  Timber.i("Selected index: %d", sel);

  setTimeout(sel);
  prefs.putInt("timeout", sel);
}

void onLanguageChange(lv_event_t *e) {}

void setTimeout(int i) {
  if (i == 4) {
    screenTimer.duration = -1; // always on
  } else if (i == 0) {
    screenTimer.duration = 5000; // 5 seconds
    screenTimer.active = true;
  } else if (i < 4) {
    screenTimer.duration = 10000 * i; // 10, 20, 30 seconds
    screenTimer.active = true;
  }
}

void rawDataCallback(uint8_t *data, int len) {
  if (data[0] == 0xB0) {
    // this is a chunk header data command
    cSize = data[1] * 256 + data[2]; // data chunk size
    pos = data[3] * 256 +
          data[4];       // position of the chunk, ideally sequential 0..
    last = data[7] == 1; // whether this is the last chunk (1) or not (0)
    total = (data[8] * 256 * 256 * 256) + (data[9] * 256 * 256) +
            (data[10] * 256) + data[11]; // total size of the whole file
    recv = 0;                            // counter for the chunk data

    start = pos == 0;
    if (pos == 0) {
      // this is the first chunk
      transfer = true;
      currentRecv = 0;

      fName = "/" + String(total, HEX) + "-" + String(total) + ".cbn";
    }
  }
  if (data[0] == 0xAF) {
    // this is the chunk data, line by line. The complete chunk will have
    // several of these actual data starts from index 5
    int ln = ((data[1] * 256 + data[2]) -
              5); // byte 1 and 2 make up the (total size of data - 5)

    if (wSwitch) {
      memcpy(buf1 + recv, data + 5, ln);
    } else {
      memcpy(buf2 + recv, data + 5, ln);
    }

    recv +=
        ln; // increment the received chunk data size by current received size

    currentRecv += ln; // track the progress

    if (recv == cSize) { // received expected? if data chunk size equals chunk
                         // receive size then chunk is complete
      if (wSwitch) {
        wLen1 = cSize;
      } else {
        wLen2 = cSize;
      }

      wSwitch = !wSwitch;
      writeFile = true;

      pos++;
      uint8_t lst = last ? 0x01 : 0x00;
      uint8_t cmd[5] = {0xB0, 0x02, highByte(pos), lowByte(pos), lst};
      watch.sendCommand(cmd, 5); // notify the app that we received the chunk,
                                 // this will trigger transfer of next chunk
    }

    if (last) {
    }
  }
}

void dataCallback(uint8_t *data, int length) {
  // Serial.println("Received Data");
  // for (int i = 0; i < length; i++)
  // {
  //   Serial.printf("%02X ", data[i]);
  // }
  // Serial.println();
}

void logCallback(Level level, unsigned long time, String message) {
  Serial.print(message);
}

void my_log_cb(const char *buf) { Serial.write(buf, strlen(buf)); }

static void btn_event_cb(lv_event_t *e) {
  const char *msg = (const char *)lv_event_get_user_data(e);
  Serial.println(msg);
}

void hal_setup() {

  Serial.begin(115200); /* prepare for possible serial debug */

  Timber.setLogCallback(logCallback);

  Timber.i("Starting up device");

  prefs.begin("my-app");

  tft.init();
  tft.initDMA();
  tft.startWrite();
  tft.fillScreen(TFT_BLACK);

  Serial.println(heapUsage());

  lv_init();

  lv_disp_draw_buf_init(&draw_buf, buf[0], buf[1], screenWidth * buf_size);

  /*Initialize the display*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  /*Change the following line to your display resolution*/
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  /*Initialize the (dummy) input device driver*/
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  lv_log_register_print_cb(my_log_cb);
  lv_disp_t *dispp = lv_disp_get_default();

  String about = "v4.0 [fbiego]\nESP32 C3 Mini\n" + watch.getAddress();
  // lv_label_set_text(ui_aboutText, about.c_str());

  tft.setBrightness(200);

  lv_obj_t *container = lv_obj_create(lv_scr_act());
  lv_obj_set_size(container, 300, 200);
  lv_obj_align(container, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_layout(container, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW_WRAP);

  // Create three buttons
  lv_obj_t *btn1 = lv_btn_create(container);
  lv_obj_t *btn2 = lv_btn_create(container);
  lv_obj_t *btn3 = lv_btn_create(container);

  // Set button labels
  lv_obj_t *label1 = lv_label_create(btn1);
  lv_label_set_text(label1, "Button 1");
  lv_obj_t *label2 = lv_label_create(btn2);
  lv_label_set_text(label2, "Button 2");
  lv_obj_t *label3 = lv_label_create(btn3);
  lv_label_set_text(label3, "Button 3");

  // Set button event callbacks
  lv_obj_add_event_cb(btn1, btn_event_cb, LV_EVENT_CLICKED,
                      (void *)"Button 1 pressed");
  lv_obj_add_event_cb(btn2, btn_event_cb, LV_EVENT_CLICKED,
                      (void *)"Button 2 pressed");
  lv_obj_add_event_cb(btn3, btn_event_cb, LV_EVENT_CLICKED,
                      (void *)"Button 3 pressed");

  Timber.i("Setup done");
  Timber.i(about);
}

void hal_loop() {
  if (!transfer) {
    lv_timer_handler(); /* let the GUI do its work */

    static uint32_t last_flush = 0;
    uint32_t now = millis();
    if (now - last_flush > 1000) { // Print every second
      Serial.println("LVGL timer handled");
      last_flush = now;
    }

    delay(15); // Increased delay

    lv_disp_t *display = lv_disp_get_default();
    lv_obj_t *actScr = lv_disp_get_scr_act(display);
  }
}
