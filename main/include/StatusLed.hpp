#pragma once

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "led_strip.h"
#include <atomic>
#include <cstdint>

/**
 * @brief Possible visual states for the RGB status LED.
 */
enum class LedState : uint8_t {
  STANDBY_BLUE,   ///< Solid blue  – AP up, waiting for configuration
  ERROR_RED,      ///< Solid red   – failed to connect to target WiFi
  ACTIVE_GREEN,   ///< Blinking green – repeater active, blink rate = f(traffic)
  RECONNECTING_YELLOW ///< Blinking yellow - watchdog attempting reconnection
};

/**
 * @brief Manages a single WS2812 addressable RGB LED via the
 * espressif/led_strip component. Runs its own FreeRTOS task for non-blocking
 * blink animations.
 *
 * The blink rate in ACTIVE_GREEN state is modulated by byte-delta values
 * received through a FreeRTOS Queue (fed by WifiManager's traffic-monitor
 * logic).
 */
class StatusLed {
public:
  /**
   * @brief Construct and start the LED task.
   * @param gpioPin          GPIO number for the WS2812 data line.
   * @param trafficQueue     Queue handle that carries uint32_t byte-delta
   * values.
   * @param taskPriority     FreeRTOS priority for the LED task (default 2).
   * @param taskStackSize    Stack depth in bytes (default 3072).
   */
  StatusLed(gpio_num_t gpioPin, QueueHandle_t trafficQueue,
            UBaseType_t taskPriority = 2, uint32_t taskStackSize = 3072);

  ~StatusLed();

  // Non-copyable
  StatusLed(const StatusLed &) = delete;
  StatusLed &operator=(const StatusLed &) = delete;

  /**
   * @brief Thread-safe: change the current visual state from any task.
   */
  void setState(LedState newState);

  /**
   * @brief Thread-safe: read the current visual state.
   */
  LedState getState() const;

private:
  static constexpr const char *TAG = "StatusLed";

  // --- Brightness cap (50 %) ---
  static constexpr uint8_t MAX_BRIGHTNESS = 50;

  // --- Blink-rate bounds (ms) in ACTIVE_GREEN state ---
  static constexpr uint32_t BLINK_FAST_MS = 100;
  static constexpr uint32_t BLINK_SLOW_MS = 1000;

  // --- Traffic thresholds for blink mapping ---
  static constexpr uint32_t TRAFFIC_LOW_BYTES = 0;
  static constexpr uint32_t TRAFFIC_HIGH_BYTES = 50000;

  // --- Hardware ---
  gpio_num_t m_gpioPin;
  led_strip_handle_t m_strip = nullptr;

  // --- FreeRTOS ---
  TaskHandle_t m_taskHandle = nullptr;
  QueueHandle_t m_trafficQueue = nullptr;
  std::atomic<LedState> m_state{LedState::STANDBY_BLUE};

  // --- Task internals ---
  static void taskWrapper(void *pvParameters);
  void run();
  void initStrip();
  void setColor(uint8_t r, uint8_t g, uint8_t b);
  void clearLed();

  /**
   * @brief Map a byte-delta value to a blink interval.
   *        Higher traffic → shorter interval (faster blink).
   */
  uint32_t deltaToBlinkMs(uint32_t delta) const;
};
