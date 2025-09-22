// M5CardputerのキーボードマトリックスをスキャンするArduinoサンプルコード

#include <Arduino.h>
// 回路図に基づくピン定義
// 行（Rows）は74HC138デコーダーの制御ピン
const int rowSelectPins[3] = {8, 9, 11};  // GPIO G8(A0), G9(A1), G11(A2)

// 列（Columns）は直接接続されたGPIO
const int colPins[] = {3, 4, 5, 6, 7, 13, 15};
const int numCols = sizeof(colPins) / sizeof(colPins[0]);

void setup() {
  // デバッグ用にシリアル通信を開始
  Serial.begin(115200);
  Serial.println("Keyboard Scan Test");

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
}

// 指定された行番号（0-7）を選択してアクティブ（LOW）にする関数
void selectRow(byte row) {
  // 3つの制御ピンにrowの2進数表現を書き込む
  digitalWrite(rowSelectPins[0], (row & 0b001) ? HIGH : LOW);  // A0
  digitalWrite(rowSelectPins[1], (row & 0b010) ? HIGH : LOW);  // A1
  digitalWrite(rowSelectPins[2], (row & 0b100) ? HIGH : LOW);  // A2
}

void loop() {
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