// M5CardputerのキーボードマトリックスをスキャンするArduinoサンプルコード

#include <Arduino.h>
#include <Wire.h>
/**
 * 旧CardPuter (V1.1)
 */
// 回路図に基づくピン定義
// 行（Rows）は74HC138デコーダーの制御ピン
const int rowSelectPins[3] = {8, 9, 11};  // GPIO G8(A0), G9(A1), G11(A2)

// 列（Columns）は直接接続されたGPIO
const int colPins[] = {3, 4, 5, 6, 7, 13, 15};
const int numCols = sizeof(colPins) / sizeof(colPins[0]);

/**
 * Cardputer ADV (TCA8418RTWR使用)
 */
// Cardputer ADVはTCA8418RTWRがI2Cで接続されている
const int TCA8418_ADDRESS = 0x34;  // I2Cアドレス
const uint8_t I2C_SDA = G8;
const uint8_t I2C_SCL = G9;
const uint8_t TCA8418_INT = G11;
// TCA8418RTWRのレジスタアドレス
const uint8_t REG_CFG = 0x01;
const uint8_t REG_KEY_LCK_EC = 0x02;
const uint8_t REG_KEY_EVENT_A = 0x03;
const uint8_t REG_INT_STAT = 0x0B;
const uint8_t REG_KP_GPIO1 = 0x1D;
const uint8_t REG_KP_GPIO2 = 0x1E;
const uint8_t REG_KP_GPIO3 = 0x1F;

/**
 * デバイス検出状態の保存など
 */
bool DetectedADV = false;

/**
 * @brief I2C送信を行う関数
 * @param address I2Cアドレス
 * @param reg レジスタアドレス
 * @param data 送信するデータ
 * @param sendStop 送信後にSTOP信号を送るかどうか (デフォルト: true)
 * @return 送信結果 (0: 成功, 1-4: エラー)
 */
uint8_t I2CSend(uint8_t address, uint8_t reg, uint8_t data,
                bool sendStop = true) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(data);
  return Wire.endTransmission(sendStop);
}

/**
 * @brief I2Cレジスタから1バイト読み取る関数
 * @param address I2Cアドレス
 * @param reg レジスタアドレス
 * @param data 読み取ったデータを格納する変数のポインタ
 * @return 読み取り成功時はtrue、失敗時はfalse
 */
bool I2CRead(uint8_t address, uint8_t reg, uint8_t* data) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;

  Wire.requestFrom(address, 1);
  if (Wire.available()) {
    *data = Wire.read();
    return true;
  }
  return false;
}

/**
 * TCA8418_ADDRESSをスキャンして存在を確認する関数
 */
bool Detect_TCA8418() {
  Wire.beginTransmission(TCA8418_ADDRESS);
  return (Wire.endTransmission() == 0);
}

bool initI2C() {
  Wire.begin(I2C_SDA, I2C_SCL, 400000);
  delay(100);  // I2Cの初期化待ち

  if (Detect_TCA8418()) {
    Serial.println("TCA8418 Detected");
    return true;
  } else {
    Serial.println("TCA8418 Not Detected");
    return false;
  }
}

/**
 * @brief データシートに従ってTCA8418を確実に初期化する
 * @return 成功時true、失敗時false
 */
bool SetupAdvKeyboard() {
  // --- 重要ステップ: キーパッドマトリックスモード用のピン設定 ---
  // 事前にTCA8418の存在を確認してください
  // M5Cardputer ADV回路図とデータシート（レジスタ0x1D-0x1F）によると、
  // キーパッド機能のためにROW0-7とCOL0-6を有効にする必要があります。
  // これらのビットに'1'を設定すると、ピンがキースキャンモードになります。

  // 1. キーパッドマトリックス用のROW0-7を有効化 (KP_GPIO1)
  I2CSend(TCA8418_ADDRESS, REG_KP_GPIO1, 0xFF);  // 全8行を有効化 (ROW0-7)
  delay(5);

  // 2. キーパッドマトリックス用のCOL0-6を有効化 (KP_GPIO2)
  I2CSend(TCA8418_ADDRESS, REG_KP_GPIO2, 0x7F);  // 最初の7列を有効化 (COL0-6)
  delay(5);

  // 3. 未使用のキーパッドピン (COL8, COL9) を無効化 (KP_GPIO3)
  I2CSend(TCA8418_ADDRESS, REG_KP_GPIO3, 0x00);  // COL8とCOL9を無効化
  delay(5);

  // 4. 割り込みとキーパッドスキャン動作の設定
  // KE_IEN = 1: キーイベントのFIFOへの報告を有効化
  I2CSend(TCA8418_ADDRESS, REG_CFG,
          0x01);  // キーイベント割り込みを有効化（FIFOレポート機能）
  delay(5);

  // 5.
  // クリーンなスタートを確保するため、FIFOバッファ内の古いイベントをフラッシュ
  Serial.println("FIFOバッファをフラッシュしています...");
  while (true) {
    uint8_t status;
    if (!I2CRead(TCA8418_ADDRESS, REG_KEY_LCK_EC, &status)) {
      break;  // I2Cエラー時は中断
    }

    if ((status & 0x01) == 0) break;  // FIFOが空の場合は中断

    // 古いイベントを破棄するためにイベントレジスタから読み取り
    uint8_t dummy_data;
    I2CRead(TCA8418_ADDRESS, REG_KEY_EVENT_A, &dummy_data);
  }

  Serial.println("Initialization complete.");
  return true;
}

