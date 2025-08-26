/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "st7735.h"
#include "stdio.h"
#include "GFX_FUNCTIONS.h"
#include "fonts.h"
#include "hcm.h"
#include "jp.h"
#include "uk.h"
#include "us.h"

#define BTN_SET_HOUR GPIO_PIN_6  // Nút tăng giờ
#define BTN_SET_MIN  GPIO_PIN_1  // Nút tăng phút
#define BTN_CONFIRM  GPIO_PIN_2  // Nút xác nhận hẹn giờ
#define BTN_MODE     GPIO_PIN_3  // Nút chuyển chế độ
#define BTN_PORT 	 GPIOA  // Thay cho #define GPIOx GPIOA
#define SQW_INT_PIN  GPIO_PIN_4  // Chân PA4 nhận ngắt từ DS3231
#define BUZZER_PIN GPIO_PIN_8
#define BUZZER_PORT GPIOA
#define NUM_TZ 4

static uint32_t alarm_display_start = 0;
uint8_t alarm_hour = 0, alarm_minute = 0;
//uint8_t set_hour = 0, set_minute = 0;

uint8_t alarm_triggered = 0;
uint8_t confirm_pressed = 0;
uint32_t last_press_time_hour = 0;
uint32_t last_press_time_minute = 0;
uint8_t edit_field = 0;  // 0: chỉnh giờ, 1: chỉnh phút
uint32_t last_display_update = 0;

uint32_t last_blink = 0;
uint8_t blink_state = 1;  // 1: hiển thị, 0: ẩn
uint8_t set_hour = 0;
uint8_t set_min = 0;
uint8_t set_sec = 0;
uint8_t set_day = 1;
uint8_t set_month = 1;
uint8_t set_year = 24;
//uint16_t sun_x = 120, sun_y = 20; // Vị trí mặt trời
//uint16_t cloud_x = 100, cloud_y = 10; // Vị trí đám mây
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#define DS3231_ADDRESS 0xD0

// convert normal decimal numbers to binary coded decimal
uint8_t Dec2Bcd(int val)
{
	return (uint8_t)(( val/10*16 ) + ( val%10 ));
}
// convert binary coded decimal to normal decimal numbers
int Bcd2Dec(uint8_t val)
{
	return (int)(( val/16*10 ) + ( val%16 ));
}

typedef struct
{
	uint8_t 	Seconds;
	uint8_t  	Minutes;
	uint8_t		Hour;
	uint8_t		Dayofweek;
	uint8_t		Dayofmonth;
	uint8_t		Month;
	uint16_t	Year;
}RTCDateTime;

RTCDateTime time;

typedef enum {
    MODE_NORMAL = 0,
    MODE_SET_ALARM,
    MODE_SET_TIME,
	MODE_SHOW_TZ
} Mode;

Mode current_mode = MODE_NORMAL;
//function to set time
void Buzzer_On() {
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET); // GPIO = LOW → còi kêu
}

void Buzzer_Off() {
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);   // GPIO = HIGH → còi tắt
}
const char *tz_names[NUM_TZ] = { "VN (UTC+7)", "JP (UTC+9)", "UK (UTC+0)", "US (UTC-5)" };
const int tz_offsets[NUM_TZ] = { 0, +2, -7, -12 };
static uint8_t tz_index = 0;

//------------------Settime-------------//

void Set_time(uint8_t sec, uint8_t min, uint8_t hour, uint8_t dow, uint8_t dom, uint8_t month, uint16_t year)
{
	uint8_t set_time[7];
	set_time[0]= Dec2Bcd(sec);
	set_time[1]= Dec2Bcd(min);
	set_time[2]= Dec2Bcd(hour);
	set_time[3]= Dec2Bcd(dow);
	set_time[4]= Dec2Bcd(dom);
	set_time[5]= Dec2Bcd(month);
	set_time[6]= Dec2Bcd(year);

	HAL_I2C_Mem_Write(&hi2c1, DS3231_ADDRESS, 0x00, 1, set_time, 7, 1000);
}

