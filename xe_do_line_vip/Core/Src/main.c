#include "main.h"

/* Biến ngoại vi */
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;
TIM_HandleTypeDef htim1;
UART_HandleTypeDef huart1;

/* Biến điều khiển */
volatile uint16_t adc_values[5];
#define MAX_PWM     1000
#define BASE_SPEED  550
float Kp = 1.5f;
float Kd = 20.0f;
float error = 0.0f;
float previous_error = 0.0f;
float weights[5] = {-20.0f, -10.0f, 0.0f, 10.0f, 20.0f};

volatile uint8_t rx_data;
volatile uint8_t remote_mode = 0;

/* Khai báo hàm con */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM1_Init(void);
static void MX_USART1_UART_Init(void);

static int clamp(int x, int min, int max) {
    if (x < min) return min;
    if (x > max) return max;
    return x;
}

void set_motor_speed(int left, int right) {
    left  = clamp(left, -MAX_PWM, MAX_PWM);
    right = clamp(right, -MAX_PWM, MAX_PWM);

    if (left >= 0) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, left);
    } else {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, -left);
    }

    if (right >= 0) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, right);
    } else {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, -right);
    }
}

/* HÀM NGẮT NHẬN LỆNH TỪ ESP8266 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        if (rx_data == 'M') {
            remote_mode = !remote_mode;
            set_motor_speed(0, 0);
        }
        if (remote_mode == 1) {
            switch (rx_data) {
                case 'F': set_motor_speed(450, 450);   break;
                case 'B': set_motor_speed(-450, -450); break;
                case 'L': set_motor_speed(-500, 500);  break;
                case 'R': set_motor_speed(500, -500);  break;
                case 'S': set_motor_speed(0, 0);       break;
            }
        }
        HAL_UART_Receive_IT(&huart1, (uint8_t *)&rx_data, 1);
    }
}
/* HÀM TỰ ĐỘNG PHỤC HỒI KHI UART BỊ LỖI TREO (OVERRUN/NOISE) */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        // Xóa các cờ lỗi phần cứng
        __HAL_UART_CLEAR_OREFLAG(huart);
        __HAL_UART_CLEAR_NEFLAG(huart);
        __HAL_UART_CLEAR_FEFLAG(huart);

        // Kích hoạt lại ngắt nhận UART để không 	bị "điếc"
        HAL_UART_Receive_IT(&huart1, (uint8_t *)&rx_data, 1);
    }
}
int main(void) {
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_ADC1_Init();
    MX_TIM1_Init();
    MX_USART1_UART_Init();

    /* Bắt đầu các tiến trình */
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_values, 5);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    HAL_UART_Receive_IT(&huart1, (uint8_t *)&rx_data, 1);

    while (1) {
        if (remote_mode == 0) {
            float sum_adc = 0;
            float weighted_sum = 0;
            for (int i = 0; i < 5; i++) {
                float val = 4095.0f - (float)adc_values[i];
                if (val < 1000) val = 0;
                weighted_sum += val * weights[i];
                sum_adc += val;
            }
            if (sum_adc > 500) {
                error = weighted_sum / sum_adc;
                float control = (Kp * error) + (Kd * (error - previous_error));
                previous_error = error;
                set_motor_speed(BASE_SPEED + (int)control, BASE_SPEED - (int)control);
            } else {
                if (previous_error < 0) set_motor_speed(-500, 500);
                else set_motor_speed(500, -500);
            }
            HAL_Delay(2); // Delay nhỏ cho vòng lặp PID ổn định
        } else {
        	// --- CHẾ ĐỘ REMOTE: NHẠY TỨC THÌ ---
        	        // Không dùng Delay ở đây nữa.
        	        // Ta chỉ cho LED sáng đứng yên hoặc dùng hàm HAL_GetTick() nếu muốn nháy.
        	        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);

        	        // Chip sẽ chạy vòng lặp này cực nhanh (micro giây),
        	        // giúp nó luôn sẵn sàng nhảy vào hàm ngắt UART ngay khi có lệnh.
        	        HAL_Delay(1);
        }

    }
}

/* CÁC HÀM CẤU HÌNH HỆ THỐNG (GIỮ NGUYÊN THEO IOC) */
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 16;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}

static void MX_ADC1_Init(void) {
    ADC_ChannelConfTypeDef sConfig = {0};
    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = ENABLE;
    hadc1.Init.ContinuousConvMode = ENABLE;
    hadc1.Init.NbrOfConversion = 5;
    hadc1.Init.DMAContinuousRequests = ENABLE;
    HAL_ADC_Init(&hadc1);
    sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;
    sConfig.Channel = ADC_CHANNEL_0; sConfig.Rank = 1; HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    sConfig.Channel = ADC_CHANNEL_1; sConfig.Rank = 2; HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    sConfig.Channel = ADC_CHANNEL_4; sConfig.Rank = 3; HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    sConfig.Channel = ADC_CHANNEL_8; sConfig.Rank = 4; HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    sConfig.Channel = ADC_CHANNEL_11; sConfig.Rank = 5; HAL_ADC_ConfigChannel(&hadc1, &sConfig);
}

static void MX_TIM1_Init(void) {
    TIM_OC_InitTypeDef sConfigOC = {0};
    htim1.Instance = TIM1;
    htim1.Init.Prescaler = 84-1;
    htim1.Init.Period = 1000-1;
    HAL_TIM_Base_Init(&htim1);
    HAL_TIM_PWM_Init(&htim1);
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1);
    HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2);
    HAL_TIM_MspPostInit(&htim1);
}

static void MX_USART1_UART_Init(void) {
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    HAL_UART_Init(&huart1);
}

static void MX_DMA_Init(void) {
    __HAL_RCC_DMA2_CLK_ENABLE();
    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
}

static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void Error_Handler(void) {
    __disable_irq();
    while (1);
}
