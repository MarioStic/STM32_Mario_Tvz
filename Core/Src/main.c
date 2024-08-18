#include "main.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>

//i2c za lcd zaslon
I2C_HandleTypeDef hi2c1;

//generirano od stm32cubeide, deklaracije funkcija za inicijalizaciju
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);

/////////////////////////////////////////////////////////////////////////////////////
//LCD
#define LCD_ADRESA 0x4E //0x27//0x4E//0x3F

void lcd_posalji_komandu(char komanda) {
	char gornji_bajt, donji_bajt;
	uint8_t niz_podataka[4];

	// Odvajanje gornjih i donjih 4 bita komande
	gornji_bajt = (komanda & 0xF0);
	donji_bajt = ((komanda << 4) & 0xF0);

	// Priprema četiri podkomande koje će se slati
	niz_podataka[0] = gornji_bajt | 0x0C;  // en=1, rs=0
	niz_podataka[1] = gornji_bajt | 0x08;  // en=0, rs=0
	niz_podataka[2] = donji_bajt | 0x0C;   // en=1, rs=0
	niz_podataka[3] = donji_bajt | 0x08;   // en=0, rs=0

	// Slanje podkomandi putem I2C na LCD
	HAL_I2C_Master_Transmit(&hi2c1, LCD_ADRESA, (uint8_t*) niz_podataka, 4,
			100);
}

void lcd_posalji_podatak(char podatak) {
	char gornji_bajt, donji_bajt;
	uint8_t niz_podataka[4];

	// Odvajanje gornjih i donjih 4 bita podatka
	gornji_bajt = (podatak & 0xF0);
	donji_bajt = ((podatak << 4) & 0xF0);

	// Priprema četiri podkomande koje će se slati
	niz_podataka[0] = gornji_bajt | 0x0D;  // en=1, rs=0
	niz_podataka[1] = gornji_bajt | 0x09;  // en=0, rs=0
	niz_podataka[2] = donji_bajt | 0x0D;   // en=1, rs=0
	niz_podataka[3] = donji_bajt | 0x09;   // en=0, rs=0

	// Slanje podkomandi putem I2C na LCD
	HAL_I2C_Master_Transmit(&hi2c1, LCD_ADRESA, (uint8_t*) niz_podataka, 4,
			100);
}

void lcd_inicijaliziraj(void) {
	// 4-bit inicijalizacija
	HAL_Delay(50);  // čekaj >40ms
	lcd_posalji_komandu(0x30);
	HAL_Delay(5);   // čekaj >4.1ms
	lcd_posalji_komandu(0x30);
	HAL_Delay(1);   // čekaj >100us
	lcd_posalji_komandu(0x30);
	HAL_Delay(10);
	lcd_posalji_komandu(0x20);  // 4-bit mode
	HAL_Delay(10);

	// inicijalizacija ekrana
	lcd_posalji_komandu(0x28); // Funkcijski set --> DL=0 (4-bit mode), N = 1 (2-line display), F = 0 (5x8 characters)
	HAL_Delay(1);
	lcd_posalji_komandu(0x08); // Upravljanje prikazom --> D=0, C=0, B=0  ---> isključi prikaz
	HAL_Delay(1);
	lcd_posalji_komandu(0x01);  // Čišćenje ekrana
	HAL_Delay(1);
	HAL_Delay(1);
	lcd_posalji_komandu(0x06); // Postavljanje smjera unosa --> I/D = 1 (inkrementiranje kursora) & S = 0 (bez pomicanja)
	HAL_Delay(1);
	lcd_posalji_komandu(0x0C); // Upravljanje prikazom --> D = 1, C i B = 0. (Kursor i blinkanje, zadnja dva bita)

}

void lcd_prazan_zaslon(void) {
	// Poziv funkcije za slanje komande za čišćenje ekrana
	lcd_posalji_komandu(0x01);
	HAL_Delay(200);
}

void lcd_posalji_podatke(const char *format, ...) {
	va_list argumenti;
	va_start(argumenti, format);

	char niz_buffera[17]; // Prilagodite veličinu buffer-a prema potrebi
	vsnprintf(niz_buffera, sizeof(niz_buffera), format, argumenti);

	va_end(argumenti);

	char *pokazivac = niz_buffera; // Koristi pokazivač za navigaciju kroz niz

	while (*pokazivac) {
		lcd_posalji_podatak(*pokazivac++);
	}
}