//-------------Gettime-----------//

void Get_time(void)
{
	uint8_t get_time[7];
	HAL_I2C_Mem_Read(&hi2c1, DS3231_ADDRESS, 0x00, 1, get_time, 7, 1000);

	time.Seconds = Bcd2Dec(get_time[0]);
	time.Minutes = Bcd2Dec(get_time[1]);
	time.Hour = Bcd2Dec(get_time[2]);
	time.Dayofweek = Bcd2Dec(get_time[3]);
	time.Dayofmonth = Bcd2Dec(get_time[4]);
	time.Month = Bcd2Dec(get_time[5]);
	time.Year = Bcd2Dec(get_time[6]);
}

void adjust_date() {
    // Kiểm tra tháng 2 (xét cả năm nhuận)
    if (set_month == 2) {
        if ((set_year % 4 == 0 && set_year % 100 != 0) || (set_year % 400 == 0)) {
            set_day = (set_day > 29) ? 29 : set_day;  // Năm nhuận: tối đa 29 ngày
        } else {
            set_day = (set_day > 28) ? 28 : set_day;  // Không nhuận: tối đa 28 ngày
        }
    }
    // Các tháng 4, 6, 9, 11 có tối đa 30 ngày
    else if (set_month == 4 || set_month == 6 || set_month == 9 || set_month == 11) {
        set_day = (set_day > 30) ? 30 : set_day;
    }
    // Các tháng khác (1, 3, 5, 7, 8, 10, 12) giữ nguyên (tối đa 31 ngày)
}

float Get_temp(void)
{
	uint8_t temp[2];
	HAL_I2C_Mem_Read(&hi2c1, DS3231_ADDRESS, 0x11, 1, temp, 2, 1000);
	return ((temp[0])+(temp[1]>>6)/4.0);
}
void force_temp_conv (void)
{
	uint8_t status=0;
	uint8_t control=0;
	HAL_I2C_Mem_Read(&hi2c1, DS3231_ADDRESS, 0x0F, 1, &status, 1, 100); //doc trang thai
	if (!(status&0x04))
		{
			HAL_I2C_Mem_Read(&hi2c1, DS3231_ADDRESS, 0x0E, 1, &control, 1, 100);
			HAL_I2C_Mem_Write(&hi2c1, DS3231_ADDRESS, 0x0F, 1, (uint8_t *)(control|(0x20)), 1, 100);
		}
}
float TEMP;


void Set_Alarm(uint8_t hour, uint8_t minute, uint8_t day) {
    uint8_t alarm_data[3];

    alarm_data[0] = Dec2Bcd(minute) & 0x7F;  // A2M2 = 0 (match phút)
    alarm_data[1] = Dec2Bcd(hour) & 0x7F;    // A2M3 = 0 (match giờ)
    alarm_data[2] = Dec2Bcd(day) & 0x7F;     // A2M4 = 0, DY/DT = 0 => match theo ngày

    HAL_I2C_Mem_Write(&hi2c1, DS3231_ADDRESS, 0x0B, 1, alarm_data, 3, 100);

    uint8_t control;
    HAL_I2C_Mem_Read(&hi2c1, DS3231_ADDRESS, 0x0E, 1, &control, 1, 100);
    control |= 0x06;  // Enable Alarm 2 and INTCN
    HAL_I2C_Mem_Write(&hi2c1, DS3231_ADDRESS, 0x0E, 1, &control, 1, 100);
}