/**
 * @brief キーボードの状態をポーリングし、イベントがあればシリアルに出力する
 */
void PrintAdvKeyStatus() {
  uint8_t status;
  if (I2CRead(TCA8418_ADDRESS, REG_KEY_LCK_EC, &status)) {
    // KEビットをチェックしてFIFOにイベントがあるかを確認
    if (status & 0x01) {
      uint8_t key_data;
      if (I2CRead(TCA8418_ADDRESS, REG_KEY_EVENT_A, &key_data)) {
        uint8_t key_id = key_data & 0x7F;   // ビット0-6がキーID
        bool is_pressed = key_data & 0x80;  // ビット7が押下/リリース状態

        if (is_pressed) {
          Serial.printf("キーID: %u 押下\n", key_id);
        } else {
          Serial.printf("キーID: %u リリース\n", key_id);
        }
      }
    }
  }
}
/**
 * @brief 旧CardPuterのセットアップ関数
 */
void SetupOldKeyboard() {
  // 行の選択ピンをOUTPUTに設定
  for (int i = 0; i < 3; i++) {
    pinMode(rowSelectPins[i], OUTPUT);
  }

  // 列のピンをINPUT_PULLUPに設定
  // これにより、キーが押されていないときはHIGHになり、
  // 押されたときにアクティブな行（LOW）と接触してLOWになります。
  for (int i = 0; i < numCols; i++) {
    pinMode(colPins[i], INPUT_PULLUP);
  }
  Serial.println("Detected Cardputer (V1.1)");
}

/**
 * @brief 旧CardPuterのキーボードマトリックスをスキャンする関数
 *        押されたキーの行と列をシリアルに出力します。
 */
// 指定された行番号（0-7）を選択してアクティブ（LOW）にする関数
void selectRow(byte row) {
  // 3つの制御ピンにrowの2進数表現を書き込む
  digitalWrite(rowSelectPins[0], (row & 0b001) ? HIGH : LOW);  // A0
  digitalWrite(rowSelectPins[1], (row & 0b010) ? HIGH : LOW);  // A1
  digitalWrite(rowSelectPins[2], (row & 0b100) ? HIGH : LOW);  // A2
}

void PrintOldKeyStatus() {
  // 8つの行を順番にスキャン
  for (int r = 0; r < 8; r++) {
    // 現在の行をアクティブにする
    selectRow(r);

    // 少し待って信号を安定させる
    delayMicroseconds(50);

    // 7つの列をチェック
    for (int c = 0; c < numCols; c++) {
      // 列ピンがLOWなら、その位置のキーが押されている
      if (digitalRead(colPins[c]) == LOW) {
        Serial.print("Key Pressed: Row=");
        Serial.print(r);
        Serial.print(", Col=");
        Serial.println(c);

        // チャタリング防止と連続出力の抑制
        delay(200);
      }
    }
  }
}

/**
 * @brief 失敗時に無限ループで停止する関数
 */
void fail() {
  while (1) {
    delay(1000);
  }
}

void setup() {
  // デバッグ用にシリアル通信を開始
  Serial.begin(115200);
  delay(300);  // シリアルモニタの準備時間
  Serial.println("Keyboard Scan Test");
  if (initI2C()) {
    if (!SetupAdvKeyboard()) {
      Serial.println("Failed to initialize keyboard.");
      fail();
    }
    DetectedADV = true;
  } else {
    SetupOldKeyboard();
  }
}

void loop() {
  if (DetectedADV) {
    PrintAdvKeyStatus();
    delay(50);  // ポーリング間隔
  } else {
    PrintOldKeyStatus();
  }
}