void lcd_redak_stupac(int redak, int stupac) {
	switch (redak) {
	case 0:
		stupac |= 0x80; // Dodaj offset za prvi redak
		break;
	case 1:
		stupac |= 0xC0; // Dodaj offset za drugi redak
		break;
	}

	// Poziv funkcije za slanje komande postavljanja kursora
	lcd_posalji_komandu(stupac);
}
//LCD
/////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////
///LEDICE
//uključenje ledica, zeleno nadzornik, crveno servis
void zeleno_nadzornik_crveno_servis_ledice() {
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_RESET);
}

//uključenje ledica, crveno nadzornik, zeleno servis
void crveno_nadzornik_zeleno_servis_ledice() {
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_SET);
}

///LEDICE
/////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////
//SENZOR
//varijabla koja govori jel senzor upaljen ili nije
bool senzor_upaljen = false;
//varijabla koja govori jel trebam staviti na lcd da je kanal spreman
bool lcd_kanal_spreman = false;

//interrupt servisna funkcija koja se pozove kada se desi interrupt na senzoru
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	//provjeravamo jel se desio interrupt tocno na pinu na kojem je senzor
	if (GPIO_Pin == GPIO_PIN_15) {
		// Procitaj koje je stanje na pinu, s njime cemo vidjeti jel senzor upaljen ili nije
		GPIO_PinState pinState = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_15);
		//input pin za senzor je pullup sa otpornikom, znaci da mu je defaultno stanje 1,
		//sto znaci da ako ocitava nesto onda ce biti u GND , tj 0 i tada je upaljen ustvari
		if (pinState == GPIO_PIN_SET) {
			// Senzor iskljucen
			senzor_upaljen = false;
			zeleno_nadzornik_crveno_servis_ledice();
		} else {
			// Senzor ukljucen
			senzor_upaljen = true;
			crveno_nadzornik_zeleno_servis_ledice();
		}
	}
}
//SENZOR
////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////
//MATRIX
//definiram koliko ima redaka i stupaca
#define RETCI 4
#define STUPCI 3

//definiranje znakova u retcima i stupcima
const char znakovi[RETCI][STUPCI] = { { '1', '2', '3' }, { '4', '5', '6' }, {
		'7', '8', '9' }, { '*', '0', '#' } };

//definiranje pinova za retke i stupce
GPIO_TypeDef *retci_portovi[RETCI] = { GPIOA, GPIOA, GPIOC, GPIOB };
uint16_t retci_pinovi[RETCI] =
		{ GPIO_PIN_6, GPIO_PIN_7, GPIO_PIN_7, GPIO_PIN_10 };

GPIO_TypeDef *stupci_portovi[STUPCI] = { GPIOA, GPIOA, GPIOB };
uint16_t stupci_pinovi[STUPCI] = { GPIO_PIN_9, GPIO_PIN_8, GPIO_PIN_4 };

//funkcija pomocu koje citam koji je znak pritisnut, redak po redak spustam u nulu i onda pomocu stupaca pinova citam koji je pin upaljen, tj koja
// je tipka pristisnuta
char procitaj_znak(void) {
	for (int redak = 0; redak < RETCI; redak++) {
		// stavi trenutni redak u 0
		HAL_GPIO_WritePin(retci_portovi[redak], retci_pinovi[redak],
				GPIO_PIN_RESET);

		for (int stupac = 0; stupac < STUPCI; stupac++) {
			// pogledaj jel stupac pin u 0
			if (HAL_GPIO_ReadPin(stupci_portovi[stupac], stupci_pinovi[stupac])
					== GPIO_PIN_RESET) {
				// mali delay za debounce
				HAL_Delay(20);
				while (HAL_GPIO_ReadPin(stupci_portovi[stupac],
						stupci_pinovi[stupac]) == GPIO_PIN_RESET)
					;
				HAL_Delay(20);

				// vrati redak u 1 prije nego izadem iz funkcije
				HAL_GPIO_WritePin(retci_portovi[redak], retci_pinovi[redak],
						GPIO_PIN_SET);
				return znakovi[redak][stupac];
			}
		}

		// vrati redak u 1 prije nego izadem iz funkcije
		HAL_GPIO_WritePin(retci_portovi[redak], retci_pinovi[redak],
				GPIO_PIN_SET);
	}

	return 0;
}

//MATRIX
////////////////////////////////////////////////////////////////////////////////////