void Display_Time() {
    Get_time();
    char buffer[30];

    float temp = Get_temp();
    sprintf(buffer, "Nhiet do: %d C", (int)(temp + 0.5));
    ST7735_WriteString(10, 10, buffer, Font_7x10, ST7735_YELLOW, ST7735_BLACK);
    drawCircle(97, 12, 2, ST7735_YELLOW); // x+14, y; bán kính 1 pixe
    sprintf(buffer, "%02d/%02d/20%02d", time.Dayofmonth, time.Month, time.Year);
    ST7735_WriteString(10, 30, buffer, Font_11x18, ST7735_CYAN, ST7735_BLACK);
    sprintf(buffer, "%02d:%02d", time.Hour, time.Minutes);
    ST7735_WriteString(10, 60, buffer, Font_16x26, ST7735_WHITE, ST7735_BLACK);
    sprintf(buffer, ":%02d", time.Seconds);
    ST7735_WriteString(16*5+10, 66, buffer, Font_11x18, ST7735_WHITE, ST7735_BLACK);
    if (alarm_triggered) {
        ST7735_WriteString(35, 100, "ALARM!", Font_11x18, ST7735_RED, ST7735_BLACK);
        Buzzer_On();   // Bật còi
        } else {
            Buzzer_Off();  // Tắt còi nếu không báo
    }
}

void Display_Alarm_Setting() {
    //char buffer[20];
    //sprintf(buffer, "Alarm: %02d:%02d", alarm_hour, alarm_minute);
    //ST7735_WriteString(10, 40, buffer, Font_7x10, ST7735_YELLOW, ST7735_BLACK);
	ST7735_FillRectangle(10, 55, 50, 10, ST7735_BLACK);
	static uint8_t blink = 1;  // Biến trạng thái nhấp nháy

    char buffer[16];

    if (edit_field == 0) {  // Đang chỉnh giờ
            if (blink)
                sprintf(buffer, "--:%02d", alarm_minute);  // Ẩn giờ
            else
                sprintf(buffer, "%02d:%02d", alarm_hour, alarm_minute);
        } else {  // Đang chỉnh phút
            if (blink)
                sprintf(buffer, "%02d:--", alarm_hour);  // Ẩn phút
            else
                sprintf(buffer, "%02d:%02d", alarm_hour, alarm_minute);
        }

        blink = !blink;  // Đảo trạng thái nhấp nháy
        ST7735_WriteString(30, 60, buffer, Font_11x18, ST7735_YELLOW, ST7735_BLACK);
}

