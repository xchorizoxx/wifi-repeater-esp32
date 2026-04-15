#include "StatusLed.hpp"
#include <algorithm>

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
StatusLed::StatusLed(gpio_num_t gpioPin, QueueHandle_t trafficQueue,
                     UBaseType_t taskPriority, uint32_t taskStackSize)
    : m_gpioPin(gpioPin), m_trafficQueue(trafficQueue) {
  initStrip();

  BaseType_t ret = xTaskCreate(taskWrapper, "led_task", taskStackSize, this,
                               taskPriority, &m_taskHandle);

  if (ret != pdPASS) {
    ESP_LOGE(TAG, "Failed to create LED task");
  }
}

StatusLed::~StatusLed() {
  if (m_taskHandle) {
    vTaskDelete(m_taskHandle);
    m_taskHandle = nullptr;
  }
  if (m_strip) {
    led_strip_clear(m_strip);
    led_strip_del(m_strip);
    m_strip = nullptr;
  }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void StatusLed::setState(LedState newState) {
  m_state.store(newState, std::memory_order_relaxed);
}

LedState StatusLed::getState() const {
  return m_state.load(std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// Hardware init
// ---------------------------------------------------------------------------
void StatusLed::initStrip() {
  led_strip_config_t strip_config = {};
  strip_config.strip_gpio_num = m_gpioPin;
  strip_config.max_leds = 1;
  strip_config.led_model = LED_MODEL_WS2812;
  strip_config.flags.invert_out = false;

  led_strip_rmt_config_t rmt_config = {};
  rmt_config.clk_src = RMT_CLK_SRC_DEFAULT;
  rmt_config.resolution_hz = 10 * 1000 * 1000; // 10 MHz
  rmt_config.flags.with_dma = false;

  esp_err_t err =
      led_strip_new_rmt_device(&strip_config, &rmt_config, &m_strip);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create LED strip: %s", esp_err_to_name(err));
    m_strip = nullptr;
    return;
  }

  clearLed();
  ESP_LOGI(TAG, "WS2812 initialised on GPIO %d", static_cast<int>(m_gpioPin));
}

// ---------------------------------------------------------------------------
// Pixel helpers
// ---------------------------------------------------------------------------
void StatusLed::setColor(uint8_t r, uint8_t g, uint8_t b) {
  if (!m_strip)
    return;

  // Cap each channel to MAX_BRIGHTNESS (50 %)
  r = std::min(r, MAX_BRIGHTNESS);
  g = std::min(g, MAX_BRIGHTNESS);
  b = std::min(b, MAX_BRIGHTNESS);

  led_strip_set_pixel(m_strip, 0, r, g, b);
  led_strip_refresh(m_strip);
}

void StatusLed::clearLed() {
  if (!m_strip)
    return;
  led_strip_clear(m_strip);
}

// ---------------------------------------------------------------------------
// Blink-rate mapping
// ---------------------------------------------------------------------------
uint32_t StatusLed::deltaToBlinkMs(uint32_t delta) const {
  if (delta >= TRAFFIC_HIGH_BYTES)
    return BLINK_FAST_MS;
  if (delta <= TRAFFIC_LOW_BYTES)
    return BLINK_SLOW_MS;

  // Linear interpolation: high traffic → short interval
  uint32_t range = TRAFFIC_HIGH_BYTES - TRAFFIC_LOW_BYTES;
  uint32_t blinkRange = BLINK_SLOW_MS - BLINK_FAST_MS;
  uint32_t interval = BLINK_SLOW_MS - ((delta * blinkRange) / range);
  return interval;
}

// ---------------------------------------------------------------------------
// FreeRTOS task
// ---------------------------------------------------------------------------
void StatusLed::taskWrapper(void *pvParameters) {
  static_cast<StatusLed *>(pvParameters)->run();
}

void StatusLed::run() {
  uint32_t currentBlinkIntervalMs = BLINK_SLOW_MS;
  bool ledOn = false;
  TickType_t lastToggleTime = xTaskGetTickCount();

  for (;;) {
    LedState currentState = m_state.load(std::memory_order_relaxed);

    // 1. Wait efficiently on the queue, max 100ms timeout
    // This allows us to react to new traffic OR state changes quickly
    uint32_t delta = 0;
    if (xQueueReceive(m_trafficQueue, &delta, pdMS_TO_TICKS(100)) == pdTRUE) {
      if (currentState == LedState::ACTIVE_GREEN) {
        currentBlinkIntervalMs = deltaToBlinkMs(delta);
      }
    }

    // 2. Handle visual output based on current state
    switch (currentState) {
    case LedState::STANDBY_BLUE:
      setColor(0, 0, MAX_BRIGHTNESS);
      ledOn = true;
      break;

    case LedState::ERROR_RED:
      setColor(MAX_BRIGHTNESS, 0, 0);
      ledOn = true;
      break;

    case LedState::RECONNECTING_YELLOW: {
      // Blink yellow at a fixed 500ms interval
      TickType_t now = xTaskGetTickCount();
      if ((now - lastToggleTime) >= pdMS_TO_TICKS(500)) {
        if (ledOn) {
          clearLed();
        } else {
          // Yellow = Red + Green
          setColor(MAX_BRIGHTNESS, MAX_BRIGHTNESS, 0); 
        }
        ledOn = !ledOn;
        lastToggleTime = now;
      }
      break;
    }

    case LedState::ACTIVE_GREEN: {
      TickType_t now = xTaskGetTickCount();
      // Only toggle if enough time has elapsed according to the traffic-based interval
      if ((now - lastToggleTime) >= pdMS_TO_TICKS(currentBlinkIntervalMs)) {
        if (ledOn) {
          clearLed();
        } else {
          setColor(0, MAX_BRIGHTNESS, 0);
        }
        ledOn = !ledOn;
        lastToggleTime = now;
      }
      break;
    }
    }
  }
}