//glavna funckija, od tuda sve pocinje
int main(void) {
	//inicijalizacija raznih stvari
	HAL_Init();
	SystemClock_Config();
	MX_GPIO_Init();
	MX_I2C1_Init();

	//inicijalizacija lcd-a
	lcd_inicijaliziraj();

	//stavljam ledice u pocetno stanje
	zeleno_nadzornik_crveno_servis_ledice();

	//polje u kojem spremam znakove sa keypada
	char matrix_podatci[4] = { };
	matrix_podatci[3] = '\0';
	//brojim koliko je znakova pritisnuto
	int brojac_znakova = 0;

	//petlja koja se cijelo vrijeme vrti
	while (1) {
		//ako je senzor upaljen
		if (senzor_upaljen) {
			//stavljam varijablu u false da kada se senzor ugasi mogu na lcd kasnije
			//ponovno staviti da je kanal prazan
			lcd_kanal_spreman = false;
			//citam jel pritisnut neki znak na keypadu
			char znak = procitaj_znak();
			//ako dobijem razlicito od 0 onda je pritisnuta tipka
			if (znak != 0) {
				//ako je prisnuta tipka '*', resetiram brojac znakova, da ponovo mogu napisati ispravan vlak
				if (znak == '*') {
					brojac_znakova = 0;
					//ako nije '#', tj enter i je broj znakova < 3(znaci da nije u potpunost kanal napisan)
					//spremam znak u polje i povecavam brojac za 1
				} else if (znak != '#' && brojac_znakova < 3) {
					matrix_podatci[brojac_znakova] = znak;
					brojac_znakova++;
					//ako je znak '#', tj enter i brojac znakova je 3 sto znaci da imam cijeli broj od kanala,
					//onda brisem zaslon i stavljam na lcd koji kanal je spreman i palim zelenu nazdornik
					//ledicu i crvenu servis ledicu
				} else if (znak == '#' && brojac_znakova == 3) {
					brojac_znakova = 0;
					lcd_prazan_zaslon();
					lcd_posalji_podatke("   Kanal %s", matrix_podatci);
					lcd_redak_stupac(1, 0);
					lcd_posalji_podatke("     spreman");
					zeleno_nadzornik_crveno_servis_ledice();
				}
			}
			//ako senzor nije upaljen
		} else {
			//ako na lcd-u ne pise da nema spremnog kanala, mogu ici napisati
			//da nema spremnog kanala
			if (!lcd_kanal_spreman) {
				//stavljam na lcd-u da nema spremnog kanala
				lcd_kanal_spreman = true;
				lcd_prazan_zaslon();
				lcd_posalji_podatke(" Nema spremnog");
				lcd_redak_stupac(1, 0);
				lcd_posalji_podatke("     kanala");
			}
		}

		HAL_Delay(100);
	}
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

	/** Configure the main internal regulator output voltage
	 */
	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
	RCC_OscInitStruct.PLL.PLLM = 16;
	RCC_OscInitStruct.PLL.PLLN = 336;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
	RCC_OscInitStruct.PLL.PLLQ = 2;
	RCC_OscInitStruct.PLL.PLLR = 2;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief I2C1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C1_Init(void) {

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
	if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN I2C1_Init 2 */

	/* USER CODE END I2C1_Init 2 */

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	/* USER CODE BEGIN MX_GPIO_Init_1 */
	/* USER CODE END MX_GPIO_Init_1 */

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOH_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOA, LD2_Pin | GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1 | GPIO_PIN_2, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);

	/*Configure GPIO pins : USART_TX_Pin USART_RX_Pin */
	GPIO_InitStruct.Pin = USART_TX_Pin | USART_RX_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pins : LD2_Pin PA6 PA7 */
	GPIO_InitStruct.Pin = LD2_Pin | GPIO_PIN_6 | GPIO_PIN_7;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pins : PB1 PB2 PB10 */
	GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_10;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/*Configure GPIO pins : PB4 */
	GPIO_InitStruct.Pin = GPIO_PIN_4;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/*Configure GPIO pin : PB15 */
	GPIO_InitStruct.Pin = GPIO_PIN_15;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/*Configure GPIO pin : PC7 */
	GPIO_InitStruct.Pin = GPIO_PIN_7;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/*Configure GPIO pins : PA8 PA9 */
	GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/* EXTI interrupt init*/
	HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

	/* USER CODE BEGIN MX_GPIO_Init_2 */
	/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1) {
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