void Display_Time_Setting() {
    static uint8_t blink = 1;
    char time_buffer[20];
    char date_buffer[20];

    if (edit_field < 3) { // Nếu đang chỉnh giờ/phút/giây
            if (edit_field == 0) { // Giờ
                if (blink) sprintf(time_buffer, "--:%02d:%02d", set_min, set_sec);
                else sprintf(time_buffer, "%02d:%02d:%02d", set_hour, set_min, set_sec);
            }
            else if (edit_field == 1) { // Phút
                if (blink) sprintf(time_buffer, "%02d:--:%02d", set_hour, set_sec);
                else sprintf(time_buffer, "%02d:%02d:%02d", set_hour, set_min, set_sec);
            }
            else if (edit_field == 2) { // Giây
                if (blink) sprintf(time_buffer, "%02d:%02d:--", set_hour, set_min);
                else sprintf(time_buffer, "%02d:%02d:%02d", set_hour, set_min, set_sec);
            }
        } else {
            // Hiển thị bình thường nếu không đang chỉnh giờ/phút/giây
            sprintf(time_buffer, "%02d:%02d:%02d", set_hour, set_min, set_sec);
        }

        // Hiển thị phần ngày (DD/MM/YYYY)
        if (edit_field >= 3) { // Nếu đang chỉnh ngày/tháng/năm
            if (edit_field == 3) { // Ngày
                if (blink) sprintf(date_buffer, "--/%02d/20%02d", set_month, set_year);
                else sprintf(date_buffer, "%02d/%02d/20%02d", set_day, set_month, set_year);
            }
            else if (edit_field == 4) { // Tháng
                if (blink) sprintf(date_buffer, "%02d/--/20%02d", set_day, set_year);
                else sprintf(date_buffer, "%02d/%02d/20%02d", set_day, set_month, set_year);
            }
            else if (edit_field == 5) { // Năm
                if (blink) sprintf(date_buffer, "%02d/%02d/20--", set_day, set_month);
                else sprintf(date_buffer, "%02d/%02d/20%02d", set_day, set_month, set_year);
            }
        } else {
            // Hiển thị bình thường nếu không đang chỉnh ngày/tháng/năm
            sprintf(date_buffer, "%02d/%02d/20%02d", set_day, set_month, set_year);
        }

        // Hiển thị giờ (màu khác biệt nếu đang chỉnh)
        ST7735_WriteString(20, 30, time_buffer, Font_11x18,
                          (edit_field < 3) ? ST7735_GREEN : ST7735_WHITE, ST7735_BLACK);

        // Hiển thị ngày (màu khác biệt nếu đang chỉnh)
        ST7735_WriteString(10, 60, date_buffer, Font_11x18,
                          (edit_field >= 3) ? ST7735_GREEN : ST7735_WHITE, ST7735_BLACK);

        blink = !blink;
}
//------------------------------------------//
void Display_Timezone(void) {
    Get_time();
    int tz_h = time.Hour + tz_offsets[tz_index];
    if (tz_h < 0) tz_h += 24;
    if (tz_h >= 24) tz_h -= 24;

    char buf[30];
    switch (tz_index) {
    		case 0:
    			ST7735_DrawImage(36, 70, 56, 40, (uint16_t*)gImage_hcm);
    			break;
            case 1:
                ST7735_DrawImage(44, 57, 40, 52, (uint16_t*)gImage_jp);
                break;
            case 2:
                ST7735_DrawImage(44, 57, 40, 64, (uint16_t*)gImage_uk);
                break;
            case 3:
                ST7735_DrawImage(39, 57, 50, 50, (uint16_t*)gImage_us);
                break;
            default:
                break;
        }
    ST7735_WriteString(10, 10, tz_names[tz_index], Font_11x18, ST7735_CYAN, ST7735_BLACK);

    sprintf(buf, "%02d:%02d:%02d", tz_h, time.Minutes, time.Seconds);
    ST7735_WriteString(19, 30, buf, Font_11x18, ST7735_WHITE, ST7735_BLACK);
}

void Clear_DS3231_Alarm() {
    uint8_t status;
    HAL_I2C_Mem_Read(&hi2c1, DS3231_ADDRESS, 0x0F, 1, &status, 1, 100);
    status &= ~0x02;  // Clear A2F (Alarm 2 Flag)
    HAL_I2C_Mem_Write(&hi2c1, DS3231_ADDRESS, 0x0F, 1, &status, 1, 100);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    static uint32_t last_interrupt_time = 0;
    uint32_t current_time = HAL_GetTick();

    // Debounce - chống nhiễu (50ms)
    if (current_time - last_interrupt_time < 50) return;
    last_interrupt_time = current_time;

    // Xử lý ngắt từ DS3231 (PA4)
    if (GPIO_Pin == GPIO_PIN_4) {
        uint8_t status;
        HAL_I2C_Mem_Read(&hi2c1, DS3231_ADDRESS, 0x0F, 1, &status, 1, 100);

        if (status & 0x02) {  // Ki2F)
        	alarm_triggered = 1;  // Bật cờ báo thức
        	alarm_display_start = HAL_GetTick();  // Ghi lại thời điểm bắt đầu báo thức
        	Clear_DS3231_Alarm();  // Xoá cờ A2F để báo thức không lặp
        }
    }
}

void loop_poll_buttons() {
    static uint8_t last_btn0 = 1, last_btn1 = 1, last_btn2 = 1, last_btn3 = 1;
    static uint32_t btn1_press_time = 0;
    static uint8_t btn1_is_holding = 0;
    static uint32_t last_increase_time = 0;

    uint8_t btn0 = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6);  // MOVE
    uint8_t btn1 = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1);  // INCREASE
    uint8_t btn2 = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2);  // CONFIRM
    uint8_t btn3 = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3);  // MODE
    // chan chinh hen gio phut or chinh gio phut giay  PA6
    if (btn0 == GPIO_PIN_RESET && last_btn0 == GPIO_PIN_SET) {
        HAL_Delay(20);
        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6) == GPIO_PIN_RESET) {
            if (current_mode == MODE_SET_ALARM || current_mode == MODE_SET_TIME) {
                edit_field = (edit_field + 1) % (current_mode == MODE_SET_ALARM ? 2 : 6);  // 6 trường thời gian
                if (current_mode == MODE_SET_TIME) Display_Time_Setting();
                else Display_Alarm_Setting();
            }
            else if (current_mode == MODE_SHOW_TZ) {
                        // Ở chế độ hiển thị múi giờ, mỗi nhấn MOVE sẽ chuyển sang múi giờ tiếp theo
                        tz_index = (tz_index + 1) % NUM_TZ;
                        ST7735_FillRectangle(30, 56, 70, 70, ST7735_BLACK);
                        Display_Timezone();
                    }
        }
    }
    //chan tang giờ PA1
    if (btn1 == GPIO_PIN_RESET && last_btn1 == GPIO_PIN_SET) {
        HAL_Delay(20);
        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_RESET) {
            if (current_mode == MODE_SET_ALARM) {
                if (edit_field == 0) alarm_hour = (alarm_hour + 1) % 24;
                else alarm_minute = (alarm_minute + 1) % 60;
                Display_Alarm_Setting();
            } else if (current_mode == MODE_SET_TIME) {
                switch (edit_field) {
                    case 0: set_hour = (set_hour + 1) % 24; break;
                    case 1: set_min = (set_min + 1) % 60; break;
                    case 2: set_sec = (set_sec + 1) % 60; break;
                    case 3: set_day = (set_day % 31) + 1;adjust_date(); break;
                    case 4: set_month = (set_month % 12) + 1;adjust_date(); break;
                    case 5: set_year = (set_year + 1) % 100;adjust_date(); break;
                }
                Display_Time_Setting();
            }
        }
        btn1_press_time = HAL_GetTick(); // Bắt đầu đếm thời gian giữ
        btn1_is_holding = 1;
    }
    if (btn1_is_holding && btn1 == GPIO_PIN_RESET) {
           if (HAL_GetTick() - btn1_press_time > 500) { // Giữ quá 500ms
               if (HAL_GetTick() - last_increase_time > 150) { // Tự động tăng mỗi 150ms
                   if (current_mode == MODE_SET_ALARM) {
                       if (edit_field == 0) alarm_hour = (alarm_hour + 1) % 24;
                       else alarm_minute = (alarm_minute + 1) % 60;
                       Display_Alarm_Setting();
                   } else if (current_mode == MODE_SET_TIME) {
                       switch (edit_field) {
                           case 0: set_hour = (set_hour + 1) % 24; break;
                           case 1: set_min = (set_min + 1) % 60; break;
                           case 2: set_sec = (set_sec + 1) % 60; break;
                           case 3: set_day = (set_day % 31) + 1; adjust_date(); break;
                           case 4: set_month = (set_month % 12) + 1; adjust_date(); break;
                           case 5: set_year = (set_year + 1) % 100; adjust_date(); break;
                       }
                       Display_Time_Setting();
                   }
                   last_increase_time = HAL_GetTick();
               }
           }
    }

    // Khi thả nút INCREASE ra
    if (btn1 == GPIO_PIN_SET && last_btn1 == GPIO_PIN_RESET) {
        btn1_is_holding = 0;
    }

    //chân tắt báo thức PA2
    if (btn2 == GPIO_PIN_RESET && last_btn2 == GPIO_PIN_SET) {
        HAL_Delay(20);
        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2) == GPIO_PIN_RESET) {
            if (current_mode == MODE_SET_ALARM) {
                Set_Alarm(alarm_hour, alarm_minute, time.Dayofmonth);
                current_mode = MODE_NORMAL;
                ST7735_FillScreen(ST7735_BLACK);
                Display_Time();
            } else if (current_mode == MODE_SET_TIME) {
                Set_time(set_sec, set_min, set_hour, 1, set_day, set_month, set_year);
                current_mode = MODE_NORMAL;
                ST7735_FillScreen(ST7735_BLACK);
                Display_Time();
            } else {
                // Đang ở MODE_NORMAL, xử lý tắt báo thức
                alarm_triggered = 0;
                Clear_DS3231_Alarm();
                Buzzer_Off();
                ST7735_FillRectangle(35, 100, 66, 18, ST7735_BLACK);
                Display_Time();
            }
        }
    }
    // chân mode PA3
    if (btn3 == GPIO_PIN_RESET && last_btn3 == GPIO_PIN_SET) {
        HAL_Delay(20);
        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == GPIO_PIN_RESET) {
                current_mode = (current_mode + 1) % 4;  // MODE_NORMAL → SET_ALARM → SET_TIME → NORMAL...
                edit_field = 0;
                if (current_mode == MODE_NORMAL) {
                    Display_Time();
                } else if (current_mode == MODE_SET_ALARM) {
                    Display_Alarm_Setting();
                } else if (current_mode == MODE_SET_TIME) {
                	Get_time();  //Lấy giờ mới nhất từ DS3231 vào biến time

                	set_hour = time.Hour;
                	set_min = time.Minutes;
                	set_sec = time.Seconds;
                	set_day = time.Dayofmonth;
                	set_month = time.Month;
                	set_year = time.Year;
                    Display_Time_Setting();
                } else if(current_mode == MODE_SHOW_TZ)	{
                	Display_Timezone();
                }
            }
        }

    last_btn0 = btn0;
    last_btn1 = btn1;
    last_btn2 = btn2;
    last_btn3 = btn3;
    }



/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */
  ST7735_Init();
  //Set_time(0, 4, 11, 5, 9, 5, 25);
  ST7735_FillScreen(ST7735_BLACK);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
	  loop_poll_buttons();
	  	 	  static Mode last_displayed_mode = MODE_NORMAL;
	  	 	      if (last_displayed_mode != current_mode) {
	  	 	          ST7735_FillScreen(ST7735_BLACK);
	  	 	          last_displayed_mode = current_mode;
	  	 	      }

	  	 	      // Sửa các điều kiện mode thành current_mode
	  	 	      if (current_mode == MODE_NORMAL) {
	  	 	          uint32_t now = HAL_GetTick();
	  	 	          if (now - last_display_update >= 1000) {
	  	 	              Display_Time();
	  	 	              last_display_update = now;
	  	 	          }
	  	 	      }
	  	 	      else if (current_mode == MODE_SET_TIME) {
	  	 	    	 uint32_t now = HAL_GetTick();
	  	 	    	 if (now - last_display_update >= 500) {
	  	 	          Display_Time_Setting();
	  	 	         last_display_update = now;
	  	 	          }
	  	 	      }
	  	 	      else if (current_mode == MODE_SET_ALARM) {
	  	 	    	 uint32_t now = HAL_GetTick();
	  	 	    	 if (now - last_display_update >= 500) {
	  	 	          Display_Alarm_Setting();
	  	 	         last_display_update = now;
	  	 	          }
	  	 	      }
	  	 	      else if (current_mode == MODE_SHOW_TZ) {
	  	 	             Display_Timezone();
	  	 	      }

	  	 	  if (alarm_triggered) {
	  	 	 	          uint32_t now = HAL_GetTick();

	  	 	 	          if ((now - alarm_display_start) >= 10000) {
	  	 	 	                  alarm_triggered = 0;
	  	 	 	                  //Clear_DS3231_Alarm();
	  	 	 	                 ST7735_FillRectangle(10, 55, 128, 160, ST7735_BLACK);
	  	 	 	              }
	  	 	 	      }
    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_1LINE;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_10, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);

  /*Configure GPIO pins : PA1 PA2 PA3 PA6 */
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PA4 */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB1 PB10 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PA8 */
  GPIO_InitStruct.Pin = GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI4_IRQn);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
